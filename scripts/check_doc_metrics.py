#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Fail the build when documented metrics drift from the generated evidence.

The ``.rst`` pages cite coverage through the ``|cov-*|`` substitutions defined
in ``docs/conf.py``, which read ``coverage_summary.json`` directly, so they
cannot go stale.  Two things still can:

1. ``README.md`` is Markdown and has no substitution mechanism, so its coverage
   figures are transcribed by hand.
2. The ``NOCOVER(DEFENCE_IN_DEPTH)`` annotation count is a property of the
   source tree, not of any artefact.

Both were found stale during a review pass (89% vs a measured 90.27%; "60
annotations" vs an actual 123).  This script checks them against ground truth
so the same drift fails CI instead of reaching a reader.

Usage:  python3 scripts/check_doc_metrics.py
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SUMMARY = REPO / "docs" / "assurance" / "evidence" / "coverage_summary.json"
README = REPO / "README.md"
CPP_DIRS = ("cpp",)

# Tolerance in percentage points.  Coverage moves slightly with every test
# added; prose is allowed to round, but not to be a full point wrong.
TOLERANCE_PP = 0.5


def nocover_count() -> int:
    """Count DEFENCE_IN_DEPTH annotation sites across the C++ tree."""
    pattern = re.compile(r"NOCOVER(?:_BEGIN)?\(DEFENCE_IN_DEPTH\)")
    total = 0
    for directory in CPP_DIRS:
        for path in (REPO / directory).rglob("*"):
            if path.suffix in {".cpp", ".hpp"} and path.is_file():
                total += len(pattern.findall(path.read_text(encoding="utf-8")))
    return total


def check() -> list[str]:
    """Return a list of drift descriptions; empty means documentation is current."""
    errors: list[str] = []

    if not SUMMARY.exists():
        return [f"{SUMMARY.relative_to(REPO)} missing — run `task coverage` first"]

    totals = json.loads(SUMMARY.read_text())["totals"]
    readme = README.read_text(encoding="utf-8")

    # 1. README line-coverage figure must match the measured value.
    measured_line = totals["lines"]["percent"]
    match = re.search(r"C\+\+ line coverage is ([0-9]+(?:\.[0-9]+)?)%", readme)
    if match is None:
        errors.append(
            "README.md: could not find the 'C++ line coverage is N%' sentence; "
            "update this checker if the wording changed deliberately"
        )
    else:
        stated = float(match.group(1))
        if abs(stated - measured_line) > TOLERANCE_PP:
            errors.append(
                f"README.md states C++ line coverage {stated}%, but "
                f"{SUMMARY.relative_to(REPO)} measures {measured_line:.2f}% "
                f"(tolerance {TOLERANCE_PP} pp)"
            )

    # 2. README NOCOVER count must match the source tree.
    measured_nocover = nocover_count()
    # Emphasis around the figure is presentation, not data: match the claim
    # whether or not it is bold, so rewording the README does not fail a gate
    # that exists to catch the *number* drifting from the source tree.
    match = re.search(r"(?:\*\*)?([0-9]+) in-source\s*\n?`?(?:\*\*)?`?NOCOVER", readme)
    if match is None:
        errors.append(
            "README.md: could not find the 'N in-source NOCOVER' claim; "
            "update this checker if the wording changed deliberately"
        )
    elif int(match.group(1)) != measured_nocover:
        errors.append(
            f"README.md claims {match.group(1)} NOCOVER(DEFENCE_IN_DEPTH) "
            f"annotations, but the source tree has {measured_nocover}"
        )

    # 3. No .rst page may hard-code a coverage percentage; they must use the
    #    |cov-*| substitutions so a regenerated artefact updates them.
    stale = re.compile(r"(?<![\d.])(9[0-9]|8[0-9])\.[0-9]{2}%\s*(region|line)\b")
    for path in sorted(REPO.glob("docs/**/*.rst")):
        if "_build" in path.parts:
            continue
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if stale.search(line):
                errors.append(
                    f"{path.relative_to(REPO)}:{lineno}: hard-coded coverage "
                    f"percentage — use the |cov-region| / |cov-line| "
                    f"substitutions from docs/conf.py instead"
                )

    return errors


def main() -> None:
    """Report any documented-metric drift and exit non-zero if there is some."""
    errors = check()
    if errors:
        print("Documented metrics do not match generated evidence:\n")
        for e in errors:
            print(f"  {e}")
        sys.exit(1)
    print("check-doc-metrics: OK (README coverage + NOCOVER count match evidence;")
    print("                       no hard-coded coverage percentages in docs/*.rst)")


if __name__ == "__main__":
    main()
