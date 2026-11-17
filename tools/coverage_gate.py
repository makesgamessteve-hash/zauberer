# SPDX-License-Identifier: MIT
"""Line-coverage gate for C (gcovr) and Lua (LuaCov).

    coverage_gate.py c   --gcovr-json cov.json      [common options]
    coverage_gate.py lua --luacov-report luacov.report.out [common options]

Common options: --threshold PCT, --exclusions FILE, --baseline FILE,
--update-baseline, --include PREFIX (repeatable), --root DIR.

Coverage is computed from the per-file entries so that exclusions can be
applied. Files below third_party/ and tests/ are never measured. The gate
fails (exit 1) when:

* an exclusion entry has no justification,
* excluded lines exceed 1 % of the measured lines (excluded + included),
* line coverage of the included files is below --threshold (the 10
  least-covered files are listed),
* coverage is lower than the value recorded in --baseline (the ratchet).

All percentages are compared after rounding to two decimals.
--update-baseline raises the recorded value to the current one (never lowers
it) when the other checks pass. Exit code 2 means a usage or input error.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tool_common import (EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError,
                         glob_match, read_text)

KINDS = {"c": "C", "lua": "Lua"}
EXCLUSION_BUDGET = 0.01  # excluded lines may be at most 1 % of measured lines
LEAST_COVERED_COUNT = 10
ALWAYS_UNMEASURED = ("third_party/", "tests/")


@dataclass(frozen=True)
class FileCoverage:
    path: str
    total: int
    covered: int

    @property
    def missed(self) -> int:
        return self.total - self.covered

    @property
    def percent(self) -> float:
        return 100.0 * self.covered / self.total if self.total else 100.0


@dataclass(frozen=True)
class Exclusion:
    pattern: str
    justification: str
    line: int

    def matches(self, path: str) -> bool:
        pattern = self.pattern
        if pattern.endswith("/"):
            pattern += "**"
        return glob_match(pattern, path)


def normalize_path(name: str, root: Path) -> str:
    """Repository-relative POSIX path for a file name from a coverage report."""
    name = name.replace("\\", "/")
    if os.path.isabs(name):
        try:
            name = Path(name).relative_to(os.path.abspath(root)).as_posix()
        except ValueError:
            pass
    while name.startswith("./"):
        name = name[2:]
    return name


def parse_exclusions(text: str, source: str) -> tuple[list[Exclusion], list[str]]:
    """Parses '<path or glob> | <justification>' lines. Returns the entries
    and a list of problems (entries without a path or justification)."""
    entries, problems = [], []
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        pattern, bar, justification = line.partition("|")
        pattern, justification = pattern.strip(), justification.strip()
        if not pattern:
            problems.append(f"{source}:{number}: exclusion entry '{line}' has no path")
            continue
        if not bar or not justification:
            problems.append(f"{source}:{number}: exclusion entry '{pattern}' has no justification "
                            "(expected '<path or glob> | <justification>')")
        entries.append(Exclusion(pattern, justification, number))
    return entries, problems


def load_gcovr_json(text: str, root: Path) -> list[FileCoverage]:
    """Per-file line coverage from `gcovr --json-summary` output."""
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        raise ToolError(f"invalid gcovr JSON: {error}") from error
    files = data.get("files") if isinstance(data, dict) else None
    if not isinstance(files, list):
        raise ToolError("gcovr JSON has no 'files' list (use gcovr --json-summary)")
    result = []
    for entry in files:
        try:
            name = entry["filename"]
            total, covered = entry["line_total"], entry["line_covered"]
        except (KeyError, TypeError) as error:
            raise ToolError(f"gcovr JSON file entry lacks {error}: {entry!r}") from error
        if not isinstance(name, str) or not isinstance(total, int) or not isinstance(covered, int):
            raise ToolError(f"malformed gcovr JSON file entry: {entry!r}")
        result.append(FileCoverage(normalize_path(name, root), total, covered))
    return result


_ROW_RE = re.compile(r"^(?P<name>.+?)\s+(?P<hits>\d+)\s+(?P<missed>\d+)\s+(?P<pct>[\d.]+)%\s*$")


def _is_rule(line: str, char: str) -> bool:
    line = line.strip()
    return len(line) >= 3 and set(line) == {char}


def parse_luacov_report(text: str, root: Path) -> list[FileCoverage]:
    """Per-file rows from the Summary section of a LuaCov report."""
    lines = text.splitlines()
    start = None
    for index in range(1, len(lines) - 1):
        if (lines[index].strip() == "Summary" and _is_rule(lines[index - 1], "=")
                and _is_rule(lines[index + 1], "=")):
            start = index + 2
    if start is None:
        raise ToolError("LuaCov report has no Summary section")
    result = []
    for line in lines[start:]:
        match = _ROW_RE.match(line.strip())
        if not match:
            continue
        name = match.group("name").strip()
        if name in ("Total", "File"):
            continue
        hits, missed = int(match.group("hits")), int(match.group("missed"))
        result.append(FileCoverage(normalize_path(name, root), hits + missed, hits))
    if not result:
        raise ToolError("LuaCov report Summary lists no files")
    return result


def _sum(files: list[FileCoverage]) -> tuple[int, int]:
    return sum(f.total for f in files), sum(f.covered for f in files)


def least_covered(files: list[FileCoverage], count: int = LEAST_COVERED_COUNT) -> list[FileCoverage]:
    measured = [f for f in files if f.total > 0]
    return sorted(measured, key=lambda f: (f.percent, -f.missed, f.path))[:count]


def load_baseline(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise ToolError(f"baseline file {path} does not exist") from error
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ToolError(f"cannot read baseline {path}: {error}") from error
    if not isinstance(data, dict):
        raise ToolError(f"baseline {path} must be a JSON object such as {{\"c\": 97.1, \"lua\": 96.5}}")
    return data


def _baseline_value(data: dict, kind: str, path: Path) -> float:
    value = data.get(kind, 0)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ToolError(f"baseline {path}: value for '{kind}' is not a number")
    return float(value)


def run(kind: str, files: list[FileCoverage], *, exclusions: list[Exclusion],
        exclusion_problems: list[str], threshold: float, include: list[str],
        baseline_path: Path | None, update_baseline: bool) -> int:
    label = KINDS[kind]
    failures = list(exclusion_problems)

    files = [f for f in files if not f.path.startswith(ALWAYS_UNMEASURED)]
    if include:
        files = [f for f in files if any(prefix in f.path for prefix in include)]
    excluded, included = [], []
    for f in files:
        (excluded if any(e.matches(f.path) for e in exclusions) else included).append(f)
    excluded_total, _ = _sum(excluded)
    total, covered = _sum(included)
    measured = excluded_total + total
    if measured == 0:
        raise ToolError(f"no measured {label} lines (check the input and --include)")

    coverage = round(100.0 * covered / total, 2) if total else 0.0
    share = excluded_total / measured
    print(f"{label} line coverage: {coverage:.2f}% ({covered}/{total} lines in {len(included)} files)")
    print(f"excluded: {excluded_total} lines in {len(excluded)} files "
          f"({100.0 * share:.2f}% of {measured} measured lines, budget {100.0 * EXCLUSION_BUDGET:.0f}%)")

    if share > EXCLUSION_BUDGET:
        failures.append(f"excluded lines ({excluded_total}) exceed {100.0 * EXCLUSION_BUDGET:.0f}% "
                        f"of measured lines ({measured})")
    threshold = round(threshold, 2)
    if coverage < threshold:
        failures.append(f"{label} line coverage {coverage:.2f}% is below the threshold {threshold:.2f}%")
        print(f"Least-covered {label} files:")
        for f in least_covered(included):
            print(f"  {f.percent:6.2f}%  {f.path} ({f.covered}/{f.total} lines, {f.missed} missed)")

    if baseline_path is not None:
        data = load_baseline(baseline_path) if (baseline_path.exists() or not update_baseline) else {}
        recorded = round(_baseline_value(data, kind, baseline_path), 2)
        if coverage < recorded:
            message = (f"{label} line coverage {coverage:.2f}% is lower than the recorded "
                       f"baseline {recorded:.2f}% ({baseline_path})")
            if update_baseline:
                print(f"warning: {message}; baseline left unchanged")
            else:
                failures.append(message)
        if update_baseline and not failures:
            if coverage > recorded:
                data[kind] = coverage
                try:
                    baseline_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n",
                                             encoding="utf-8")
                except OSError as error:
                    raise ToolError(f"cannot write baseline {baseline_path}: {error}") from error
                print(f"baseline {kind} raised from {recorded:.2f}% to {coverage:.2f}% in {baseline_path}")
            else:
                print(f"baseline {kind} stays at {recorded:.2f}%")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return EXIT_FAIL
    print(f"{label} coverage gate passed")
    return EXIT_OK


def build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root used to relativize report paths (default: %(default)s)")
    common.add_argument("--threshold", type=float, default=95.0,
                        help="minimum line coverage in percent (default: %(default)s)")
    common.add_argument("--exclusions", type=Path, metavar="FILE",
                        help="exclusion list, e.g. tools/coverage-exclusions.txt")
    common.add_argument("--baseline", type=Path, metavar="FILE",
                        help='ratchet file such as tools/coverage-baseline.json ({"c": .., "lua": ..})')
    common.add_argument("--update-baseline", action="store_true",
                        help="raise the baseline to the current value if the gate passes")
    common.add_argument("--include", action="append", default=[], metavar="PREFIX",
                        help="only measure files whose path contains PREFIX (repeatable)")

    parser = argparse.ArgumentParser(description="Enforce line-coverage gates for C and Lua.")
    sub = parser.add_subparsers(dest="kind", required=True, metavar="{c,lua}")
    c_parser = sub.add_parser("c", parents=[common], help="gate C coverage from gcovr --json-summary")
    c_parser.add_argument("--gcovr-json", type=Path, required=True, metavar="FILE")
    lua_parser = sub.add_parser("lua", parents=[common], help="gate Lua coverage from a LuaCov report")
    lua_parser.add_argument("--luacov-report", type=Path, required=True, metavar="FILE")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    try:
        args = parser.parse_args(argv)
        if args.update_baseline and args.baseline is None:
            parser.error("--update-baseline requires --baseline")
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    try:
        exclusions, problems = [], []
        if args.exclusions is not None:
            exclusions, problems = parse_exclusions(read_text(args.exclusions), str(args.exclusions))
        if args.kind == "c":
            files = load_gcovr_json(read_text(args.gcovr_json), args.root)
        else:
            files = parse_luacov_report(read_text(args.luacov_report), args.root)
        return run(args.kind, files, exclusions=exclusions, exclusion_problems=problems,
                   threshold=args.threshold, include=args.include,
                   baseline_path=args.baseline, update_baseline=args.update_baseline)
    except ToolError as error:
        print(f"coverage_gate: error: {error}", file=sys.stderr)
        return EXIT_ERROR


if __name__ == "__main__":
    sys.exit(main())
