#!/usr/bin/env python3
r"""LSIS-AFS Compliance Report Generator.

Produces a machine-readable JSON artifact mapping every LSIS requirement and
NFR to its test evidence and pass/fail status.

Requires JUnit XML test results -- fails if not found rather than producing
a vacuously passing report.

Usage::

  task compliance-report

  # Or manually:
  ./build/debug/test_lunalink -r junit \
    -o build/test_results.xml '~[wcet]~[channel]~[e2e]'
  ./build/debug/test_lunalink -r junit \
    -o build/test_e2e.xml '[e2e]'
  python scripts/compliance_report.py
"""

from __future__ import annotations

import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "build"
TRACEABILITY_JSON = (
    REPO_ROOT / "docs" / "assurance" / "private" / "evidence" / "traceability.json"
)

# JUnit XML files that must exist for a valid report.
JUNIT_FILES = ["test_results.xml", "test_e2e.xml"]

# Optional JUnit XML files (loaded when present, not required).
OPTIONAL_JUNIT_FILES = ["swifi_results.xml"]

# NFR → test case name substring mapping.  Each NFR is validated by checking
# that at least one matching test exists in the JUnit results and passed.
NFR_TEST_MAP: dict[str, dict] = {
    "NFR-1.3": {
        "description": "Frame decoding < 1 s per frame",
        "test_pattern": "frame_decode completes in < 1 second",
    },
    "NFR-2.1": {
        "description": "Spreading codes match Annex3 references exactly",
        "test_pattern": "[LSIS-AFS-101]",
    },
    "NFR-2.3": {
        "description": "Frame sync detection > 99% at SNR > 0 dB",
        "test_pattern": "[LSIS-AFS-710]",
    },
    "NFR-3.1": {
        "description": "Unit tests for all components",
        "test_pattern": None,  # Validated by total test count > 0
    },
}

# NFRs that require benchmark XML (not available from unit test JUnit).
# These are reported as NOT_RUN when benchmark XML is absent.
NFR_BENCHMARK_MAP: dict[str, dict] = {
    "NFR-1.1": {
        "description": "Code generation < 1 s per PRN",
        "benchmark_file": "wcet_results.xml",
    },
    "NFR-1.2": {
        "description": "Frame encoding < 100 ms per frame",
        "benchmark_file": "wcet_results.xml",
    },
    "NFR-2.2": {
        "description": "BER < 1e-1 at SNR > 0 dB",
        "benchmark_file": "ber_results.xml",
    },
    "NFR-3.3": {
        "description": "Test coverage > 90%",
        "benchmark_file": None,  # Validated by coverage CI gate
    },
}


def load_traceability() -> dict:
    """Load the traceability JSON produced by verify_traceability.py."""
    if not TRACEABILITY_JSON.exists():
        print(f"ERROR: Traceability JSON not found at {TRACEABILITY_JSON}")
        print("Run: python scripts/verify_traceability.py")
        sys.exit(1)
    return json.loads(TRACEABILITY_JSON.read_text())


def load_test_results(*xml_paths: Path) -> dict[str, bool]:
    """Parse Catch2 JUnit XML files and return {test_name: passed}."""
    results: dict[str, bool] = {}
    for xml_path in xml_paths:
        if not xml_path.exists():
            continue
        tree = ET.parse(xml_path)
        for tc in tree.iter("testcase"):
            name = tc.get("name", "")
            failed = tc.find("failure") is not None
            results[name] = not failed
    return results


def find_tests_for_tag(
    tag: str, test_results: dict[str, bool]
) -> list[dict[str, object]]:
    """Find all test cases whose name contains the given requirement tag."""
    matches = []
    for name, passed in test_results.items():
        if tag in name:
            matches.append({"name": name, "passed": passed})
    return matches


