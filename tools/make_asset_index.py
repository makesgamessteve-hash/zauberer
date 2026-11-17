# SPDX-License-Identifier: MIT
"""Writes the asset index used by the web build.

Each line of the index is

    <path>\t<size>\t<sha256>\t<boot>

where path is relative to the assets directory (with '/'), size is in bytes,
sha256 is the lowercase hex digest and boot is 1 for files the web runtime
prefetches before main.lua runs (see BOOT_PATTERNS), else 0. The first line
is the header '# zauberer asset index v1'; entries are sorted by path, so the
output is deterministic. Dotfiles, dot directories and the index itself are
skipped.

Exit codes: 0 written, 2 usage or I/O error.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from tool_common import EXIT_ERROR, EXIT_OK, REPO_ROOT, ToolError, glob_match

HEADER = "# zauberer asset index v1"
INDEX_NAME = "index.tsv"
BOOT_PATTERNS = ("*/package.lua", "*/credits.lua", "*/scripts/**", "*/data/**",
                 "*/locale/**", "*/shaders/**", "*/fonts/*.lua", "*/sprites/*.lua")


@dataclass(frozen=True)
class Entry:
    path: str
    size: int
    sha256: str
    boot: bool

    def line(self) -> str:
        return f"{self.path}\t{self.size}\t{self.sha256}\t{1 if self.boot else 0}"


def is_boot(path: str) -> bool:
    return any(glob_match(pattern, path) for pattern in BOOT_PATTERNS)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 16), b""):
            digest.update(block)
    return digest.hexdigest()


def build_index(assets: Path, out: Path | None = None) -> list[Entry]:
    """Index entries for every file below assets, sorted by path."""
    if not assets.is_dir():
        raise ToolError(f"assets directory {assets} does not exist")
    skip = {os.path.abspath(assets / INDEX_NAME)}
    if out is not None:
        skip.add(os.path.abspath(out))
    entries = []
    try:
        for dirpath, dirnames, filenames in os.walk(assets):
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for name in filenames:
                path = Path(dirpath) / name
                if name.startswith(".") or os.path.abspath(path) in skip or not path.is_file():
                    continue
                rel = path.relative_to(assets).as_posix()
                if any(c in rel for c in "\t\r\n"):
                    raise ToolError(f"asset path {rel!r} contains a tab or line break")
                entries.append(Entry(rel, path.stat().st_size, _sha256(path), is_boot(rel)))
    except OSError as error:
        raise ToolError(f"cannot read assets: {error}") from error
    return sorted(entries, key=lambda e: e.path)


def render_index(entries: list[Entry]) -> str:
    return "\n".join([HEADER] + [entry.line() for entry in entries]) + "\n"


def write_index(out: Path, text: str) -> None:
    """Writes text to out atomically (via a temporary file in the same directory)."""
    try:
        out.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp = tempfile.mkstemp(prefix=".index-", dir=out.parent)
        try:
            with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
                stream.write(text)
            os.chmod(tmp, 0o644)  # mkstemp creates 0600; the index is served to browsers
            os.replace(tmp, out)
        except BaseException:
            os.unlink(tmp)
            raise
    except OSError as error:
        raise ToolError(f"cannot write {out}: {error}") from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Write the web asset index (path, size, sha256, boot).")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root (default: %(default)s)")
    parser.add_argument("--assets", type=Path,
                        help="assets directory (default: <root>/assets)")
    parser.add_argument("--out", type=Path,
                        help=f"index file to write (default: <assets>/{INDEX_NAME})")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    assets = args.assets if args.assets is not None else args.root / "assets"
    out = args.out if args.out is not None else assets / INDEX_NAME
    try:
        entries = build_index(assets, out)
        write_index(out, render_index(entries))
    except ToolError as error:
        print(f"make_asset_index: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    boot = sum(1 for entry in entries if entry.boot)
    print(f"make_asset_index: wrote {len(entries)} entries ({boot} boot) to {out}")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
