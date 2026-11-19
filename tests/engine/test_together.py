# SPDX-License-Identifier: MIT
"""The M9 exit demo as processes: a relay, a host and three clients join one
world by invite code, ride together, split up, reunite, pitch the tent and
vote to sleep; the host saves and quits, and the next day everyone rejoins.

Environment: ZB_BIN, ZB_SOURCE_DIR (see engine_support), ZB_RELAY_BIN.
"""
from __future__ import annotations

import os
import re
import subprocess
import time
import unittest
from pathlib import Path

from tests.engine.engine_support import EngineTestCase, SOURCE_DIR
from tests.engine.test_relay import relay_binary

NAMES = ["Rostam", "Shirin", "Dara", "Parisa"]


class TogetherTests(EngineTestCase):
    def relay(self) -> tuple[subprocess.Popen, int]:
        proc = subprocess.Popen([str(relay_binary()), "--port", "0", "--seed", "3"], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True)
        self.addCleanup(proc.stdout.close)
        self.addCleanup(lambda: proc.poll() is None and (proc.terminate(), proc.wait()))
        line = proc.stdout.readline()
        m = re.search(r"listening on port (\d+)", line)
        self.assertTrue(m, line)
        return proc, int(m.group(1))

    def script(self, name: str, prelude: str) -> Path:
        path = self.tmp / f"{name}.lua"
        body = (SOURCE_DIR / "tests" / "scenarios" / f"{name}.lua").read_text(encoding="utf-8")
        path.write_text(prelude + body, encoding="utf-8")
        return path

    def wait_line(self, proc: subprocess.Popen, pattern: str, seconds: float) -> tuple[re.Match, list[str]]:
        seen = []
        end = time.time() + seconds
        while time.time() < end:
            line = proc.stdout.readline()
            if not line:
                break
            seen.append(line)
            m = re.search(pattern, line)
            if m:
                return m, seen
        self.fail(f"no '{pattern}' in:\n" + "".join(seen))

    def round(self, n: int, url: str, users: list[Path], slept_after: int = 0) -> int:
        names = "{" + ", ".join(f'"{x}"' for x in NAMES) + "}"
        prelude = f'together = {{ url = "{url}", round = {n}, names = {names}, slept_after = {slept_after} }}\n'
        host = self.start_engine("--headless", "--scenario", str(self.script("together_host", prelude)),
                                 user_dir=users[0])
        m, seen = self.wait_line(host, r"invite code: (\w+)", 120)
        code = m.group(1)
        clients = []
        for i in range(2, 5):  # one after the other, so each gets its heir again
            pre = prelude.replace("together = {", f'together = {{ code = "{code}", heir = {i},')
            clients.append(self.start_engine("--headless", "--scenario", str(self.script("together_client", pre)),
                                             user_dir=users[i - 1]))
            time.sleep(1.5)
        slept = 0
        out, _ = host.communicate(timeout=900)
        text = "".join(seen) + out
        self.assertIn("passed after", text, text[-3000:])
        m = re.search(r"slept until: (\d+)", text)
        if m:
            slept = int(m.group(1))
        for c in clients:
            cout, _ = c.communicate(timeout=300)
            self.assertIn("passed after", cout, cout[-3000:])
        return slept

    def test_four_heirs_together(self):
        _, port = self.relay()
        url = f"ws://127.0.0.1:{port}"
        users = []
        for i in range(4):
            d = self.tmp / f"user{i}"
            d.mkdir()
            users.append(d)
        slept = self.round(1, url, users)
        self.assertGreater(slept, 0)
        self.round(2, url, users, slept_after=slept - 60)


if __name__ == "__main__":
    unittest.main()
