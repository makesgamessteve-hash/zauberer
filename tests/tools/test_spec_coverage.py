# SPDX-License-Identifier: MIT
"""Tests for tools/spec_coverage.py."""
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import spec_coverage  # noqa: E402

DEMO_SPEC = """# Spec: demo

## ADDED Requirements

### Requirement: Thing
The thing SHALL work.

#### Scenario: First scenario
- **WHEN** something happens
- **THEN** it works

#### Scenario:   Second    scenario
- **WHEN** something else happens
"""


class SpecCoverageTest(TempDirTestCase):
    def run_tool(self, *args):
        return run_main(spec_coverage.main, "--root", self.root, *args)

    def test_all_scenarios_covered(self):
        self.write({
            "openspec/specs/demo/spec.md": DEMO_SPEC,
            "tests/unit/test_demo.c": "/* spec: demo: First scenario */\nvoid t(void) {}\n",
            "tests/scenarios/demo.lua": "-- spec:   demo:  Second\tscenario\n",
        })
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)
        self.assertIn("2/2 scenarios covered", out)

    def test_untested_scenario_fails_and_names_it(self):
        # spec: test-automation: Untested scenario
        self.write({
            "openspec/specs/demo/spec.md": DEMO_SPEC,
            "tests/unit/test_demo.c": "/* spec: demo: First scenario */\n",
        })
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("demo: Second scenario", out.splitlines()[1])
        self.assertNotIn("demo: First scenario", out)
        self.assertIn("1/2 scenarios covered", out)

    def test_nested_capability_changes_and_archive(self):
        self.write({
            "openspec/specs/identity/user-auth/spec.md": "#### Scenario: Login\n",
            "openspec/changes/add-x/specs/demo/spec.md": "#### Scenario: From change\n",
            "openspec/changes/archive/old/specs/demo/spec.md": "#### Scenario: Archived\n",
            "tests/a.py": "# spec: identity/user-auth: Login\n",
        })
        covered, uncovered = spec_coverage.check(self.root)
        self.assertEqual([s.tag for s in covered], ["identity/user-auth: Login"])
        self.assertEqual([s.tag for s in uncovered], ["demo: From change"])

    def test_scenario_in_change_and_main_specs_counts_once(self):
        self.write({
            "openspec/specs/demo/spec.md": "#### Scenario: Shared\n",
            "openspec/changes/one/specs/demo/spec.md": "#### Scenario: Shared\n",
            "openspec/changes/two/specs/demo/spec.md": "#### Scenario: Shared\n",
        })
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("0/1 scenarios covered", out)
        self.assertEqual(out.count("demo: Shared"), 1)

    def test_name_must_match_exactly(self):
        self.write({
            "openspec/specs/demo/spec.md": "#### Scenario: Low rate\n#### Scenario: Low rate limit\n"
                                           "#### Scenario: Pause\n",
            # Tags for "Low rate limit" and "Pauses" must not cover "Low rate" / "Pause".
            "tests/a.c": "/* spec: demo: Low rate limit */\n// spec: demo: Pauses\n",
        })
        covered, uncovered = spec_coverage.check(self.root)
        self.assertEqual([s.name for s in covered], ["Low rate limit"])
        self.assertEqual([s.name for s in uncovered], ["Low rate", "Pause"])

    def test_tags_in_strings_and_several_per_line(self):
        self.write({
            "openspec/specs/demo/spec.md": "#### Scenario: One\n#### Scenario: Two\n",
            "tests/s.lua": 'scenario{ tags = { "spec: demo: One", "spec: demo: Two" } }\n',
        })
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)

    def test_removed_and_fenced_scenarios_are_ignored(self):
        self.write({
            "openspec/specs/demo/spec.md": (
                "## ADDED Requirements\n#### Scenario: Real\n"
                "```markdown\n#### Scenario: In a code block\n```\n"
                "## REMOVED Requirements\n#### Scenario: Gone\n"),
            "tests/a.c": "// spec: demo: Real\n",
        })
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)
        self.assertIn("1/1 scenarios covered", out)

    def test_cache_and_build_directories_are_not_scanned(self):
        self.write({
            "openspec/specs/demo/spec.md": "#### Scenario: Hidden\n",
            "tests/__pycache__/a.pyc": b"spec: demo: Hidden\n",
            "tests/node_modules/x.js": "// spec: demo: Hidden\n",
            "tests/build/x.c": "// spec: demo: Hidden\n",
        })
        code, _ = self.run_tool()
        self.assertEqual(code, 1)

    def test_json_report(self):
        self.write({
            "openspec/specs/demo/spec.md": DEMO_SPEC,
            "tests/x.c": "// spec: demo: First scenario\n",
        })
        out_file = self.root / "report.json"
        code, _ = self.run_tool("--json", out_file)
        self.assertEqual(code, 1)
        report = json.loads(out_file.read_text(encoding="utf-8"))
        self.assertEqual(report, {"covered": ["demo: First scenario"],
                                  "uncovered": ["demo: Second scenario"]})

    def test_missing_openspec_directory_is_an_error(self):
        code, out = self.run_tool()
        self.assertEqual(code, 2)
        self.assertIn("openspec", out)

    def test_bad_option_is_a_usage_error(self):
        code, _ = self.run_tool("--no-such-option")
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
