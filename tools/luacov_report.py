# SPDX-License-Identifier: MIT
"""Turns a LuaCov stats file (written by `zauberer --lua-coverage`) into a
LuaCov report, using the vendored LuaCov and the zb_lua interpreter built
with the engine.

    luacov_report.py --lua build/zb_lua --stats luacov.stats.out --report luacov.report.out
                     [--include assets/base/scripts] [--root DIR]

Exit codes: 0 = report written, 2 = usage or input error.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from tool_common import EXIT_ERROR, EXIT_OK, REPO_ROOT


def lua_string(value: str) -> str:
    """A Lua long-bracket-free string literal."""
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def lua_pattern(prefix: str) -> str:
    """LuaCov include pattern matching files below prefix (any absolute root)."""
    special = "^$()%.[]*+-?"
    escaped = "".join("%" + c if c in special else c for c in prefix.strip("/"))
    return escaped + "/"


def make_config(stats: Path, report: Path, includes: list[str]) -> str:
    lines = ["return {", f"  statsfile = {lua_string(str(stats))},", f"  reportfile = {lua_string(str(report))},"]
    if includes:
        lines.append("  include = { " + ", ".join(lua_string(lua_pattern(p)) for p in includes) + " },")
    lines.append("}")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--lua", required=True, help="the zb_lua interpreter")
    parser.add_argument("--stats", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--root", default=str(REPO_ROOT))
    args = parser.parse_args(argv)
    stats = Path(args.stats).resolve()
    if not stats.is_file():
        print(f"luacov_report: stats file {stats} not found", file=sys.stderr)
        return EXIT_ERROR
    luacov = Path(args.root) / "third_party" / "luacov"
    with tempfile.TemporaryDirectory(prefix="zb-luacov-") as tmp:
        config = Path(tmp) / ".luacov"
        config.write_text(make_config(stats, Path(args.report).resolve(), args.include), encoding="utf-8")
        env = dict(os.environ)
        env["LUA_PATH"] = f"{luacov}/?.lua;{luacov}/?/init.lua;;"
        code = ("local runner = require('luacov.runner'); local cfg = runner.load_config(" + lua_string(str(config))
                + "); require('luacov.reporter').report()")
        lua = str(Path(args.lua).resolve()) if os.sep in args.lua or "/" in args.lua else args.lua
        try:
            result = subprocess.run([lua, "-e", code], cwd=tmp, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, check=False)
        except OSError as error:
            print(f"luacov_report: cannot run {args.lua}: {error}", file=sys.stderr)
            return EXIT_ERROR
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        return EXIT_ERROR
    print(f"luacov_report: wrote {args.report}")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
