# SPDX-License-Identifier: MIT
"""Tests for tools/tool_common.py (globs and directory walking)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase  # noqa: E402

from tool_common import glob_match, is_skipped_dir, rel_posix, walk_files  # noqa: E402


class GlobTest(unittest.TestCase):
    def test_reuse_glob_semantics(self):
        self.assertTrue(glob_match("assets/**", "assets/base/sprites/a.png"))
        self.assertTrue(glob_match("*.c", "main.c"))
        self.assertFalse(glob_match("*.c", "src/main.c"))
        self.assertTrue(glob_match("**.c", "src/core/main.c"))
        self.assertTrue(glob_match("src/**/x.c", "src/a/b/x.c"))
        self.assertFalse(glob_match("src/**/x.c", "src/x.c"))  # as in the reuse tool
        self.assertTrue(glob_match("a?.txt", "a?.txt"))  # '?' is literal
        self.assertFalse(glob_match("a?.txt", "ab.txt"))
        self.assertTrue(glob_match(r"lit\*.txt", "lit*.txt"))
        self.assertFalse(glob_match(r"lit\*.txt", "litx.txt"))
        self.assertTrue(glob_match("a.b", "a.b"))
        self.assertFalse(glob_match("a.b", "axb"))


class WalkTest(TempDirTestCase):
    def test_walk_skips_build_and_cache_directories(self):
        self.write({"a.txt": "", "sub/b.txt": "", "build/c.txt": "", "build-cov/d.txt": "",
                    "__pycache__/e.pyc": "", ".git/f": "", "node_modules/g.js": "",
                    "builder/h.txt": ""})
        found = [rel_posix(path, self.root) for path in walk_files(self.root)]
        self.assertEqual(found, ["a.txt", "builder/h.txt", "sub/b.txt"])
        self.assertTrue(is_skipped_dir("build-dev"))
        self.assertFalse(is_skipped_dir("builds"))


if __name__ == "__main__":
    unittest.main()
