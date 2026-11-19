# SPDX-License-Identifier: MIT
"""Tests for tools/deploy_web.py against a temporary local bare repository
(never a real remote)."""
import os
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import deploy_web  # noqa: E402
from tool_common import ToolError  # noqa: E402

VERSION_H = '#define ZB_VERSION "1.0.0-rc.1"\n'


class DeployWebTest(TempDirTestCase):
    def setUp(self):
        super().setUp()
        # Git sees only this test's configuration: an identity, no signing.
        config = self.root / "gitconfig"
        config.write_text("[user]\n\tname = Test\n\temail = test@example.invalid\n"
                          "[commit]\n\tgpgsign = false\n[init]\n\tdefaultBranch = main\n", encoding="utf-8")
        patcher = mock.patch.dict(os.environ, {"GIT_CONFIG_GLOBAL": str(config), "GIT_CONFIG_NOSYSTEM": "1"})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.remote = self.root / "remote.git"
        self.git(self.root, "init", "-q", "--bare", str(self.remote))
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.git(self.repo, "init", "-q")
        self.write({"repo/src/core/version.h": VERSION_H, "repo/README.md": "source\n"})
        self.git(self.repo, "add", "-A")
        self.git(self.repo, "commit", "-q", "-m", "source")
        self.git(self.repo, "remote", "add", "origin", str(self.remote))
        self.head = self.git(self.repo, "rev-parse", "HEAD")

    def git(self, cwd, *args):
        return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()

    def build(self, name="site", files=None):
        files = files or {"index.html": "<html>v1</html>", "zauberer-st.js": "js", "zauberer-st.wasm": "wasm",
                          "assets/base/package.lua": "return {}"}
        self.write({f"{name}/{rel}": content for rel, content in files.items()})
        return self.root / name

    def remote_files(self, ref="gh-pages"):
        return sorted(self.git(self.remote, "ls-tree", "-r", "--name-only", ref).splitlines())

    def deploy(self, source, *args):
        return run_main(deploy_web.main, source, "--repo", self.repo, *args)

    def test_deploy_publishes_the_build(self):
        # spec: release-pipeline: Deploy publishes the web build
        code, out = self.deploy(self.build())
        self.assertEqual(code, 0, out)
        self.assertEqual(self.remote_files(), [".nojekyll", "assets/base/package.lua", "index.html", "version.txt",
                                               "zauberer-st.js", "zauberer-st.wasm"])
        self.assertEqual(self.git(self.remote, "show", "gh-pages:version.txt"), f"1.0.0-rc.1\n{self.head}")
        message = self.git(self.remote, "log", "-1", "--format=%B", "gh-pages")
        self.assertIn(f"Deploy web build 1.0.0-rc.1 (commit {self.head[:12]})", message)
        self.assertIn(self.head, message)
        first = self.git(self.remote, "rev-parse", "gh-pages")
        # The repository gains no branch and keeps no worktree.
        self.assertEqual(self.git(self.repo, "branch", "--format=%(refname:short)"), "main")
        self.assertEqual(len(self.git(self.repo, "worktree", "list").splitlines()), 1)

        # A new build replaces the old one on top of the branch's history.
        code, out = self.deploy(self.build("site2", {"index.html": "<html>v2</html>", "zauberer.js": "mt"}),
                                "--version", "1.0.0", "--commit", "abcdef0123456789")
        self.assertEqual(code, 0, out)
        self.assertEqual(self.remote_files(), [".nojekyll", "index.html", "version.txt", "zauberer.js"])
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages^"), first)
        self.assertIn("Deploy web build 1.0.0 (commit abcdef012345)",
                      self.git(self.remote, "log", "-1", "--format=%s", "gh-pages"))

        # The same build again: nothing to deploy.
        code, out = self.deploy(self.root / "site2", "--version", "1.0.0", "--commit", "abcdef0123456789")
        self.assertEqual(code, 0, out)
        self.assertIn("already has this build; nothing to deploy", out)
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages^"), first)

    def test_dry_run_changes_nothing(self):
        # spec: release-pipeline: Web deploy dry run changes nothing
        code, out = self.deploy(self.build(), "--dry-run")
        self.assertEqual(code, 0, out)
        self.assertIn("origin has no branch gh-pages; it will be started", out)
        self.assertIn("A  index.html", out)
        self.assertIn("A  .nojekyll", out)
        self.assertIn("dry run: would commit 'Deploy web build 1.0.0-rc.1", out)
        self.assertEqual(self.git(self.remote, "branch", "--list"), "")
        self.deploy(self.root / "site")
        before = self.git(self.remote, "rev-parse", "gh-pages")
        code, out = self.deploy(self.build("site2", {"index.html": "<html>v2</html>"}), "--dry-run")
        self.assertEqual(code, 0, out)
        self.assertIn("M  index.html", out)
        self.assertIn("D  zauberer-st.js", out)
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages"), before)
        self.assertEqual(len(self.git(self.repo, "worktree", "list").splitlines()), 1)

    def test_push_is_retried_with_backoff(self):
        # spec: release-pipeline: Push is retried with backoff
        results = iter([False, False, True])
        waits = []
        attempts = deploy_web.push_with_retries(lambda: next(results), retries=4, sleep=waits.append)
        self.assertEqual((attempts, waits), (3, [2.0, 4.0]))
        waits = []
        with self.assertRaises(ToolError):
            deploy_web.push_with_retries(lambda: False, retries=4, sleep=waits.append)
        self.assertEqual(waits, [2.0, 4.0, 8.0, 16.0])
        # A remote that refuses pushes: the commit is made, the push fails after the retries.
        self.git(self.repo, "remote", "set-url", "--push", "origin", str(self.root / "missing.git"))
        with mock.patch.object(deploy_web.time, "sleep") as sleep:
            code, out = self.deploy(self.build())
        self.assertEqual(code, 2)
        self.assertIn("push failed 5 times; giving up", out)
        self.assertEqual([c.args[0] for c in sleep.call_args_list], [2.0, 4.0, 8.0, 16.0])

    def test_no_push_and_bad_sources(self):
        code, out = self.deploy(self.build(), "--no-push")
        self.assertEqual(code, 0, out)
        self.assertIn("--no-push: not pushed", out)
        self.assertEqual(self.git(self.remote, "branch", "--list"), "")
        self.write({"empty/readme.txt": "x"})
        code, out = self.deploy(self.root / "empty")
        self.assertEqual(code, 2)
        self.assertIn("is not a web build (no index.html)", out)
        code, out = self.deploy(self.build(), "--remote", "nowhere")
        self.assertEqual(code, 2)
        self.assertIn("git ls-remote", out)


if __name__ == "__main__":
    unittest.main()
