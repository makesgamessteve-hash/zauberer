# SPDX-License-Identifier: MIT
"""Module dependency check for src/.

A module is the first directory below src/. A project include
'#include "<module>/..."' is only allowed if the including module may depend
on that module (see ALLOWED). Only platform may include SDL and Emscripten
headers, and only script may include the Lua C API headers.

Reports 'file:line: module X must not include Y' and exits 1 on violations,
0 when clean and 2 on usage or I/O errors.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tool_common import (EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError,
                         rel_posix, walk_files)

MODULES = ("core", "platform", "vfs", "input", "render", "audio", "world", "game", "net", "script", "app")

ALLOWED: dict[str, frozenset[str]] = {
    "core": frozenset({"core"}),
    "platform": frozenset({"core", "platform"}),
    "vfs": frozenset({"core", "vfs"}),
    "input": frozenset({"core", "input"}),
    "render": frozenset({"core", "vfs", "render"}),
    "audio": frozenset({"core", "vfs", "audio"}),
    "world": frozenset({"core", "world"}),
    "game": frozenset({"core", "world", "input", "game"}),
    "net": frozenset({"core", "net"}),
    "script": frozenset({"core", "vfs", "input", "render", "audio", "world", "game", "net", "script"}),
    "app": frozenset(MODULES),
}

PLATFORM_ONLY_MODULE = "platform"
LUA_MODULE = "script"
LUA_HEADERS = frozenset({"lua.h", "lauxlib.h", "lualib.h", "luaconf.h", "lua.hpp"})
SOURCE_EXTENSIONS = frozenset({".c", ".h", ".inc", ".m"})

_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^<>"\n]*)[>"]', re.MULTILINE)
_SDL_RE = re.compile(r"^SDL\d*(/|\.h$|_)")
# Comments are blanked (keeping newlines); string and character literals are
# kept so that comment markers inside them are not mistaken for comments.
_COMMENT_OR_LITERAL_RE = re.compile(
    r'//[^\n]*|/\*.*?(?:\*/|\Z)|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


@dataclass(frozen=True, order=True)
class Violation:
    path: str
    line: int
    message: str

    def __str__(self) -> str:
        return f"{self.path}:{self.line}: {self.message}"


def strip_comments(text: str) -> str:
    """Replaces C comments with spaces, keeping line breaks and offsets."""
    def blank(match: re.Match[str]) -> str:
        token = match.group(0)
        if token.startswith(("//", "/*")):
            return re.sub(r"[^\n]", " ", token)
        return token
    return _COMMENT_OR_LITERAL_RE.sub(blank, text)


def check_text(module: str, text: str, path: str = "<text>") -> list[Violation]:
    """Checks the includes of one source file that belongs to module."""
    if module not in ALLOWED:
        raise ValueError(f"unknown module '{module}'")
    allowed = ALLOWED[module]
    violations = []
    code = strip_comments(text)
    for match in _INCLUDE_RE.finditer(code):
        delimiter, header = match.group(1), match.group(2).strip()
        shown = f'"{header}"' if delimiter == '"' else f"<{header}>"
        line = code.count("\n", 0, match.start()) + 1
        reason = None
        if _SDL_RE.match(header) or header.startswith("emscripten"):
            if module != PLATFORM_ONLY_MODULE:
                reason = "only platform may use SDL and Emscripten headers"
        elif header.rsplit("/", 1)[-1] in LUA_HEADERS:
            if module != LUA_MODULE:
                reason = "only script may use the Lua C API"
        elif "/" in header:
            target = header.split("/", 1)[0]
            if target in ALLOWED and target not in allowed:
                reason = f"{module} may depend on {', '.join(sorted(allowed))}"
        if reason:
            violations.append(Violation(path, line, f"module {module} must not include {shown} ({reason})"))
    return violations


def module_of(rel_to_src: str) -> str | None:
    """The module name of a path relative to src/, or None for files that
    sit directly in src/."""
    parts = rel_to_src.split("/")
    return parts[0] if len(parts) > 1 else None


def check_tree(src: Path, root: Path) -> tuple[list[Violation], int]:
    """Checks every C source and header below src. Returns the violations and
    the number of files checked."""
    if not src.is_dir():
        raise ToolError(f"source directory {src} does not exist")
    violations: list[Violation] = []
    count = 0
    for path in walk_files(src):
        if path.suffix not in SOURCE_EXTENSIONS:
            continue
        count += 1
        shown = rel_posix(path, root)
        module = module_of(rel_posix(path, src))
        if module is None:
            violations.append(Violation(shown, 1, "source file outside a module directory"))
            continue
        if module not in ALLOWED:
            violations.append(Violation(
                shown, 1, f"unknown module '{module}' (add it to ALLOWED in tools/check_includes.py)"))
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            raise ToolError(f"cannot read {path}: {error}") from error
        violations.extend(check_text(module, text, shown))
    return sorted(violations), count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check module include rules in src/.")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root (default: %(default)s)")
    parser.add_argument("--src", type=Path,
                        help="source directory to check (default: <root>/src)")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    src = args.src if args.src is not None else args.root / "src"
    try:
        violations, count = check_tree(src, args.root)
    except ToolError as error:
        print(f"check_includes: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    for violation in violations:
        print(violation)
    if violations:
        print(f"check_includes: {len(violations)} violation(s) in {count} files")
        return EXIT_FAIL
    print(f"check_includes: {count} files checked, no violations")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
