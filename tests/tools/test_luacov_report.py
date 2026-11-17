# SPDX-License-Identifier: MIT
from __future__ import annotations

import os
import unittest
from pathlib import Path

from tests.tools.toolsupport import TempDirTestCase, run_main

import luacov_report


class LuacovReportTests(TempDirTestCase):
    def test_pattern_escapes_magic_characters(self):
        self.assertEqual(luacov_report.lua_pattern("assets/base/scripts/"), "assets/base/scripts/")
        self.assertEqual(luacov_report.lua_pattern("a-b.c"), "a%-b%.c/")

    def test_config_lists_files_and_includes(self):
        stats, report = Path("/s/stats"), Path("/s/report")
        text = luacov_report.make_config(stats, report, ["assets/base/scripts"])
        self.assertIn(f"statsfile = {luacov_report.lua_string(str(stats))}", text)
        self.assertIn(f"reportfile = {luacov_report.lua_string(str(report))}", text)
        self.assertIn('include = { "assets/base/scripts/" }', text)
        self.assertNotIn("include", luacov_report.make_config(Path("a"), Path("b"), []))

    def test_lua_string_escapes(self):
        self.assertEqual(luacov_report.lua_string('a"b\\c'), '"a\\"b\\\\c"')

    def test_missing_stats_file(self):
        code, out = run_main(luacov_report.main, "--lua", "zb_lua", "--stats", self.root / "none",
                             "--report", self.root / "r")
        self.assertEqual(code, 2)
        self.assertIn("not found", out)

    def test_missing_interpreter(self):
        stats = self.root / "stats"
        stats.write_text("", encoding="utf-8")
        code, out = run_main(luacov_report.main, "--lua", self.root / "no-such-lua", "--stats", stats,
                             "--report", self.root / "r")
        self.assertEqual(code, 2)
        self.assertIn("cannot run", out)

    @unittest.skipIf(os.name == "nt", "uses a shell script as the fake interpreter")
    def test_interpreter_failure(self):
        stats = self.root / "stats"
        stats.write_text("", encoding="utf-8")
        fake = self.root / "fake-lua"
        fake.write_text("#!/bin/sh\necho broken >&2\nexit 3\n", encoding="utf-8")
        fake.chmod(0o755)
        code, out = run_main(luacov_report.main, "--lua", fake, "--stats", stats, "--report", self.root / "r")
        self.assertEqual(code, 2)
        self.assertIn("broken", out)


if __name__ == "__main__":
    unittest.main()
