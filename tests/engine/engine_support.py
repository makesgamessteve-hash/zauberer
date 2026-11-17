# SPDX-License-Identifier: MIT
"""Helpers for engine-level tests: run the zauberer binary with a fresh user
directory, optional fixture mods and optional Lua coverage collection.

Environment:
  ZB_BIN          path of the zauberer executable (required)
  ZB_SOURCE_DIR   repository root (default: two levels above this file)
  ZB_LUA_COVERAGE when set, every run adds --lua-coverage <file>
  ZB_RENDER_TESTS "1" enables the rendered (windowed) tests
  ZB_RUNNER       program that runs the executables (node for web-node builds)
"""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SOURCE_DIR = Path(os.environ.get("ZB_SOURCE_DIR", Path(__file__).resolve().parents[2]))
ASSETS = SOURCE_DIR / "assets"
FIXTURE_MODS = SOURCE_DIR / "tests" / "fixtures" / "mods"


def runner_prefix() -> list[str]:
    """The emulator command for cross builds (web-node runs under Node.js)."""
    runner = os.environ.get("ZB_RUNNER", "")
    return [runner] if runner and not runner.endswith("-NOTFOUND") else []


def engine_binary() -> Path:
    value = os.environ.get("ZB_BIN")
    if not value:
        raise unittest.SkipTest("ZB_BIN is not set")
    return Path(value)


class EngineTestCase(unittest.TestCase):
    """Runs the engine; self.user_dir is a fresh user data directory per test."""

    def setUp(self) -> None:
        self.bin = engine_binary()
        self._tmp = tempfile.TemporaryDirectory(prefix="zb-engine-")
        self.tmp = Path(self._tmp.name)
        self.user_dir = self.tmp / "user"
        self.user_dir.mkdir()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def install_mods(self, *names: str, user_dir: Path | None = None) -> None:
        target = (user_dir or self.user_dir) / "mods"
        target.mkdir(parents=True, exist_ok=True)
        for name in names:
            shutil.copytree(FIXTURE_MODS / name, target / name)

    def run_engine(self, *args: str, assets: Path | None = ASSETS, user_dir: Path | None = None,
                   env: dict[str, str] | None = None, binary: Path | None = None,
                   cwd: Path | None = None, timeout: float = 300) -> subprocess.CompletedProcess:
        cmd = runner_prefix() + [str(binary or self.bin)]
        if assets is not None:
            cmd += ["--assets", str(assets)]
        cmd += ["--user-dir", str(user_dir or self.user_dir)]
        coverage = os.environ.get("ZB_LUA_COVERAGE")
        if coverage:
            cmd += ["--lua-coverage", coverage]
        cmd += list(args)
        full_env = dict(os.environ)
        full_env.setdefault("ZB_NO_DIALOGS", "1")
        full_env.setdefault("ZB_GOLDEN_OUT", str(self.tmp / "golden-out"))
        full_env.pop("ZB_TOUCH_PRIMARY", None)
        full_env.update(env or {})
        return subprocess.run(cmd, cwd=str(cwd or SOURCE_DIR), env=full_env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, timeout=timeout, check=False)

    def assert_ok(self, result: subprocess.CompletedProcess) -> None:
        self.assertEqual(result.returncode, 0, result.stdout[-4000:])

    def assert_fails(self, result: subprocess.CompletedProcess) -> None:
        self.assertNotEqual(result.returncode, 0, result.stdout[-4000:])

    def scenario(self, name: str, *args: str, headless: bool = True, **kwargs) -> subprocess.CompletedProcess:
        extra = ["--headless"] if headless else []
        return self.run_engine(*extra, "--scenario", f"tests/scenarios/{name}.lua", *args, **kwargs)

    @staticmethod
    def final_hash(output: str) -> str:
        for line in reversed(output.splitlines()):
            if "state hash at tick" in line:
                return line.rsplit(" ", 1)[1]
        raise AssertionError("no state hash reported:\n" + output[-2000:])
