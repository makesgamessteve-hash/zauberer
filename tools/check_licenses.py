# SPDX-License-Identifier: MIT
"""License metadata and license split check.

A file's license comes from (in order of precedence)
  1. an 'SPDX-License-Identifier:' line in its first 5 lines, or in a
     '<file>.license' sidecar (REUSE convention for binary files),
  2. REUSE.toml at the repository root (REUSE 3 format): the matching
     [[annotations]] entry, where - as in the REUSE specification and the
     reuse tool - the LAST matching entry wins. An entry with
     precedence = "override" takes priority over the file's own header.
     Globs: '*' matches within one path segment, '**' across segments.

Checked files: 'git ls-files --cached --others --exclude-standard' when the
root is a git repository, otherwise a directory walk. Skipped: .git/, build
directories, LICENSES/, LICENSE*/COPYING* files, '*.license' sidecars and
REUSE.toml.

The check fails (exit 1) for files without license information and for
violations of the project license split:
  * media under assets/ must be CC-BY-4.0, or the license the package's
    credits.lua records for it (third-party assets such as CC0-1.0),
  * src/, tools/, tests/, cmake/, web/, assets/*/scripts/, assets/*/shaders/
    and CMake files must be MIT,
  * third_party/ is exempt from the split but still needs metadata.
Exit code 2 means a usage or I/O error (for example an invalid REUSE.toml).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path

import check_credits
import lua_literal
from tool_common import (EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError,
                         glob_to_regex, is_skipped_dir, rel_posix, walk_files)

HEADER_LINES = 5
PROJECT_ASSET_LICENSE = "CC-BY-4.0"
CODE_LICENSE = "MIT"
MEDIA_EXTENSIONS = check_credits.ASSET_EXTENSIONS
MIT_GLOBS = ("src/**", "tools/**", "tests/**", "cmake/**", "web/**",
             "assets/*/scripts/**", "assets/*/shaders/**")
CMAKE_NAMES = frozenset({"CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json"})
EXEMPT_PREFIX = "third_party/"
PRECEDENCES = ("closest", "aggregate", "override")

_SPDX_RE = re.compile(r"SPDX-License-Identifier:[ \t]*(.*)")
_SPDX_VALUE_RE = re.compile(r"[A-Za-z0-9.+:()\- \t]*")
_IGNORED_FILE_RE = re.compile(r"^(LICEN[CS]E|COPYING)([-.].*)?$|.*\.license$|^REUSE\.toml$")
_IGNORED_DIRS = frozenset({"LICENSES", ".reuse"})


@dataclass(frozen=True)
class Annotation:
    patterns: tuple[str, ...]
    license: str | None
    precedence: str

    def matches(self, path: str) -> bool:
        return any(glob_to_regex(p).fullmatch(path) for p in self.patterns)


@dataclass(frozen=True)
class LicenseInfo:
    license: str
    source: str  # "SPDX header", "sidecar" or "REUSE.toml"


def normalize_expression(expression: str) -> str:
    return " ".join(expression.split())


def spdx_from_text(text: str) -> str | None:
    """The first SPDX-License-Identifier value in text, without comment
    terminators such as '*/' or '-->'."""
    for match in _SPDX_RE.finditer(text):
        value = re.split(r"\*/|-->|\]\]", match.group(1))[0]
        value = normalize_expression(_SPDX_VALUE_RE.match(value).group(0))
        if value:
            return value
    return None


def read_header_license(path: Path) -> str | None:
    """SPDX identifier from the first HEADER_LINES lines of a file."""
    try:
        with path.open("rb") as stream:
            head = b"".join(stream.readline(4096) for _ in range(HEADER_LINES))
    except OSError as error:
        raise ToolError(f"cannot read {path}: {error}") from error
    return spdx_from_text(head.decode("utf-8", errors="replace"))


def load_reuse_toml(root: Path) -> list[Annotation]:
    path = root / "REUSE.toml"
    if not path.is_file():
        return []
    try:
        with path.open("rb") as stream:
            data = tomllib.load(stream)
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise ToolError(f"cannot read {path}: {error}") from error
    annotations = data.get("annotations", [])
    if not isinstance(annotations, list):
        raise ToolError(f"{path}: 'annotations' must be an array of tables ([[annotations]])")
    result = []
    for number, item in enumerate(annotations, start=1):
        where = f"{path}: annotation #{number}"
        if not isinstance(item, dict):
            raise ToolError(f"{where} is not a table")
        patterns = item.get("path")
        if isinstance(patterns, str):
            patterns = [patterns]
        if not isinstance(patterns, list) or not patterns or not all(isinstance(p, str) for p in patterns):
            raise ToolError(f"{where} needs 'path' as a string or a list of strings")
        license_id = item.get("SPDX-License-Identifier")
        if isinstance(license_id, list) and all(isinstance(x, str) for x in license_id):
            license_id = " AND ".join(license_id) if license_id else None
        if license_id is not None and not isinstance(license_id, str):
            raise ToolError(f"{where}: 'SPDX-License-Identifier' must be a string or a list of strings")
        precedence = item.get("precedence", "closest")
        if precedence not in PRECEDENCES:
            raise ToolError(f"{where}: 'precedence' must be one of {', '.join(PRECEDENCES)}")
        license_id = normalize_expression(license_id) if license_id else None
        result.append(Annotation(tuple(patterns), license_id or None, precedence))
    return result


def find_annotation(annotations: list[Annotation], rel: str) -> Annotation | None:
    """The last annotation matching rel (REUSE semantics)."""
    for annotation in reversed(annotations):
        if annotation.matches(rel):
            return annotation
    return None


def license_of(root: Path, rel: str, annotations: list[Annotation]) -> LicenseInfo | None:
    annotation = find_annotation(annotations, rel)
    if annotation and annotation.precedence == "override" and annotation.license:
        return LicenseInfo(annotation.license, "REUSE.toml")
    sidecar = root / (rel + ".license")
    if sidecar.is_file():
        try:
            header = spdx_from_text(sidecar.read_text(encoding="utf-8", errors="replace"))
        except OSError as error:
            raise ToolError(f"cannot read {sidecar}: {error}") from error
        if header:
            return LicenseInfo(header, "sidecar")
    else:
        header = read_header_license(root / rel)
        if header:
            return LicenseInfo(header, "SPDX header")
    if annotation and annotation.license:
        return LicenseInfo(annotation.license, "REUSE.toml")
    return None


class CreditsLookup:
    """Licenses recorded in the packages' credits.lua files, by repository path."""

    def __init__(self, root: Path):
        self.root = root
        self.cache: dict[Path, dict[str, str]] = {}

    def _package_of(self, rel: str) -> Path | None:
        path = (self.root / rel).parent
        assets = self.root / "assets"
        while path != assets and assets in path.parents:
            if (path / check_credits.PACKAGE_MANIFEST).is_file():
                return path
            path = path.parent
        return None

    def license_for(self, rel: str) -> tuple[str, str] | None:
        """(license, credits path) recorded for rel, or None."""
        package = self._package_of(rel)
        if package is None:
            return None
        if package not in self.cache:
            licenses: dict[str, str] = {}
            try:
                for entry in check_credits.load_credits(package):
                    if (isinstance(entry, dict) and isinstance(entry.get("file"), str)
                            and isinstance(entry.get("license"), str) and entry["license"].strip()):
                        licenses[entry["file"].strip()] = normalize_expression(entry["license"])
            except (OSError, lua_literal.LuaSyntaxError, ValueError):
                pass  # reported by check_credits
            self.cache[package] = licenses
        name = (self.root / rel).relative_to(package).as_posix()
        recorded = self.cache[package].get(name)
        if recorded is None:
            return None
        return recorded, rel_posix(package / check_credits.CREDITS_FILE, self.root)


