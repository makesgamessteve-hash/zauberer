# SPDX-License-Identifier: MIT
"""Helpers shared by the repository check tools in tools/.

Standard library only. Paths that the tools report are POSIX-style and
relative to the repository root.
"""
from __future__ import annotations

import functools
import os
import re
from pathlib import Path
from typing import Iterator

# The repository root: the parent of the tools/ directory.
REPO_ROOT = Path(__file__).resolve().parent.parent

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_ERROR = 2

_BUILD_DIR_RE = re.compile(r"^(build|cmake-build)(-.*)?$")
SKIPPED_DIR_NAMES = frozenset({".git", ".hg", "__pycache__", "node_modules"})


class ToolError(Exception):
    """A usage or I/O problem. The tools report it and exit with code 2."""


def is_skipped_dir(name: str) -> bool:
    """True for VCS, cache and build directory names."""
    return name in SKIPPED_DIR_NAMES or bool(_BUILD_DIR_RE.match(name))


def walk_files(base: Path) -> Iterator[Path]:
    """Yields every regular file below base in a deterministic order,
    skipping VCS, cache and build directories."""
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = sorted(d for d in dirnames if not is_skipped_dir(d))
        for name in sorted(filenames):
            path = Path(dirpath) / name
            if path.is_file():
                yield path


def rel_posix(path: Path | str, root: Path | str) -> str:
    """path relative to root with '/' separators, or path itself (POSIX
    style) when it is not below root."""
    abs_path = Path(os.path.abspath(path))
    try:
        return abs_path.relative_to(os.path.abspath(root)).as_posix()
    except ValueError:
        return Path(path).as_posix()


@functools.lru_cache(maxsize=None)
def glob_to_regex(pattern: str) -> re.Pattern[str]:
    """Compiles a glob with REUSE.toml semantics: '*' matches within one
    path segment, '**' matches across segments (including '/'), and a
    backslash makes the next character literal. Everything else, including
    '?', is literal."""
    out = []
    i = 0
    n = len(pattern)
    while i < n:
        char = pattern[i]
        if char == "\\" and i + 1 < n:
            out.append(re.escape(pattern[i + 1]))
            i += 2
        elif char == "*":
            if pattern.startswith("**", i):
                out.append(".*")
                i += 2
                while i < n and pattern[i] == "*":
                    i += 1
            else:
                out.append("[^/]*")
                i += 1
        else:
            out.append(re.escape(char))
            i += 1
    return re.compile("".join(out), re.DOTALL)


def glob_match(pattern: str, path: str) -> bool:
    """True if the whole POSIX path matches the glob (see glob_to_regex)."""
    return glob_to_regex(pattern).fullmatch(path) is not None


def read_text(path: Path) -> str:
    """Reads a UTF-8 text file, raising ToolError on I/O problems."""
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise ToolError(f"cannot read {path}: {error}") from error
