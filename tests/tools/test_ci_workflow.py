# SPDX-License-Identifier: MIT
"""Checks the structure of .github/workflows/ci.yml: every platform job runs
on every push and pull request, none may fail silently, and the determinism
job compares the hashes of all platforms."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT  # noqa: E402

try:
    import yaml
except ImportError:  # pragma: no cover - PyYAML is part of the CI images
    yaml = None

PLATFORM_JOBS = {"lint", "linux-gcc-coverage", "linux-clang-asan", "linux-render", "windows-msvc",
                 "windows-gl-smoke", "web"}


@unittest.skipIf(yaml is None, "PyYAML is not installed")
class CiWorkflowTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = (REPO_ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        cls.workflow = yaml.safe_load(text)
        cls.jobs = cls.workflow["jobs"]

    def test_runs_on_every_push_and_pull_request(self):
        triggers = self.workflow.get("on", self.workflow.get(True))
        self.assertIn("push", triggers)
        self.assertIn("pull_request", triggers)
        self.assertIsNone(triggers["push"], "no branch filter: every push runs CI")

    # spec: test-automation: Platform-specific failure
    def test_a_failure_on_any_platform_fails_the_run(self):
        self.assertTrue(PLATFORM_JOBS <= set(self.jobs), sorted(PLATFORM_JOBS - set(self.jobs)))
        runners = {name: job["runs-on"] for name, job in self.jobs.items()}
        self.assertTrue(runners["windows-msvc"].startswith("windows"))
        for name, job in self.jobs.items():
            self.assertFalse(job.get("continue-on-error", False), f"{name} must not be allowed to fail")
            for step in job["steps"]:
                self.assertFalse(step.get("continue-on-error", False), f"a step of {name} may not fail silently")
            self.assertNotIn("if", job, f"{name} must always run")

    def test_determinism_compares_every_platform(self):
        det = self.jobs["determinism"]
        self.assertEqual(set(det["needs"]), {"linux-gcc-coverage", "windows-msvc", "web"})
        script = det["steps"][-1]["run"]
        for platform in ("linux", "windows", "web"):
            self.assertIn(f"{platform}=", script)
        for job in det["needs"]:
            uploads = [s for s in self.jobs[job]["steps"] if s.get("uses", "").startswith("actions/upload-artifact")]
            self.assertTrue(any(s["with"]["name"].startswith("hash-") for s in uploads), job)

    def test_coverage_gates_run(self):
        steps = " ".join(s.get("run", "") for s in self.jobs["linux-gcc-coverage"]["steps"])
        self.assertIn("coverage_gate.py c", steps)
        self.assertIn("coverage_gate.py lua", steps)
        self.assertIn("--baseline tools/coverage-baseline.json", steps)


if __name__ == "__main__":
    unittest.main()
