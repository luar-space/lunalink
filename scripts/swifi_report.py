#!/usr/bin/env python3
"""Parse JUnit XML from SWIFI test run and produce a JSON evidence artifact.

Also reconciles scenario count against fault_coverage.rst.

Usage:
    python3 scripts/swifi_report.py \
        --xml build/swifi/swifi_results.xml \
        --fault-coverage docs/assurance/private/fault_coverage.rst

Output: docs/assurance/private/evidence/swifi_results.json
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


def parse_junit_xml(path: str) -> list[dict]:
    """Parse JUnit XML and extract test case results."""
    tree = ET.parse(path)
    root = tree.getroot()
    scenarios: list[dict] = []

    # JUnit XML can have <testsuites> wrapping <testsuite> or just <testsuite>
    # Catch2 JUnit reporter produces <testsuites><testsuite><testcase>...
    for tc in root.iter("testcase"):
        name = tc.get("name", "unknown")
        classname = tc.get("classname", "")
        time_s = float(tc.get("time", "0"))

        # Determine status
        failure = tc.find("failure")
        error = tc.find("error")
        skipped = tc.find("skipped")

        if failure is not None:
            status = "FAIL"
        elif error is not None:
            status = "ERROR"
        elif skipped is not None:
            status = "SKIP"
        else:
            status = "PASS"

        # Build full name
        full_name = f"{classname}/{name}" if classname and classname != name else name

        scenarios.append(
            {
                "name": full_name,
                "status": status,
                "time_seconds": round(time_s, 6),
            }
        )

    return scenarios


def count_fi_entries(fault_coverage_path: str) -> int:
    """Count FI- entries in fault_coverage.rst."""
    if not os.path.isfile(fault_coverage_path):
        return -1

    count = 0
    with open(fault_coverage_path) as f:
        for line in f:
            # Count lines that start a new FI- entry in the list-table
            # Pattern: "   * - FI-NN"
            if re.search(r"\*\s+-\s+FI-\d+", line):
                count += 1
    return count


def get_git_sha() -> str:
    """Get the current HEAD SHA."""
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "HEAD"], stderr=subprocess.DEVNULL
            )
            .decode()
            .strip()
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Generate SWIFI evidence artifact from JUnit XML"
    )
    parser.add_argument(
        "--xml",
        required=True,
        help="Path to JUnit XML from SWIFI test run",
    )
    parser.add_argument(
        "--fault-coverage",
        default="docs/assurance/private/fault_coverage.rst",
        help="Path to fault_coverage.rst for count reconciliation",
    )
    parser.add_argument(
        "--output",
        default="docs/assurance/private/evidence/swifi_results.json",
        help="Output JSON path",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.xml):
        print(f"ERROR: JUnit XML not found: {args.xml}", file=sys.stderr)
        sys.exit(1)

    scenarios = parse_junit_xml(args.xml)

    if not scenarios:
        print("WARNING: No test cases found in JUnit XML", file=sys.stderr)

    # Build summary
    total = len(scenarios)
    passed = sum(1 for s in scenarios if s["status"] == "PASS")
    failed = sum(1 for s in scenarios if s["status"] in ("FAIL", "ERROR"))
    pass_rate = round(passed / total * 100.0, 1) if total > 0 else 0.0

    summary = {
        "total": total,
        "passed": passed,
        "failed": failed,
        "pass_rate": pass_rate,
    }

    report = {
        "generated": datetime.datetime.now(datetime.UTC).isoformat(),
        "git_sha": get_git_sha(),
        "scenarios": scenarios,
        "summary": summary,
    }

    # Write output
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(report, f, indent=2)
        f.write("\n")

    print(f"SWIFI report: {args.output}")
    print(f"  Total: {total}, Passed: {passed}, Failed: {failed}")
    print(f"  Pass rate: {pass_rate}%")

    # Reconcile against fault_coverage.rst
    # We count only SWIFI-tagged tests (name contains "SWIFI" or "FI-") for
    # reconciliation, since the JUnit XML may contain non-fault-injection tests
    # in the same binary.
    swifi_scenarios = [
        s for s in scenarios if "SWIFI" in s["name"] or "FI-" in s["name"]
    ]
    swifi_count = (
        len(swifi_scenarios) if swifi_scenarios else total
    )  # fall back to total if no tags

    fi_count = count_fi_entries(args.fault_coverage)
    if fi_count < 0:
        print(f"  WARNING: {args.fault_coverage} not found — skipping reconciliation")
    elif fi_count != swifi_count:
        print(
            f"  WARNING: fault_coverage.rst has {fi_count} FI- entries "
            f"but JUnit XML has {swifi_count} SWIFI test cases (of {total} total)"
        )
        if fi_count > swifi_count:
            print("  fault_coverage.rst may document scenarios not yet automated.")
        else:
            print(
                "  JUnit XML may contain tests not yet documented"
                " in fault_coverage.rst."
            )
        print("  Review manually.")
    else:
        print(
            f"  Reconciliation: {fi_count} FI- entries match"
            f" {swifi_count} SWIFI test cases: OK"
        )

    if failed > 0:
        print(f"\nERROR: {failed} test(s) failed", file=sys.stderr)
        for s in scenarios:
            if s["status"] in ("FAIL", "ERROR"):
                print(f"  FAIL: {s['name']}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
