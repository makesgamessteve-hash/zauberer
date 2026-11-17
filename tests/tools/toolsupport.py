# SPDX-License-Identifier: MIT
"""Shared helpers for the tests of the tools in tools/: puts tools/ on
sys.path, builds small fake repositories and runs a tool's main()."""
from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))


def run_main(main, *args) -> tuple[int, str]:
    """Runs main(argv) and returns (exit code, combined stdout/stderr)."""
    output = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        code = main([str(arg) for arg in args])
    return code, output.getvalue()


def write_tree(root: Path, files: dict[str, str | bytes]) -> None:
    """Creates files below root; str content is written as UTF-8."""
    for rel, content in files.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8")


class TempDirTestCase(unittest.TestCase):
    """A test case with a fresh temporary directory in self.root."""

    def setUp(self) -> None:
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)

    def write(self, files: dict[str, str | bytes]) -> None:
        write_tree(self.root, files)
