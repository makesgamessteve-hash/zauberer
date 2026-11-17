# SPDX-License-Identifier: MIT
"""Credits check for asset packages.

Every package (a directory with a package.lua below assets/ or mods/) must
have a credits.lua that returns a list of entries

    { file = "sprites/mote.png", author = "...", source = "...",
      license = "CC-BY-4.0", exception = "optional rationale" }

The check fails (exit 1) when
  * an asset file of the package has no entry,
  * an entry points to a file that does not exist,
  * an entry's license is not on the allowlist (CC0-1.0, CC-BY-4.0) and the
    entry records no exception rationale,
  * author, source or license is empty, or credits.lua is missing/invalid.
Exit code 2 means a usage or I/O error.
"""
from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

import lua_literal
from tool_common import EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError, rel_posix

ASSET_EXTENSIONS = frozenset({".png", ".jpg", ".jpeg", ".glb", ".gltf", ".bin", ".vox",
                              ".wav", ".ogg", ".mp3", ".ttf", ".otf"})
LICENSE_ALLOWLIST = ("CC0-1.0", "CC-BY-4.0")
REQUIRED_FIELDS = ("author", "source", "license")
PACKAGE_MANIFEST = "package.lua"
CREDITS_FILE = "credits.lua"
PACKAGE_BASES = ("assets", "mods")


@dataclass
class PackageReport:
    path: str
    problems: list[str] = field(default_factory=list)
    credited_assets: int = 0


def is_asset(name: str) -> bool:
    return os.path.splitext(name)[1].lower() in ASSET_EXTENSIONS


def _visible(name: str) -> bool:
    return not name.startswith(".") and name != "__pycache__"


def find_packages(root: Path) -> list[Path]:
    """Every package directory below <root>/assets and <root>/mods."""
    packages = []
    for base in PACKAGE_BASES:
        if not (root / base).is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(root / base):
            dirnames[:] = sorted(d for d in dirnames if _visible(d))
            if PACKAGE_MANIFEST in filenames:
                packages.append(Path(dirpath))
    return sorted(packages)


def asset_files(package: Path) -> list[str]:
    """Asset files of a package, relative to it, excluding nested packages."""
    result = []
    for dirpath, dirnames, filenames in os.walk(package):
        dirnames[:] = sorted(d for d in dirnames if _visible(d)
                             and not (Path(dirpath) / d / PACKAGE_MANIFEST).is_file())
        for name in filenames:
            if _visible(name) and is_asset(name):
                result.append((Path(dirpath) / name).relative_to(package).as_posix())
    return sorted(result)


def load_credits(package: Path) -> list:
    """The parsed entries of a package's credits.lua. Raises OSError,
    lua_literal.LuaSyntaxError or ValueError (wrong structure)."""
    data = lua_literal.parse_file(package / CREDITS_FILE)
    if data == {}:
        data = []
    if not isinstance(data, list):
        raise ValueError("credits.lua must return a list of entries")
    return data


def _nonempty_string(value) -> bool:
    return isinstance(value, str) and value.strip() != ""


def _valid_relative(path: str) -> bool:
    parts = path.split("/")
    return not path.startswith("/") and "\\" not in path and ".." not in parts and "" not in parts


def check_package(package: Path, root: Path) -> PackageReport:
    shown_package = rel_posix(package, root)
    report = PackageReport(shown_package)
    credits_shown = f"{shown_package}/{CREDITS_FILE}"
    assets = asset_files(package)

    if not (package / CREDITS_FILE).is_file():
        report.problems.append(f"{credits_shown}: missing credits file "
                               f"({len(assets)} asset file(s) need entries)")
        return report
    try:
        entries = load_credits(package)
    except OSError as error:
        raise ToolError(f"cannot read {credits_shown}: {error}") from error
    except (lua_literal.LuaSyntaxError, ValueError) as error:
        report.problems.append(f"{credits_shown}: cannot parse credits: {error}")
        return report

    credited: set[str] = set()
    for number, entry in enumerate(entries, start=1):
        where = f"{credits_shown}: entry #{number}"
        if entry == []:
            entry = {}
        if not isinstance(entry, dict):
            report.problems.append(f"{where} is not a table")
            continue
        name = entry.get("file")
        if not _nonempty_string(name):
            report.problems.append(f"{where} has no 'file'")
            continue
        name = name.strip()
        asset = f"{shown_package}/{name}"
        if not _valid_relative(name):
            report.problems.append(f"{where}: file '{name}' must be a path relative to the package")
            continue
        if name in credited:
            report.problems.append(f"{asset}: credited more than once in {credits_shown}")
        credited.add(name)
        if not (package / name).is_file():
            report.problems.append(f"{asset}: credits entry points to a missing file ({credits_shown})")
        for key in REQUIRED_FIELDS:
            if not _nonempty_string(entry.get(key)):
                report.problems.append(f"{asset}: credits entry has an empty or missing '{key}'")
        license_id = entry.get("license")
        if (_nonempty_string(license_id) and license_id.strip() not in LICENSE_ALLOWLIST
                and not _nonempty_string(entry.get("exception"))):
            report.problems.append(
                f"{asset}: license '{license_id.strip()}' is not on the allowlist "
                f"({', '.join(LICENSE_ALLOWLIST)}) and no exception rationale is recorded")

    for name in assets:
        if name in credited:
            report.credited_assets += 1
        else:
            report.problems.append(f"{shown_package}/{name}: asset file has no credits entry "
                                   f"in {credits_shown}")
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check that every asset of every package is credited with an allowed license.")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root (default: %(default)s)")
    parser.add_argument("--package", type=Path, action="append", metavar="DIR",
                        help="check only this package directory (repeatable)")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    root = args.root
    try:
        if args.package:
            packages = []
            for package in args.package:
                if not package.is_dir() and not package.is_absolute() and (root / package).is_dir():
                    package = root / package
                if not package.is_dir():
                    raise ToolError(f"package directory {package} does not exist")
                packages.append(package)
        else:
            if not any((root / base).is_dir() for base in PACKAGE_BASES):
                raise ToolError(f"neither assets/ nor mods/ exists below {root}")
            packages = find_packages(root)
        reports = [check_package(package, root) for package in packages]
    except ToolError as error:
        print(f"check_credits: error: {error}", file=sys.stderr)
        return EXIT_ERROR

    problems = [problem for report in reports for problem in report.problems]
    for report in reports:
        status = "OK" if not report.problems else f"{len(report.problems)} problem(s)"
        print(f"{report.path}: {report.credited_assets} credited asset(s), {status}")
    for problem in problems:
        print(f"  {problem}")
    if problems:
        print(f"check_credits: {len(problems)} problem(s) in {len(reports)} package(s)")
        return EXIT_FAIL
    print(f"check_credits: {len(reports)} package(s) OK")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