def is_cmake_file(rel: str) -> bool:
    name = rel.rsplit("/", 1)[-1]
    return name in CMAKE_NAMES or name.endswith(".cmake")


def expected_license(rel: str, credits: CreditsLookup) -> tuple[str, str] | None:
    """(expected license, reason) for files covered by the license split."""
    if rel.startswith(EXEMPT_PREFIX):
        return None
    if rel.startswith("assets/") and os.path.splitext(rel)[1].lower() in MEDIA_EXTENSIONS:
        recorded = credits.license_for(rel)
        if recorded and recorded[0] != PROJECT_ASSET_LICENSE:
            return recorded[0], f"as credited in {recorded[1]}"
        return PROJECT_ASSET_LICENSE, "project media"
    if is_cmake_file(rel) or any(glob_to_regex(g).fullmatch(rel) for g in MIT_GLOBS):
        return CODE_LICENSE, "project code"
    return None


def _ignored(rel: str) -> bool:
    parts = rel.split("/")
    directories = parts[:-1]
    if directories and directories[0] == "out":
        return True
    if any(is_skipped_dir(part) or part in _IGNORED_DIRS for part in directories):
        return True
    return bool(_IGNORED_FILE_RE.match(parts[-1]))


def is_git_toplevel(root: Path) -> bool:
    """True if root is the top level of a git work tree (and git works)."""
    if not (root / ".git").exists():
        return False
    try:
        result = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=root,
                                capture_output=True, text=True)
    except OSError:
        return False
    return (result.returncode == 0
            and os.path.samefile(result.stdout.strip() or os.sep, os.path.abspath(root)))


