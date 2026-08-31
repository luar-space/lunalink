#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
r"""Compute effective C++ coverage excluding NOCOVER-marked lines.

Parses llvm-cov LCOV export and cross-references source-level NOCOVER
markers to produce an "effective coverage" metric — the percentage of
*coverable* lines that are actually covered.

Markers
-------
Single line:   ``// NOCOVER(CATEGORY): reason``
Block:         ``// NOCOVER_BEGIN(CATEGORY): reason`` … ``// NOCOVER_END``

Any line that is both uncovered (execution count 0 in LCOV data) and
inside a NOCOVER region is excluded from the denominator.  An uncovered
line that is NOT marked is an error — this is the gate that proves
nothing slipped through.

Exit codes
----------
0  — effective coverage meets threshold (default 100%)
1  — unmarked uncovered lines found, or effective coverage below threshold
2  — usage / file error

Usage::

    python3 scripts/effective_coverage.py \\
        --lcov  build/coverage-oss/coverage.lcov \\
        --src   cpp/ \\
        [--threshold 100] [--json build/coverage-oss/effective.json]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

# ---------------------------------------------------------------------------
# Source marker parsing
# ---------------------------------------------------------------------------

_RE_NOCOVER_SINGLE = re.compile(r"//\s*NOCOVER\(")
_RE_NOCOVER_BEGIN = re.compile(r"//\s*NOCOVER_BEGIN\(")
_RE_NOCOVER_END = re.compile(r"//\s*NOCOVER_END\b")


def parse_nocover_lines(path: Path) -> set[int]:
    """Return the set of 1-based line numbers inside NOCOVER regions.

    A single-line ``// NOCOVER(...)`` marker also covers the immediately
    following line if it is a closing brace (``}``), since the coverage
    tool marks both the statement and its closing brace as uncovered.
    """
    excluded: set[int] = set()
    in_block = False
    lines: list[str] = []
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()
    for i, line in enumerate(lines):
        lineno = i + 1
        if _RE_NOCOVER_BEGIN.search(line):
            in_block = True
        if in_block or _RE_NOCOVER_SINGLE.search(line):
            excluded.add(lineno)
            # Auto-extend: a single-line NOCOVER covers subsequent lines in
            # the same block until the next line that isn't part of the same
            # uncoverable statement (closing brace, blank, or new statement).
            if not in_block:
                for j in range(i + 1, min(i + 4, len(lines))):
                    nxt = lines[j].strip()
                    if not nxt or nxt.startswith("//") or nxt.startswith("/*"):
                        break
                    excluded.add(j + 1)
                    if nxt.startswith("}"):
                        break
        if _RE_NOCOVER_END.search(line):
            in_block = False
    return excluded


# ---------------------------------------------------------------------------
# LCOV parsing
# ---------------------------------------------------------------------------


@dataclass
class FileCoverage:
    """Per-file coverage data from LCOV."""

    total_lines: int = 0
    covered_lines: int = 0
    uncovered_lines: int = 0
    excluded_lines: int = 0
    unmarked_uncovered: list[int] = field(default_factory=list)


def parse_lcov(  # noqa: C901
    lcov_path: Path, src_root: Path, path_prefix: str
) -> dict[str, FileCoverage]:
    """Parse LCOV data and cross-reference with NOCOVER markers."""
    results: dict[str, FileCoverage] = {}
    current_file: str | None = None
    line_data: list[tuple[int, int]] = []

    def _flush():
        nonlocal current_file, line_data
        if current_file is None:
            return
        # Resolve source path: strip build-dir prefix, find in src_root
        rel = current_file
        if path_prefix and rel.startswith(path_prefix):
            rel = rel[len(path_prefix) :]
        rel = rel.lstrip("/")
        src_path = src_root.parent / rel  # cpp/foo.cpp → <root>/cpp/foo.cpp
        if not src_path.exists():
            # Try relative to cwd
            src_path = Path(rel)

        fc = FileCoverage()
        nocover = parse_nocover_lines(src_path) if src_path.exists() else set()

        for lineno, count in line_data:
            fc.total_lines += 1
            if count > 0:
                fc.covered_lines += 1
            else:
                if lineno in nocover:
                    fc.excluded_lines += 1
                else:
                    fc.uncovered_lines += 1
                    fc.unmarked_uncovered.append(lineno)

        results[rel] = fc
        current_file = None
        line_data = []

    with open(lcov_path, encoding="utf-8") as fh:
        for raw_line in fh:
            line = raw_line.rstrip()
            if line.startswith("SF:"):
                _flush()
                current_file = line[3:]
                line_data = []
            elif line.startswith("DA:"):
                parts = line[3:].split(",")
                if len(parts) >= 2:
                    lineno = int(parts[0])
                    count = int(parts[1])
                    line_data.append((lineno, count))
            elif line == "end_of_record":
                _flush()
    _flush()
    return results


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def print_report(
    results: dict[str, FileCoverage],
    threshold: float,
    json_path: Path | None,
) -> bool:
    """Print summary and return True if effective coverage meets threshold."""
    total = covered = excluded = unmarked = 0
    unmarked_details: dict[str, list[int]] = {}

    for rel, fc in sorted(results.items()):
        total += fc.total_lines
        covered += fc.covered_lines
        excluded += fc.excluded_lines
        unmarked += len(fc.unmarked_uncovered)
        if fc.unmarked_uncovered:
            unmarked_details[rel] = fc.unmarked_uncovered

    coverable = total - excluded
    raw_pct = (covered / total * 100) if total else 0
    eff_pct = (covered / coverable * 100) if coverable else 0

    print("=" * 72)
    print("Effective Coverage Report")
    print("=" * 72)
    print()

    # Per-file table
    print(f"{'File':<50} {'Lines':>5} {'Cov':>5} {'Excl':>5} {'Miss':>5} {'Eff%':>6}")
    print("-" * 72)
    for rel, fc in sorted(results.items()):
        file_coverable = fc.total_lines - fc.excluded_lines
        file_eff = (
            (fc.covered_lines / file_coverable * 100) if file_coverable else 100.0
        )
        miss = len(fc.unmarked_uncovered)
        marker = " !" if miss else ""
        print(
            f"{rel:<50} {fc.total_lines:>5} {fc.covered_lines:>5} "
            f"{fc.excluded_lines:>5} {miss:>5} {file_eff:>5.1f}%{marker}"
        )
    print("-" * 72)
    print(
        f"{'TOTAL':<50} {total:>5} {covered:>5} "
        f"{excluded:>5} {unmarked:>5} {eff_pct:>5.1f}%"
    )
    print()

    print(f"Raw line coverage:       {covered}/{total} = {raw_pct:.2f}%")
    print(f"Excluded (NOCOVER):      {excluded} lines")
    print(f"Coverable lines:         {coverable}")
    print(f"Effective coverage:      {covered}/{coverable} = {eff_pct:.2f}%")
    print(f"Threshold:               {threshold:.0f}%")
    print()

    if unmarked_details:
        print("ERROR: Uncovered lines WITHOUT NOCOVER marker:")
        for rel, lines in sorted(unmarked_details.items()):
            print(f"  {rel}: lines {', '.join(str(ln) for ln in sorted(lines))}")
        print()

    ok = eff_pct >= threshold and not unmarked_details
    print("PASS" if ok else "FAIL")

    if json_path:
        report = {
            "raw_coverage_pct": round(raw_pct, 2),
            "effective_coverage_pct": round(eff_pct, 2),
            "total_lines": total,
            "covered_lines": covered,
            "excluded_lines": excluded,
            "coverable_lines": coverable,
            "unmarked_uncovered": unmarked,
            "threshold": threshold,
            "pass": ok,
            "files": {
                rel: {
                    "total": fc.total_lines,
                    "covered": fc.covered_lines,
                    "excluded": fc.excluded_lines,
                    "unmarked_uncovered": fc.unmarked_uncovered,
                }
                for rel, fc in sorted(results.items())
            },
        }
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(report, indent=2) + "\n")
        print(f"\nJSON report: {json_path}")

    return ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:  # noqa: C901
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--lcov", required=True, type=Path, help="LCOV file from llvm-cov export"
    )
    parser.add_argument(
        "--src", required=True, type=Path, help="Source root (e.g., cpp/)"
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=100.0,
        help="Minimum effective coverage %% (default: 100)",
    )
    parser.add_argument(
        "--json", type=Path, default=None, help="Output JSON report path"
    )
    parser.add_argument(
        "--path-prefix",
        type=str,
        default="",
        help="Prefix to strip from LCOV SF: paths",
    )
    args = parser.parse_args()

    if not args.lcov.exists():
        print(f"ERROR: LCOV file not found: {args.lcov}", file=sys.stderr)
        return 2
    if not args.src.exists():
        print(f"ERROR: Source root not found: {args.src}", file=sys.stderr)
        return 2

    results = parse_lcov(args.lcov, args.src, args.path_prefix)
    ok = print_report(results, args.threshold, args.json)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
