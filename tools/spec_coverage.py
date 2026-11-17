# SPDX-License-Identifier: MIT
"""Spec scenario coverage check.

Collects every scenario ("#### Scenario: <name>") from openspec/specs/**/spec.md
and openspec/changes/*/specs/**/spec.md (the archive is excluded). The
capability of a spec is its directory relative to the specs/ folder, for
example "game-loop" or "identity/user-auth".

A scenario is covered when some file below tests/ contains the tag

    spec: <capability>: <scenario name>

Runs of whitespace are collapsed before matching, and the scenario name must
match exactly: the tag must end the line or be followed by punctuation such
as a comment terminator or a closing quote, so "spec: x: Low rate" does not
cover the scenario "Low rate limit" (or vice versa).

Exit codes: 0 all covered, 1 uncovered scenarios, 2 usage or I/O error.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tool_common import (EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError,
                         rel_posix, walk_files)

_SCENARIO_RE = re.compile(r"^####\s+Scenario:\s*(.*?)\s*$")
_SECTION_RE = re.compile(r"^##\s+(.*?)\s*$")
_FENCE_RE = re.compile(r"^\s*(```|~~~)")
_TAG_MARKER = "spec:"


@dataclass(frozen=True, order=True)
class Scenario:
    capability: str
    name: str

    @property
    def tag(self) -> str:
        return f"{self.capability}: {self.name}"


def collapse(text: str) -> str:
    """Collapses runs of whitespace into single spaces and strips the ends."""
    return " ".join(text.split())


def parse_scenarios(text: str) -> list[str]:
    """Returns the scenario names of one spec.md, in order. Scenarios inside
    fenced code blocks and below a "## REMOVED ..." section are ignored."""
    names = []
    in_fence = False
    removed = False
    for line in text.splitlines():
        if _FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        section = _SECTION_RE.match(line)
        if section:
            removed = section.group(1).upper().startswith("REMOVED")
            continue
        match = _SCENARIO_RE.match(line)
        if match and not removed:
            name = collapse(match.group(1))
            if name:
                names.append(name)
    return names


def spec_files(root: Path) -> list[tuple[str, Path]]:
    """(capability, path) for every spec.md in the main specs and in the
    active (non-archived) changes."""
    openspec = root / "openspec"
    if not openspec.is_dir():
        raise ToolError(f"no openspec/ directory below {root}")
    bases = []
    if (openspec / "specs").is_dir():
        bases.append(openspec / "specs")
    changes = openspec / "changes"
    if changes.is_dir():
        for change in sorted(changes.iterdir()):
            if change.is_dir() and change.name != "archive" and (change / "specs").is_dir():
                bases.append(change / "specs")
    result = []
    for base in bases:
        for path in sorted(base.rglob("spec.md")):
            capability = path.parent.relative_to(base).as_posix()
            if capability != ".":
                result.append((capability, path))
    return result


def collect_scenarios(root: Path) -> dict[Scenario, list[str]]:
    """Every scenario mapped to the spec files that define it (a scenario in
    both a change and the main specs is one scenario)."""
    scenarios: dict[Scenario, list[str]] = {}
    for capability, path in spec_files(root):
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            raise ToolError(f"cannot read {path}: {error}") from error
        for name in parse_scenarios(text):
            scenarios.setdefault(Scenario(capability, name), []).append(rel_posix(path, root))
    return scenarios


def collect_tags(tests_dir: Path) -> list[str]:
    """The collapsed text following each "spec:" marker in any file below
    tests_dir (line by line)."""
    tags = []
    if not tests_dir.is_dir():
        return tags
    marker = _TAG_MARKER.encode()
    for path in walk_files(tests_dir):
        try:
            data = path.read_bytes()
        except OSError as error:
            raise ToolError(f"cannot read {path}: {error}") from error
        if marker not in data:
            continue
        for line in data.decode("utf-8", errors="replace").splitlines():
            if _TAG_MARKER not in line:
                continue
            line = collapse(line)
            start = line.find(_TAG_MARKER)
            while start >= 0:
                tags.append(line[start + len(_TAG_MARKER):].lstrip())
                start = line.find(_TAG_MARKER, start + 1)
    return tags


def _is_word(char: str) -> bool:
    return char.isalnum() or char == "_"


def tag_matches(tag_text: str, wanted: str) -> bool:
    """True if tag_text (the text after "spec:") starts with exactly the
    wanted "<capability>: <name>" followed by a boundary."""
    if not tag_text.startswith(wanted):
        return False
    rest = tag_text[len(wanted):]
    if not rest:
        return True
    if _is_word(rest[0]):
        return False
    return not (rest[0] == " " and len(rest) > 1 and _is_word(rest[1]))


def check(root: Path) -> tuple[list[Scenario], list[Scenario]]:
    """Returns (covered, uncovered), both sorted."""
    scenarios = collect_scenarios(root)
    tags = collect_tags(root / "tests")
    covered, uncovered = [], []
    for scenario in sorted(scenarios):
        wanted = scenario.tag
        if any(tag_matches(tag, wanted) for tag in tags):
            covered.append(scenario)
        else:
            uncovered.append(scenario)
    return covered, uncovered


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Fail if an OpenSpec scenario is not referenced by any test "
                    "via a 'spec: <capability>: <scenario name>' tag.")
    parser.add_argument("--root", type=Path, default=REPO_ROOT,
                        help="repository root (default: %(default)s)")
    parser.add_argument("--json", type=Path, metavar="FILE",
                        help='also write {"covered": [...], "uncovered": [...]} to FILE')
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    root = args.root
    try:
        covered, uncovered = check(root)
        if args.json:
            report = {"covered": [s.tag for s in covered],
                      "uncovered": [s.tag for s in uncovered]}
            try:
                args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            except OSError as error:
                raise ToolError(f"cannot write {args.json}: {error}") from error
    except ToolError as error:
        print(f"spec_coverage: error: {error}", file=sys.stderr)
        return EXIT_ERROR

    total = len(covered) + len(uncovered)
    if uncovered:
        print(f"Scenarios without a test tag 'spec: <capability>: <scenario name>' ({len(uncovered)}):")
        for scenario in uncovered:
            print(f"  {scenario.tag}")
    print(f"spec coverage: {len(covered)}/{total} scenarios covered")
    return EXIT_FAIL if uncovered else EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