def list_files(root: Path, use_git: bool = True) -> list[str]:
    """Repository-relative paths of the files to check: git's file list
    (tracked plus untracked, not ignored) for a git work tree, else a walk."""
    if use_git and is_git_toplevel(root):
        try:
            output = subprocess.run(
                ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                cwd=root, check=True, capture_output=True).stdout
        except (OSError, subprocess.CalledProcessError) as error:
            raise ToolError(f"git ls-files failed in {root}: {error}") from error
        names = [name for name in output.decode("utf-8", errors="surrogateescape").split("\0") if name]
    else:
        names = [rel_posix(path, root) for path in walk_files(root)]
    result = []
    for rel in sorted(set(names)):
        path = root / rel
        if _ignored(rel) or path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            continue  # like the reuse tool, empty files need no license
        result.append(rel)
    return result


def check(root: Path, use_git: bool = True) -> tuple[list[str], int]:
    """Returns (problems, number of checked files)."""
    annotations = load_reuse_toml(root)
    credits = CreditsLookup(root)
    problems = []
    files = list_files(root, use_git)
    for rel in files:
        info = license_of(root, rel, annotations)
        if info is None:
            problems.append(f"{rel}: no license information (add an SPDX-License-Identifier "
                            "header or a REUSE.toml annotation)")
            continue
        expected = expected_license(rel, credits)
        if expected and normalize_expression(info.license) != expected[0]:
            problems.append(f"{rel}: declared {info.license} ({info.source}), "
                            f"expected {expected[0]} ({expected[1]})")
    return problems, len(files)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check that every file has license metadata and follows the MIT / CC-BY-4.0 split.")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root (default: %(default)s)")
    parser.add_argument("--no-git", action="store_true",
                        help="walk the directory tree instead of asking git for the file list")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    try:
        if not args.root.is_dir():
            raise ToolError(f"root {args.root} is not a directory")
        problems, count = check(args.root, use_git=not args.no_git)
    except ToolError as error:
        print(f"check_licenses: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    for problem in problems:
        print(problem)
    if problems:
        print(f"check_licenses: {len(problems)} problem(s) in {count} files")
        return EXIT_FAIL
    print(f"check_licenses: {count} files checked, all licensed correctly")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
