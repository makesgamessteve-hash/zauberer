# SPDX-License-Identifier: MIT
"""Tests for tools/coverage_gate.py."""
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT, TempDirTestCase, run_main  # noqa: E402

import coverage_gate  # noqa: E402


def gcovr_summary(files):
    """gcovr --json-summary style document for [(name, total, covered)]."""
    entries = [{"filename": name, "line_total": total, "line_covered": covered,
                "line_percent": round(100.0 * covered / total, 1) if total else 100.0,
                "function_total": 1, "function_covered": 1, "branch_total": 0, "branch_covered": 0}
               for name, total, covered in files]
    line_total = sum(total for _, total, _ in files)
    line_covered = sum(covered for _, _, covered in files)
    return json.dumps({"gcovr/summary_format_version": "0.6", "files": entries,
                       "line_total": line_total, "line_covered": line_covered,
                       "line_percent": round(100.0 * line_covered / line_total, 1), "root": "."})


def luacov_report(rows):
    """LuaCov report text for [(name, hits, missed)], including a per-file
    listing before the Summary that must not be parsed as rows."""
    lines = ["==============================================================================",
             "assets/base/scripts/demo/state.lua",
             "==============================================================================",
             "   1 local M = {}",
             "*0 decoy.lua 1 2 50.00%",
             "",
             "==============================================================================",
             "Summary",
             "==============================================================================",
             "",
             "File                                   Hits Missed Coverage",
             "---------------------------------------------------------------"]
    for name, hits, missed in rows:
        lines.append(f"{name:<38} {hits:<4} {missed:<6} {100.0 * hits / (hits + missed):.2f}%")
    hits = sum(r[1] for r in rows)
    missed = sum(r[2] for r in rows)
    lines += ["---------------------------------------------------------------",
              f"{'Total':<38} {hits:<4} {missed:<6} {100.0 * hits / (hits + missed):.2f}%", ""]
    return "\n".join(lines)


