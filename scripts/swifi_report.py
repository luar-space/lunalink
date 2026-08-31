#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Parse JUnit XML from SWIFI test run and produce a JSON evidence artifact.

Also reconciles scenario count against fault_coverage.rst.

Usage:
    python3 scripts/swifi_report.py \
        --xml build/swifi/swifi_results.xml \
        --fault-coverage docs/assurance/fault_coverage.rst

Output: docs/assurance/evidence/swifi_results.json
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


def doc_fi_set(fault_coverage_path: str) -> set[str]:
    """Extract the set of FI-NN IDs documented in ``fault_coverage.rst``.

    Rows whose status column contains ``(acceptance test`` are excluded —
    those run in the regular test binary (not the SWIFI binary) and so
    should not participate in the SWIFI reconciliation.
    """
    if not os.path.isfile(fault_coverage_path):
        return set()

    with open(fault_coverage_path) as f:
        lines = f.readlines()

    fi_row_re = re.compile(r"\*\s+-\s+(FI-\d+)")
    next_row_re = re.compile(r"^\s*\*\s+-\s+")
    excluded_status_re = re.compile(r"\(acceptance test", re.IGNORECASE)

    result: set[str] = set()
    i = 0
    while i < len(lines):
        m = fi_row_re.search(lines[i])
        if m:
            fi_id = m.group(1)
            body_lines: list[str] = [lines[i]]
            j = i + 1
            while j < len(lines) and not next_row_re.match(lines[j]):
                body_lines.append(lines[j])
                j += 1
            body = " ".join(body_lines)
            if not excluded_status_re.search(body):
                result.add(fi_id)
            i = j
        else:
            i += 1
    return result


def tests_fi_set(test_dirs: list[str]) -> set[str]:
    """Scan SWIFI test source files for ``FI-NN`` comment anchors.

    Each SWIFI test is expected to carry a ``// FI-NN — ...`` header linking
    it back to its fault_coverage.rst entry.  This function collects every
    such ID found in the given directories.
    """
    fi_anchor_re = re.compile(r"FI-\d+")
    result: set[str] = set()
    for root_dir in test_dirs:
        if not os.path.isdir(root_dir):
            continue
        for root, _dirs, files in os.walk(root_dir):
            for name in files:
                if not (name.endswith(".cpp") or name.endswith(".hpp")):
                    continue
                path = os.path.join(root, name)
                try:
                    with open(path) as f:
                        content = f.read()
                except OSError:
                    continue
                result.update(fi_anchor_re.findall(content))
    return result


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


def reconcile_fi_sets(fault_coverage_path: str) -> bool:
    r"""Reconcile FI-NN sets between fault_coverage.rst and SWIFI test sources.

    Each SWIFI test is expected to carry a ``// FI-NN — …`` comment anchor
    pointing at its fault_coverage.rst row; 1:1 counting does not work
    because Catch2 ``SECTION``\ s and ``GENERATE``\ s fan a single
    ``TEST_CASE`` out into multiple JUnit ``<testcase>`` elements.

    Returns True when the doc and test anchor sets are in sync, False
    otherwise (missing anchors, orphan anchors, or missing doc).
    """
    if not os.path.isfile(fault_coverage_path):
        print(f"  WARNING: {fault_coverage_path} not found — skipping reconciliation")
        return False

    doc_set = doc_fi_set(fault_coverage_path)
    test_set = tests_fi_set(["cpp/tests"])
    missing_tests = sorted(doc_set - test_set)
    missing_docs = sorted(test_set - doc_set)

    if not missing_tests and not missing_docs:
        print(
            f"  Reconciliation: {len(doc_set)} FI- entries all anchored in"
            f" SWIFI test sources: OK"
        )
        return True

    if missing_tests:
        print(
            f"  WARNING: {len(missing_tests)} FI- entries in"
            f" {fault_coverage_path} have no test anchor:"
            f" {', '.join(missing_tests)}"
        )
    if missing_docs:
        print(
            f"  WARNING: {len(missing_docs)} FI- anchors in test"
            f" sources have no fault_coverage.rst row:"
            f" {', '.join(missing_docs)}"
        )
    print("  Review manually.")
    return False


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Generate SWIFI evidence artifact from JUnit XML"
    )
    parser.add_argument(
        "--xml",
        action="append",
        help="Path to JUnit XML from SWIFI test run (repeat for multiple binaries)",
    )
    parser.add_argument(
        "--fault-coverage",
        default="docs/assurance/fault_coverage.rst",
        help="Path to fault_coverage.rst for count reconciliation",
    )
    parser.add_argument(
        "--output",
        default="docs/assurance/evidence/swifi_results.json",
        help="Output JSON path",
    )
    parser.add_argument(
        "--reconcile-only",
        action="store_true",
        help="Run the FI-NN doc-vs-test-anchor reconciliation only; skip"
        " JUnit parsing and exit non-zero on any mismatch.  Used by"
        " `task ci-checks` as a fast-fail gate that does not require the"
        " SWIFI binaries to be built.",
    )
    args = parser.parse_args()

    if args.reconcile_only:
        ok = reconcile_fi_sets(args.fault_coverage)
        sys.exit(0 if ok else 1)

    if not args.xml:
        parser.error("--xml is required unless --reconcile-only is set")

    scenarios: list[dict] = []
    for xml_path in args.xml:
        if not os.path.isfile(xml_path):
            print(f"ERROR: JUnit XML not found: {xml_path}", file=sys.stderr)
            sys.exit(1)
        scenarios.extend(parse_junit_xml(xml_path))

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

    reconcile_fi_sets(args.fault_coverage)

    if failed > 0:
        print(f"\nERROR: {failed} test(s) failed", file=sys.stderr)
        for s in scenarios:
            if s["status"] in ("FAIL", "ERROR"):
                print(f"  FAIL: {s['name']}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