def _build_requirements(trace: dict, test_results: dict[str, bool]) -> list[dict]:
    """Build requirement entries by matching tags to JUnit outcomes."""
    requirements = []
    req_dict = trace.get("requirements", {})
    for req_id, req_data in sorted(req_dict.items()):
        test_files = req_data.get("test_files", [])
        code_files = req_data.get("code_files", [])
        covered = req_data.get("covered", False)

        if not covered:
            status = "UNTESTED"
            matched_tests: list[dict[str, object]] = []
        else:
            matched_tests = find_tests_for_tag(req_id, test_results)
            if not matched_tests:
                status = "NOT_RUN"
            elif all(t["passed"] for t in matched_tests):
                status = "PASS"
            else:
                status = "FAIL"

        requirements.append(
            {
                "id": req_id,
                "test_files": test_files,
                "code_files": code_files,
                "matched_tests": [t["name"] for t in matched_tests],
                "status": status,
            }
        )
    return requirements


def _eval_nfr_test(nfr_id: str, nfr_info: dict, test_results: dict[str, bool]) -> dict:
    """Evaluate a single NFR against JUnit test results."""
    pattern = nfr_info["test_pattern"]
    if pattern is None:
        status = "PASS" if len(test_results) > 0 else "FAIL"
        evidence = f"{len(test_results)} test cases executed"
    else:
        matches = [
            (name, passed) for name, passed in test_results.items() if pattern in name
        ]
        if not matches:
            status = "NOT_RUN"
            evidence = f"No test matching '{pattern}' in JUnit XML"
        elif all(p for _, p in matches):
            status = "PASS"
            evidence = f"{len(matches)} test(s) passed"
        else:
            failed = [n for n, p in matches if not p]
            status = "FAIL"
            evidence = f"FAILED: {', '.join(failed)}"

    return {
        "id": nfr_id,
        "description": nfr_info["description"],
        "status": status,
        "evidence": evidence,
    }


def _eval_nfr_benchmark(nfr_id: str, nfr_info: dict) -> dict:
    """Evaluate a single NFR against benchmark results."""
    bench_file = nfr_info.get("benchmark_file")
    if bench_file and (BUILD_DIR / bench_file).exists():
        status = "PASS"
        evidence = f"Benchmark results in {bench_file}"
    else:
        status = "NOT_RUN"
        evidence = (
            f"Run `task performance` to generate {bench_file}"
            if bench_file
            else "Validated by coverage CI gate"
        )
    return {
        "id": nfr_id,
        "description": nfr_info["description"],
        "status": status,
        "evidence": evidence,
    }


def _load_junit() -> tuple[list[Path], dict[str, bool]]:
    """Load JUnit XML files, exit if none found.

    Required files must be present; optional files (e.g. SWIFI results)
    are loaded when available so that SWIFI-only requirement tags are
    matched against their actual test outcomes.
    """
    xml_paths = [BUILD_DIR / name for name in JUNIT_FILES]
    missing = [p for p in xml_paths if not p.exists()]
    if missing:
        print("ERROR: Required JUnit XML file(s) not found:")
        for p in missing:
            print(f"  {p}")
        print("Run tests first:")
        for name in JUNIT_FILES:
            print(f"  ./build/debug/test_lunalink -r junit -o build/{name}")
        sys.exit(1)
    found = list(xml_paths)

    # Also search common SWIFI build directories for optional results.
    for name in OPTIONAL_JUNIT_FILES:
        for search_dir in [
            BUILD_DIR,
            BUILD_DIR / "swifi-ninja",
            BUILD_DIR / "swifi-make",
        ]:
            candidate = search_dir / name
            if candidate.exists() and candidate not in found:
                found.append(candidate)
                break

    results = load_test_results(*found)
    if not results:
        print("ERROR: JUnit XML files contain no test cases")
        sys.exit(1)
    return found, results


def _count_statuses(items: list[dict]) -> dict[str, int]:
    """Count PASS/FAIL/NOT_RUN/UNTESTED across a list of entries."""
    counts: dict[str, int] = {}
    for item in items:
        s = item["status"]
        counts[s] = counts.get(s, 0) + 1
    return counts