class CoverageGateTest(TempDirTestCase):
    def gate_c(self, files, *args):
        self.write({"cov.json": gcovr_summary(files)})
        return run_main(coverage_gate.main, "c", "--root", self.root,
                        "--gcovr-json", self.root / "cov.json", *args)

    def gate_lua(self, rows, *args):
        self.write({"luacov.report.out": luacov_report(rows)})
        return run_main(coverage_gate.main, "lua", "--root", self.root,
                        "--luacov-report", self.root / "luacov.report.out", *args)

    # -- threshold -----------------------------------------------------------
    def test_c_coverage_at_threshold_passes(self):
        code, out = self.gate_c([("src/core/a.c", 600, 570), ("src/core/b.c", 400, 380)])
        self.assertEqual(code, 0, out)
        self.assertIn("C line coverage: 95.00% (950/1000 lines in 2 files)", out)

    def test_coverage_below_threshold_lists_least_covered_files(self):
        # spec: test-automation: Coverage below threshold
        files = [(f"src/core/f{i:02}.c", 100, 100 - i) for i in range(12)]  # f00 .. f11
        files.append(("src/core/big.c", 1000, 1000))
        # covered 1200 - 66 = 1134 of 1200 + 1000 -> want 94.8: adjust big.c
        files[-1] = ("src/core/big.c", 1000, 952)  # (1134 + 952) / 2200 = 94.818...
        code, out = self.gate_c(files, "--threshold", "95")
        self.assertEqual(code, 1)
        self.assertIn("FAIL: C line coverage 94.82% is below the threshold 95.00%", out)
        listed = [line.split()[1] for line in out.splitlines() if line.startswith("  ") and "%" in line]
        self.assertEqual(len(listed), 10)
        self.assertEqual(listed[0], "src/core/f11.c")  # least covered first
        self.assertEqual(listed[:3], ["src/core/f11.c", "src/core/f10.c", "src/core/f09.c"])
        self.assertNotIn("src/core/f00.c", listed)

    def test_threshold_uses_two_decimal_rounding(self):
        code, out = self.gate_c([("src/a.c", 100000, 94996)])  # 94.996 -> 95.00
        self.assertEqual(code, 0, out)

    def test_lua_coverage_below_threshold(self):
        code, out = self.gate_lua([("assets/base/scripts/main.lua", 90, 10)])
        self.assertEqual(code, 1)
        self.assertIn("Lua line coverage 90.00% is below the threshold 95.00%", out)
        self.assertIn("assets/base/scripts/main.lua (90/100 lines, 10 missed)", out)

    def test_lua_include_filter(self):
        rows = [("assets/base/scripts/main.lua", 99, 1), ("third/lib.lua", 1, 99),
                ("tests/lua/test_x.lua", 0, 50)]
        code, out = self.gate_lua(rows, "--include", "assets/base/scripts/")
        self.assertEqual(code, 0, out)
        self.assertIn("99.00% (99/100 lines in 1 files)", out)
        code, _ = self.gate_lua(rows)
        self.assertEqual(code, 1)  # third/lib.lua counts without the filter

    def test_third_party_and_tests_are_never_measured(self):
        code, out = self.gate_c([("src/a.c", 100, 100), ("third_party/lua/lapi.c", 500, 10),
                                 ("tests/unit/core/test_a.c", 50, 0)])
        self.assertEqual(code, 0, out)
        self.assertIn("100.00% (100/100 lines in 1 files)", out)

    def test_absolute_report_paths_are_made_relative(self):
        absolute = str(self.root / "src" / "core" / "a.c")
        self.write({"ex.txt": "src/core/a.c | cannot run in CI\n"})
        code, out = self.gate_c([(absolute, 1, 0), ("src/core/b.c", 1000, 1000)],
                                "--exclusions", self.root / "ex.txt")
        self.assertEqual(code, 0, out)
        self.assertIn("excluded: 1 lines in 1 files", out)

    # -- ratchet -------------------------------------------------------------
    def test_coverage_ratchet(self):
        # spec: test-automation: Coverage ratchet
        self.write({"baseline.json": json.dumps({"c": 0, "lua": 97.1})})
        code, out = self.gate_lua([("assets/base/scripts/main.lua", 965, 35)],
                                  "--baseline", self.root / "baseline.json")
        self.assertEqual(code, 1)
        self.assertIn("96.50% is lower than the recorded baseline 97.10%", out)

    def test_ratchet_uses_the_value_for_the_kind(self):
        self.write({"baseline.json": json.dumps({"c": 99.0, "lua": 50.0})})
        code, out = self.gate_lua([("a.lua", 965, 35)], "--baseline", self.root / "baseline.json")
        self.assertEqual(code, 0, out)

    def test_ratchet_compares_rounded_values(self):
        self.write({"baseline.json": json.dumps({"c": 97.104})})
        code, out = self.gate_c([("src/a.c", 100000, 97096)],  # 97.096 -> 97.10 (rounded)
                                "--baseline", self.root / "baseline.json")
        self.assertEqual(code, 0, out)

    def test_update_baseline_only_increases(self):
        baseline = self.root / "baseline.json"
        self.write({"baseline.json": json.dumps({"c": 96.0, "lua": 90.0})})
        code, out = self.gate_c([("src/a.c", 1000, 975)], "--baseline", baseline, "--update-baseline")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.loads(baseline.read_text()), {"c": 97.5, "lua": 90.0})
        # A lower value does not lower the baseline (and update mode does not fail on it).
        code, out = self.gate_c([("src/a.c", 1000, 960)], "--baseline", baseline, "--update-baseline")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.loads(baseline.read_text())["c"], 97.5)
        # Without --update-baseline the drop fails the ratchet.
        code, _ = self.gate_c([("src/a.c", 1000, 960)], "--baseline", baseline)
        self.assertEqual(code, 1)

    def test_update_baseline_skipped_when_threshold_fails(self):
        baseline = self.root / "baseline.json"
        self.write({"baseline.json": json.dumps({"c": 0, "lua": 0})})
        code, _ = self.gate_c([("src/a.c", 100, 90)], "--baseline", baseline, "--update-baseline")
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(baseline.read_text())["c"], 0)

    def test_update_baseline_creates_missing_file(self):
        baseline = self.root / "new-baseline.json"
        code, out = self.gate_lua([("a.lua", 99, 1)], "--baseline", baseline, "--update-baseline")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.loads(baseline.read_text()), {"lua": 99.0})

    def test_missing_baseline_without_update_is_an_error(self):
        code, out = self.gate_c([("src/a.c", 1, 1)], "--baseline", self.root / "missing.json")
        self.assertEqual(code, 2)
        self.assertIn("does not exist", out)

    # -- exclusions ------------------------------------------------------------
    def test_unjustified_exclusion(self):
        # spec: test-automation: Unjustified exclusion
        self.write({"ex.txt": "# comment\n\nsrc/platform/platform_sdl.c\n"
                              "src/render/backend_gl.c |   \n"
                              "src/core/ok.c | only reachable with a real GPU driver\n"})
        files = [("src/core/a.c", 100000, 100000)]
        code, out = self.gate_c(files, "--exclusions", self.root / "ex.txt")
        self.assertEqual(code, 1)
        self.assertIn("'src/platform/platform_sdl.c' has no justification", out)
        self.assertIn("ex.txt:4: exclusion entry 'src/render/backend_gl.c' has no justification", out)
        self.assertNotIn("src/core/ok.c' has no justification", out)

    def test_excluded_files_are_removed_from_totals(self):
        self.write({"ex.txt": "src/platform/*.c | needs a display\n"})
        files = [("src/core/a.c", 2000, 1950), ("src/platform/platform_sdl.c", 20, 0)]
        code, out = self.gate_c(files, "--exclusions", self.root / "ex.txt")
        self.assertEqual(code, 0, out)
        self.assertIn("97.50% (1950/2000 lines in 1 files)", out)
        self.assertIn("excluded: 20 lines in 1 files (0.99% of 2020 measured lines", out)

    def test_exclusion_budget_exceeded(self):
        self.write({"ex.txt": "src/platform/ | needs a display\n"})
        files = [("src/core/a.c", 1000, 1000), ("src/platform/x.c", 11, 0)]
        code, out = self.gate_c(files, "--exclusions", self.root / "ex.txt")
        self.assertEqual(code, 1)
        self.assertIn("FAIL: excluded lines (11) exceed 1% of measured lines (1011)", out)

    def test_exclusion_patterns(self):
        star = coverage_gate.Exclusion("src/*.c", "x", 1)
        self.assertTrue(star.matches("src/a.c"))
        self.assertFalse(star.matches("src/core/a.c"))
        self.assertTrue(coverage_gate.Exclusion("src/**.c", "x", 1).matches("src/core/a.c"))
        self.assertTrue(coverage_gate.Exclusion("src/core/", "x", 1).matches("src/core/sub/a.c"))
        self.assertTrue(coverage_gate.Exclusion("src/core/a.c", "x", 1).matches("src/core/a.c"))
        self.assertFalse(coverage_gate.Exclusion("src/core/a.c", "x", 1).matches("src/core/a.cc"))

    # -- input errors ------------------------------------------------------------
    def test_invalid_inputs_are_errors(self):
        self.write({"bad.json": "{not json", "nofiles.json": "{}", "report.out": "no summary\n"})
        for args in (["c", "--gcovr-json", self.root / "bad.json"],
                     ["c", "--gcovr-json", self.root / "nofiles.json"],
                     ["c", "--gcovr-json", self.root / "missing.json"],
                     ["lua", "--luacov-report", self.root / "report.out"],
                     ["c", "--gcovr-json", self.root / "bad.json", "--update-baseline"],
                     ["rust", "--gcovr-json", self.root / "bad.json"],
                     []):
            code, _ = run_main(coverage_gate.main, *args)
            self.assertEqual(code, 2, args)

    def test_no_measured_lines_is_an_error(self):
        code, out = self.gate_lua([("other/x.lua", 1, 0)], "--include", "assets/base/scripts/")
        self.assertEqual(code, 2)
        self.assertIn("no measured Lua lines", out)

    # -- the repository's own files ------------------------------------------------
    def test_repository_exclusion_list_and_baseline_are_valid(self):
        text = (REPO_ROOT / "tools" / "coverage-exclusions.txt").read_text(encoding="utf-8")
        _, problems = coverage_gate.parse_exclusions(text, "coverage-exclusions.txt")
        self.assertEqual(problems, [])
        baseline = json.loads((REPO_ROOT / "tools" / "coverage-baseline.json").read_text(encoding="utf-8"))
        for kind in coverage_gate.KINDS:
            self.assertIsInstance(baseline[kind], (int, float))


if __name__ == "__main__":
    unittest.main()
