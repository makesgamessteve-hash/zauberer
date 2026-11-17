# SPDX-License-Identifier: MIT
"""Tests for tools/compare_hashes.py."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import compare_hashes  # noqa: E402


class CompareHashesTest(TempDirTestCase):
    def report(self, name, value):
        path = self.root / f"{name}.txt"
        path.write_text(value + "\n", encoding="utf-8")
        return f"{name}={path}"

    def test_equal_hashes_pass(self):
        code, out = run_main(compare_hashes.main, self.report("linux", "0123456789abcdef"),
                             self.report("web", "0123456789abcdef"), self.report("windows", "0123456789abcdef"))
        self.assertEqual(code, 0)
        self.assertIn("3 platforms agree on 0123456789abcdef", out)

    # spec: test-automation: Hash mismatch between platforms
    def test_mismatch_shows_both_hashes(self):
        code, out = run_main(compare_hashes.main, self.report("linux", "0123456789abcdef"),
                             self.report("web", "fedcba9876543210"))
        self.assertEqual(code, 1)
        self.assertIn("linux  0123456789abcdef", out)
        self.assertIn("web    fedcba9876543210", out)

    def test_usage_and_input_errors(self):
        self.assertEqual(run_main(compare_hashes.main, "linux=x")[0], 2)
        self.assertEqual(run_main(compare_hashes.main, "nonsense", "other")[0], 2)
        code, out = run_main(compare_hashes.main, self.report("linux", "not-a-hash"), self.report("web", "0" * 16))
        self.assertEqual(code, 2)
        self.assertIn("is not a state hash", out)
        code, out = run_main(compare_hashes.main, "linux=" + str(self.root / "missing"), self.report("web", "0" * 16))
        self.assertEqual(code, 2)
        self.assertIn("cannot read", out)


if __name__ == "__main__":
    unittest.main()
