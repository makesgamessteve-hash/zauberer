# SPDX-License-Identifier: MIT
"""Runs the gameplay bot (tests/bots/play_bot.lua) on several seeds and styles.

    python3 tools/run_bots.py --bin build/release/bin/zauberer --out <dir> [--runs 10] [--jobs 3]
                              [--ticks 150000]

Run n plays seed n with style STYLES[(n - 1) % 4]; each run's log (the BOT
lines) is written to <out>/run-<n>-<style>.log. Standard library only.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STYLES = ("story", "explorer", "hunter", "camper")


def run(n: int, args: argparse.Namespace) -> str:
    style = STYLES[(n - 1) % len(STYLES)]
    text = (ROOT / "tests" / "bots" / "play_bot.lua").read_text(encoding="utf-8")
    text = re.sub(r'^local STYLE = "[a-z]+"', f'local STYLE = "{style}"', text, flags=re.M)
    text = re.sub(r"^local BUDGET = \d+", f"local BUDGET = {args.ticks}", text, flags=re.M)
    out = Path(args.out)
    with tempfile.TemporaryDirectory(prefix="zb-bot-") as tmp:
        scenario = Path(tmp) / f"bot-{n}.lua"
        scenario.write_text(text, encoding="utf-8")
        result = subprocess.run([args.bin, "--assets", str(ROOT / "assets"), "--user-dir", str(Path(tmp) / "user"),
                                 "--headless", "--seed", str(n), "--scenario", str(scenario)],
                                capture_output=True, text=True, errors="replace", timeout=args.timeout)
    lines = [line.split("[lua] ", 1)[1] for line in result.stdout.splitlines() if "[lua] BOT" in line]
    errors = [line for line in (result.stdout + result.stderr).splitlines() if "[error]" in line]
    path = out / f"run-{n:02d}-{style}.log"
    path.write_text("\n".join(lines + errors) + "\n", encoding="utf-8")
    return f"run {n} ({style}): {len(lines)} lines, {len(errors)} errors, exit {result.returncode}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--bin", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--ticks", type=int, default=150000)
    parser.add_argument("--timeout", type=int, default=7200)
    args = parser.parse_args(argv)
    Path(args.out).mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for line in pool.map(lambda n: run(n, args), range(1, args.runs + 1)):
            print(line, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
