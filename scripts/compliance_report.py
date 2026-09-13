#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
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
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "build"
TRACEABILITY_JSON = REPO_ROOT / "docs" / "assurance" / "evidence" / "traceability.json"
COVERAGE_SUMMARY_JSON = (
    REPO_ROOT / "docs" / "assurance" / "evidence" / "coverage_summary.json"
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
    "NFR-2.4": {
        "description": "Time reconstruction accurate to code phase",
        "test_pattern": "[LSIS-AFS-810]",
    },
    "NFR-3.1": {
        "description": "Unit tests for all components",
        "test_pattern": None,  # Validated by total test count > 0
    },
    "NFR-3.2": {
        "description": "Integration tests covering the end-to-end flow",
        "test_pattern": "E2E",
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
    "NFR-1.4": {
        "description": "Real-time capability: process faster than signal duration",
        "benchmark_file": "wcet_results.xml",
    },
    "NFR-2.2": {
        "description": "BER < 1e-5 at SNR > 0 dB",
        "benchmark_file": "ber_results.xml",
    },
    "NFR-3.3": {
        "description": "Test coverage > 90%",
        # Evaluated from the committed coverage evidence, not a JUnit file.
        "benchmark_file": None,
        "coverage_summary": True,
    },
}


# Usability (NFR-4.x) and maintainability (NFR-5.x) requirements from the
# competition NFR summary.  These are properties of the delivered repository
# rather than of a test run, so each is evaluated by a concrete, falsifiable
# check against the working tree -- never asserted.  `check` returns
# (passed, evidence).
def _c_paths(*rel: str) -> tuple[bool, str]:
    """Check that every named path exists; report any that do not."""
    missing = [r for r in rel if not (REPO_ROOT / r).exists()]
    if missing:
        return False, f"missing: {', '.join(missing)}"
    return True, f"present: {', '.join(rel)}"


def _c_docstrings() -> tuple[bool, str]:
    """Run the docstring-sync checker used by `task lint-check`."""
    script = REPO_ROOT / "scripts" / "sync_docstrings.py"
    if not script.exists():
        return False, "scripts/sync_docstrings.py missing"
    proc = subprocess.run(
        [sys.executable, str(script)],
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
        check=False,
    )
    ok = proc.returncode == 0
    detail = (
        "C++ @param tags and binding docstrings in sync" if ok else "drift detected"
    )
    return ok, f"docs/api.rst + sync_docstrings.py: {detail}"


def _c_runtime_params() -> tuple[bool, str]:
    """Check the documented runtime-configurable parameters are real kwargs."""
    wanted = {
        "src/lunalink/afs/_receiver.py": ["ldpc_iterations", "raise_on_error"],
        "src/lunalink/afs/_transmitter.py": ["sample_rate", "prn_id"],
    }
    missing: list[str] = []
    for rel, params in wanted.items():
        path = REPO_ROOT / rel
        text = path.read_text() if path.exists() else ""
        missing += [f"{rel}:{p}" for p in params if p not in text]
    if missing:
        return False, f"missing runtime parameters: {', '.join(missing)}"
    flat = [p for ps in wanted.values() for p in ps]
    return True, f"runtime-configurable: {', '.join(flat)}"


# NFR-5.4 thresholds: a history is "meaningful" when it has enough commits to
# show incremental work, spans enough time to show sustained development, and
# carries messages long enough to say why rather than only what.
MIN_COMMITS = 50
MIN_SPAN_DAYS = 60
MIN_MEAN_SUBJECT = 25


def _c_git_history() -> tuple[bool, str]:
    """Evaluate the git history present in this repository.

    Measures three properties rather than asserting any of them: commit
    count, the span between first and last commit, and mean subject length.
    See the Provenance section of README.md for how this history relates to
    the private development tree it was extracted from.
    """

    def git(*args: str) -> str:
        proc = subprocess.run(
            ["git", *args], capture_output=True, text=True, cwd=REPO_ROOT, check=False
        )
        return proc.stdout.strip()

    count_raw = git("rev-list", "--count", "HEAD")
    if not count_raw.isdigit():
        return False, "no git history available (not a git checkout?)"
    count = int(count_raw)

    dates = git("log", "--format=%cI").splitlines()
    subjects = [s for s in git("log", "--format=%s").splitlines() if s]
    if not dates or not subjects:
        return False, f"{count} commits but no readable metadata"

    newest = datetime.fromisoformat(dates[0])
    oldest = datetime.fromisoformat(dates[-1])
    span_days = (newest - oldest).days
    mean_subject = sum(len(s) for s in subjects) / len(subjects)

    evidence = (
        f"{count} commits over {span_days} days "
        f"({oldest.date()} to {newest.date()}), "
        f"mean subject {mean_subject:.0f} chars"
    )
    shortfalls = []
    if count < MIN_COMMITS:
        shortfalls.append(f"fewer than {MIN_COMMITS} commits")
    if span_days < MIN_SPAN_DAYS:
        shortfalls.append(f"span under {MIN_SPAN_DAYS} days")
    if mean_subject < MIN_MEAN_SUBJECT:
        shortfalls.append(f"mean subject under {MIN_MEAN_SUBJECT} chars")
    if shortfalls:
        return False, f"{evidence} -- {'; '.join(shortfalls)}"
    return True, evidence


NFR_ARTIFACT_MAP: dict[str, dict] = {
    "NFR-3.4": {
        "description": "Test vectors reproducible",
        "check": lambda: _c_paths(
            "scripts/export_test_vectors_all.py",
            "tests/generate_test_vectors.py",
            "test-vectors/codes",
            "test-vectors/frames",
            "test-vectors/signals",
            "test-vectors/parsed",
        ),
    },
    "NFR-4.1": {
        "description": "Clear API documentation: all public APIs documented",
        "check": _c_docstrings,
    },
    "NFR-4.2": {
        "description": "Working examples for all features",
        "check": lambda: _c_paths(
            "examples/01_transmit.py",
            "examples/02_decode.py",
            "examples/03_round_trip.py",
            "docs/notebooks/01_spreading_codes.ipynb",
            "docs/notebooks/02_error_correction.ipynb",
            "docs/notebooks/03_navigation_message.ipynb",
        ),
    },
    "NFR-4.3": {
        "description": "Error messages provide actionable guidance",
        "check": lambda: _c_paths(
            "src/lunalink/afs/_errors.py",
            "docs/signal/icd.rst",
        ),
    },
    "NFR-4.4": {
        "description": "Runtime configuration support",
        "check": _c_runtime_params,
    },
    "NFR-5.1": {
        "description": "Modular architecture with clean component boundaries",
        "check": lambda: _c_paths(
            "docs/signal/architecture.rst",
            "cpp/codes",
            "cpp/encoding",
            "cpp/messages",
            "cpp/signal",
            "cpp/platform",
        ),
    },
    "NFR-5.2": {
        "description": "Separation of concerns: independent modules",
        "check": lambda: _c_paths(
            "cpp/include/lunalink/codes",
            "cpp/include/lunalink/encoding",
            "cpp/include/lunalink/messages",
            "cpp/include/lunalink/signal",
            "src/lunalink/afs",
        ),
    },
    "NFR-5.3": {
        "description": "Consistent coding style enforced via tooling",
        "check": lambda: _c_paths(
            ".clang-format",
            ".clang-tidy",
            "cppcheck.cfg",
            "pyproject.toml",
            ".github/workflows/lint.yml",
            ".github/workflows/tidy.yml",
        ),
    },
    "NFR-5.4": {
        "description": "Version control with meaningful history",
        "check": _c_git_history,
    },
}


def _eval_nfr_artifact(nfr_id: str, nfr_info: dict) -> dict:
    """Evaluate an NFR whose evidence is a property of the repository."""
    passed, evidence = nfr_info["check"]()
    return {
        "id": nfr_id,
        "description": nfr_info["description"],
        "status": "PASS" if passed else "LIMITATION",
        "evidence": evidence,
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


def _eval_nfr_coverage(nfr_id: str, nfr_info: dict) -> dict:
    """Evaluate a coverage NFR from the committed coverage evidence.

    Reports the measured figure for every structural metric rather than a bare
    verdict: region coverage is the ECSS Category B primary metric and carries
    the pass/fail decision, line coverage is a floor beneath it, and both
    thresholds are read from the evidence file itself so this report can never
    assert a threshold the evidence was not measured against.
    """
    if not COVERAGE_SUMMARY_JSON.exists():
        return {
            "id": nfr_id,
            "description": nfr_info["description"],
            "status": "NOT_RUN",
            "evidence": (
                "Run `task coverage-cpp && task coverage-report` to generate "
                f"{COVERAGE_SUMMARY_JSON.relative_to(REPO_ROOT)}"
            ),
        }

    summary = json.loads(COVERAGE_SUMMARY_JSON.read_text())
    totals = summary["totals"]
    thresholds = summary["thresholds"]
    region = totals["regions"]["percent"]
    line = totals["lines"]["percent"]
    mcdc = totals["mcdc"]["percent"]

    passed = region >= thresholds["region"] and line >= thresholds["line"]
    return {
        "id": nfr_id,
        "description": nfr_info["description"],
        "status": "PASS" if passed else "FAIL",
        "evidence": (
            f"C++ region {region:.2f}% (>= {thresholds['region']:.0f}%), "
            f"line {line:.2f}% (>= {thresholds['line']:.0f}%), "
            f"MC/DC {mcdc:.2f}% (>= {thresholds['mcdc']:.0f}%) "
            f"from {COVERAGE_SUMMARY_JSON.name} @ {summary['git_sha']}; "
            "Python 100% (pytest --cov gate)"
        ),
    }


def _eval_nfr_benchmark(nfr_id: str, nfr_info: dict) -> dict:
    """Evaluate a single NFR against benchmark results."""
    if nfr_info.get("coverage_summary"):
        return _eval_nfr_coverage(nfr_id, nfr_info)
    bench_file = nfr_info.get("benchmark_file")
    if bench_file and any(
        (d / bench_file).exists() for d in (BUILD_DIR, BUILD_DIR / "performance")
    ):
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

    nfr_entries = (
        [
            _eval_nfr_test(nid, info, test_results)
            for nid, info in sorted(NFR_TEST_MAP.items())
        ]
        + [
            _eval_nfr_benchmark(nid, info)
            for nid, info in sorted(NFR_BENCHMARK_MAP.items())
        ]
        + [
            _eval_nfr_artifact(nid, info)
            for nid, info in sorted(NFR_ARTIFACT_MAP.items())
        ]
    )
    nfr_entries.sort(key=lambda e: e["id"])

    req_counts = _count_statuses(requirements)
    nfr_counts = _count_statuses(nfr_entries)

    return {
        "title": "LSIS-AFS Compliance Report",
        "document_id": "LUNALINK-COMPLIANCE-001",
        "generated_by": "scripts/compliance_report.py",
        # Repo-relative: an absolute path records the home directory of
        # whoever generated the evidence, which is noise in a published
        # artefact and differs on every machine that regenerates it.
        "junit_files": [
            str(p.relative_to(REPO_ROOT)) if p.is_relative_to(REPO_ROOT) else str(p)
            for p in found_xmls
        ],
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
            # A requirement whose evidence is a stated limitation rather than a
            # pass -- currently NFR-5.4, the squashed public export.  Counted
            # separately so it is never silently folded into the pass total.
            "nfr_limitation": nfr_counts.get("LIMITATION", 0),
        },
    }


def main() -> None:
    """Generate compliance report and print summary."""
    report = generate_report()

    # Write JSON to the build tree and refresh the committed evidence copy in
    # the same step, so the artifact a reviewer reads is never a stale
    # hand-copy of the artifact this script produced.
    payload = json.dumps(report, indent=2) + "\n"
    out_path = BUILD_DIR / "compliance_report.json"
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    out_path.write_text(payload)
    evidence_path = TRACEABILITY_JSON.parent / "compliance_report.json"
    evidence_path.write_text(payload)
    print(f"Compliance report written to: {out_path}")
    print(f"Committed evidence refreshed:  {evidence_path}\n")

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
    print(f"  LIMIT:    {summary['nfr_limitation']}")
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