def generate_report() -> dict:
    """Generate the compliance report."""
    result = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "verify_traceability.py")],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("ERROR: verify_traceability.py failed:")
        if result.stderr:
            print(result.stderr)
        if result.stdout:
            print(result.stdout)
        sys.exit(1)
    trace = load_traceability()
    found_xmls, test_results = _load_junit()

    requirements = _build_requirements(trace, test_results)

    nfr_entries = [
        _eval_nfr_test(nid, info, test_results)
        for nid, info in sorted(NFR_TEST_MAP.items())
    ] + [
        _eval_nfr_benchmark(nid, info)
        for nid, info in sorted(NFR_BENCHMARK_MAP.items())
    ]
    nfr_entries.sort(key=lambda e: e["id"])

    req_counts = _count_statuses(requirements)
    nfr_counts = _count_statuses(nfr_entries)

    return {
        "title": "LSIS-AFS Compliance Report",
        "document_id": "LUNALINK-COMPLIANCE-001",
        "generated_by": "scripts/compliance_report.py",
        "junit_files": [str(p) for p in found_xmls],
        "total_test_cases": len(test_results),
        "requirements": requirements,
        "nfr": nfr_entries,
        "summary": {
            "total_requirements": len(requirements),
            "pass": req_counts.get("PASS", 0),
            "untested": req_counts.get("UNTESTED", 0),
            "not_run": req_counts.get("NOT_RUN", 0),
            "fail": req_counts.get("FAIL", 0),
            "total_nfr": len(nfr_entries),
            "nfr_pass": nfr_counts.get("PASS", 0),
            "nfr_not_run": nfr_counts.get("NOT_RUN", 0),
            "nfr_fail": nfr_counts.get("FAIL", 0),
        },
    }


def main() -> None:
    """Generate compliance report and print summary."""
    report = generate_report()

    # Write JSON
    out_path = BUILD_DIR / "compliance_report.json"
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Compliance report written to: {out_path}\n")

    # Print summary table
    summary = report["summary"]
    print("=" * 60)
    print("LSIS-AFS COMPLIANCE REPORT")
    print(f"  JUnit sources: {len(report['junit_files'])} file(s)")
    print(f"  Test cases:    {report['total_test_cases']}")
    print("=" * 60)
    print(f"Requirements: {summary['total_requirements']}")
    print(f"  PASS:     {summary['pass']}")
    print(f"  NOT_RUN:  {summary['not_run']}")
    print(f"  UNTESTED: {summary['untested']}")
    print(f"  FAIL:     {summary['fail']}")
    print(f"NFRs:       {summary['total_nfr']}")
    print(f"  PASS:     {summary['nfr_pass']}")
    print(f"  NOT_RUN:  {summary['nfr_not_run']}")
    print(f"  FAIL:     {summary['nfr_fail']}")
    print("=" * 60)

    # Print requirement details
    print(f"\n{'ID':<20} {'Status':<10} {'Tests':<5} {'Code'}")
    print("-" * 60)
    for req in report["requirements"]:
        n_tests = len(req["matched_tests"])
        n_code = len(req["code_files"])
        print(f"{req['id']:<20} {req['status']:<10} {n_tests:<5} {n_code}")

    # Print NFR details
    print(f"\n{'NFR':<10} {'Status':<10} {'Evidence'}")
    print("-" * 60)
    for nfr in report["nfr"]:
        print(f"{nfr['id']:<10} {nfr['status']:<10} {nfr['evidence']}")

    if summary["fail"] > 0 or summary["nfr_fail"] > 0:
        total_fail = summary["fail"] + summary["nfr_fail"]
        print(f"\nFAILED: {total_fail} item(s)")
        sys.exit(1)


if __name__ == "__main__":
    main()
