# SPDX-License-Identifier: MIT
"""Compares the final replay state hashes reported by the CI platform jobs.

    compare_hashes.py <platform>=<file> [<platform>=<file> ...]

Each file holds the 16-hex-digit hash a platform reported for the reference
replay (tests/engine writes it when ZB_HASH_REPORT is set). All hashes must
be equal; otherwise every platform's hash is printed and the exit code is 1.

Exit codes: 0 = all equal, 1 = mismatch, 2 = usage or input error.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from tool_common import EXIT_ERROR, EXIT_FAIL, EXIT_OK

_HASH_RE = re.compile(r"^[0-9a-f]{16}$")


def read_reports(args: list[str]) -> dict[str, str]:
    reports = {}
    for arg in args:
        if "=" not in arg:
            raise ValueError(f"'{arg}' is not <platform>=<file>")
        platform, file = arg.split("=", 1)
        try:
            value = Path(file).read_text(encoding="utf-8").strip()
        except OSError as error:
            raise ValueError(f"{platform}: cannot read {file}: {error}") from error
        if not _HASH_RE.match(value):
            raise ValueError(f"{platform}: '{value}' in {file} is not a state hash")
        reports[platform] = value
    return reports


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if len(args) < 2:
        print(__doc__, file=sys.stderr)
        return EXIT_ERROR
    try:
        reports = read_reports(args)
    except ValueError as error:
        print(f"compare_hashes: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    width = max(len(p) for p in reports)
    if len(set(reports.values())) == 1:
        print(f"compare_hashes: {len(reports)} platforms agree on {next(iter(reports.values()))}")
        return EXIT_OK
    print("compare_hashes: state hashes differ between platforms:")
    for platform, value in sorted(reports.items()):
        print(f"  {platform:<{width}}  {value}")
    return EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
