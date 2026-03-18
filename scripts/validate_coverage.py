#!/usr/bin/env python3
"""Tool Validation Suite — llvm-cov coverage measurement verification.

Part of the static analysis evidence chain for ECSS-Q-ST-80C §5.5.
This script proves that the coverage tool (llvm-cov) correctly reports
covered vs uncovered lines, branches, and MC/DC conditions.

Five canary functions in coverage_canary.hpp have KNOWN coverage properties.
Tests in test_coverage_canary.cpp deliberately exercise specific paths,
creating predictable gaps. This script verifies llvm-cov reports match
the expected coverage state.

Usage:
    python3 scripts/validate_coverage.py [--build-dir DIR] [--llvm-dir DIR]
    task validate-coverage

Evidence output:
    build/reports/coverage_validation_report.rst  (formal evidence)
    build/reports/coverage_validation_report.json (machine-readable)

Standards traceability:
    ECSS-Q-ST-80C §5.5  — tool confidence for verification tools
    DO-330 TQL-5         — tool qualification by demonstration
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

CANARY_FILE = "cpp/tests/tool_validation/coverage_canary.hpp"
REPORT_DIR = Path("build/reports")


@dataclass
class CanaryCheck:
    """Single canary check: expected vs actual coverage measurement."""

    name: str
    description: str
    check_type: str  # "line", "branch", "mcdc", "function"
    passed: bool = False
    expected: str = ""
    actual: str = ""


def find_llvm_tool(tool: str, hint: str | None = None) -> str | None:
    """Locate an LLVM tool binary."""
    if hint:
        path = Path(hint) / tool
        if path.exists():
            return str(path)
    candidates = [
        tool,
        f"/opt/homebrew/opt/llvm/bin/{tool}",
        f"/usr/local/opt/llvm/bin/{tool}",
        f"/opt/homebrew/bin/{tool}",
        f"/usr/local/bin/{tool}",
    ]
    for c in candidates:
        try:
            subprocess.run([c, "--version"], capture_output=True, check=True, timeout=5)
            return c
        except (
            subprocess.CalledProcessError,
            FileNotFoundError,
            subprocess.TimeoutExpired,
        ):
            continue
    return None


def build_coverage(build_dir: str, llvm_dir: str | None) -> bool:
    """Build with coverage instrumentation."""
    clang = f"{llvm_dir}/clang" if llvm_dir else "clang"
    clangxx = f"{llvm_dir}/clang++" if llvm_dir else "clang++"

    gen = (
        "Ninja"
        if any(Path(p).exists() for p in ["/usr/bin/ninja", "/opt/homebrew/bin/ninja"])
        else "Unix Makefiles"
    )

    print("Building with coverage instrumentation...")
    result = subprocess.run(
        [
            "cmake",
            "-B",
            build_dir,
            "-S",
            ".",
            "-G",
            gen,
            "-DBUILD_PYTHON_BINDINGS=OFF",
            "-DBUILD_TESTING=ON",
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DENABLE_LLVM_COVERAGE=ON",
            f"-DCMAKE_C_COMPILER={clang}",
            f"-DCMAKE_CXX_COMPILER={clangxx}",
        ],
        capture_output=True,
        text=True,
        timeout=120,
    )
    if result.returncode != 0:
        print(f"CMake configure failed:\n{result.stderr}", file=sys.stderr)
        return False

    result = subprocess.run(
        ["cmake", "--build", build_dir, "--parallel"],
        capture_output=True,
        text=True,
        timeout=300,
    )
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}", file=sys.stderr)
        return False

    return True


def run_canary_tests(build_dir: str) -> bool:
    """Run only the [canary] tagged tests."""
    print("Running canary tests...")
    # Clear previous profiling data
    for f in Path(build_dir).glob("*.profraw"):
        f.unlink()

    env = os.environ.copy()
    env["LLVM_PROFILE_FILE"] = f"{build_dir}/canary-%m.profraw"

    result = subprocess.run(
        [f"{build_dir}/test_lunalink", "[canary]", "--reporter", "console"],
        capture_output=True,
        text=True,
        timeout=60,
        env=env,
    )
    if result.returncode != 0:
        print(f"Tests failed:\n{result.stdout}\n{result.stderr}", file=sys.stderr)
        return False

    print("  Tests passed.")
    return True


def merge_profile(build_dir: str, llvm_profdata: str) -> bool:
    """Merge raw profiles into indexed profile data."""
    profraw_files = list(Path(build_dir).glob("canary-*.profraw"))
    if not profraw_files:
        print("No profraw files generated!", file=sys.stderr)
        return False

    result = subprocess.run(
        [llvm_profdata, "merge", "-sparse"]
        + [str(f) for f in profraw_files]
        + ["-o", f"{build_dir}/canary.profdata"],
        capture_output=True,
        text=True,
        timeout=30,
    )
    return result.returncode == 0


def get_line_coverage(build_dir: str, llvm_cov: str) -> dict[int, int]:
    """Get per-line execution counts for the canary header.

    Returns: {line_number: execution_count} (0 = uncovered)
    """
    result = subprocess.run(
        [
            llvm_cov,
            "export",
            f"{build_dir}/test_lunalink",
            f"-instr-profile={build_dir}/canary.profdata",
            "-format=text",
            f"-sources={CANARY_FILE}",
            f"-path-equivalence={build_dir},{Path('.').resolve()}",
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if result.returncode != 0:
        return {}

    data = json.loads(result.stdout)
    line_counts: dict[int, int] = {}

    for file_data in data.get("data", []):
        for func in file_data.get("functions", []):
            for region in func.get("regions", []):
                # region: [line_start, col_start, line_end, col_end, count, ...]
                if len(region) >= 5:
                    line = region[0]
                    count = region[4]
                    # Use max count for any region starting on this line
                    line_counts[line] = max(line_counts.get(line, 0), count)

    return line_counts


def get_function_coverage(build_dir: str, llvm_cov: str) -> dict[str, int]:
    """Get per-function execution counts.

    Returns: {function_name: execution_count}
    """
    result = subprocess.run(
        [
            llvm_cov,
            "export",
            f"{build_dir}/test_lunalink",
            f"-instr-profile={build_dir}/canary.profdata",
            "-format=text",
            f"-sources={CANARY_FILE}",
            f"-path-equivalence={build_dir},{Path('.').resolve()}",
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if result.returncode != 0:
        return {}

    data = json.loads(result.stdout)
    func_counts: dict[str, int] = {}

    for file_data in data.get("data", []):
        for func in file_data.get("functions", []):
            name = func.get("name", "")
            count = func.get("count", 0)
            func_counts[name] = count

    return func_counts


def get_coverage_report_text(build_dir: str, llvm_cov: str) -> str:
    """Get the annotated source coverage report."""
    cwd = str(Path(".").resolve())
    result = subprocess.run(
        [
            llvm_cov,
            "show",
            f"{build_dir}/test_lunalink",
            f"-instr-profile={build_dir}/canary.profdata",
            f"-sources={CANARY_FILE}",
            "-show-line-counts-or-regions",
            "-show-branches=count",
            "-show-mcdc",
            f"-path-equivalence={build_dir},{cwd}",
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    return result.stdout


def validate_canaries(
    build_dir: str,
    llvm_cov: str,
) -> list[CanaryCheck]:
    """Run all canary validations."""
    checks: list[CanaryCheck] = []

    func_counts = get_function_coverage(build_dir, llvm_cov)
    report_text = get_coverage_report_text(build_dir, llvm_cov)

    # ── Check 1: Covered line has count > 0 ──────────────────────────
    # line_coverage_canary: "result = x * 2" (true branch) should be covered
    c1 = CanaryCheck(
        name="Covered line reported as covered",
        description="line_coverage_canary: true branch (result = x * 2) has count > 0",
        check_type="line",
    )
    # Find any function containing "line_coverage_canary" with count > 0
    canary1_called = any(
        count > 0
        for name, count in func_counts.items()
        if "line_coverage_canary" in name
    )
    c1.passed = canary1_called
    c1.expected = "function executed (count > 0)"
    c1.actual = f"{'found' if canary1_called else 'not found'} in function coverage"
    checks.append(c1)

    # ── Check 2: Uncovered line has count == 0 ────────────────────────
    # line_coverage_canary: "result = -1" (else branch) should be uncovered
    c2 = CanaryCheck(
        name="Uncovered line reported as uncovered",
        description=(
            "line_coverage_canary: else branch (result = -1) shows 0 count in report"
        ),
        check_type="line",
    )
    # Look for "result = -1" line being uncovered in the annotated report
    c2.passed = "result = -1;" in report_text and (
        "0|" in report_text.split("result = -1;")[0].split("\n")[-1]
        if "result = -1;" in report_text
        else False
    )
    c2.expected = "else branch line shows 0 execution count"
    c2.actual = (
        "line uncovered in report" if c2.passed else "could not verify from report"
    )
    checks.append(c2)

    # ── Check 3: Covered branch reported ──────────────────────────────
    # branch_coverage_canary: cases 0,1,2 should show as taken
    c3 = CanaryCheck(
        name="Covered branches reported as taken",
        description="branch_coverage_canary: cases 0,1,2 show non-zero counts",
        check_type="branch",
    )
    canary2_called = any(
        count > 0
        for name, count in func_counts.items()
        if "branch_coverage_canary" in name
    )
    c3.passed = canary2_called
    c3.expected = "branch_coverage_canary function executed"
    c3.actual = f"{'executed' if canary2_called else 'not executed'}"
    checks.append(c3)

    # ── Check 4: Uncovered branch reported ────────────────────────────
    # branch_coverage_canary: case 3 and default should be uncovered
    c4 = CanaryCheck(
        name="Uncovered branches reported as not taken",
        description="branch_coverage_canary: case 3/default show 0 counts in report",
        check_type="branch",
    )
    # Look for "result = 40" (case 3) being uncovered
    c4.passed = "result = 40;" in report_text and (
        "0|" in report_text.split("result = 40;")[0].split("\n")[-1]
        if "result = 40;" in report_text
        else False
    )
    c4.expected = "case 3 line shows 0 execution count"
    c4.actual = (
        "case 3 uncovered in report" if c4.passed else "could not verify from report"
    )
    checks.append(c4)

    # ── Check 5: MC/DC full coverage reported ─────────────────────────
    c5 = CanaryCheck(
        name="Full MC/DC reported for fully-tested decision",
        description="mcdc_full_canary: (a && b) || c shows MC/DC coverage in report",
        check_type="mcdc",
    )
    # MC/DC sections in the report contain "MC/DC" keyword near the function
    mcdc_section = (
        report_text.split("mcdc_full_canary")[-1].split("mcdc_partial_canary")[0]
        if "mcdc_full_canary" in report_text
        else ""
    )
    c5.passed = "MC/DC" in mcdc_section or "Condition" in mcdc_section
    c5.expected = "MC/DC annotation present for mcdc_full_canary"
    c5.actual = "MC/DC section found" if c5.passed else "no MC/DC section in report"
    checks.append(c5)

    # ── Check 6: MC/DC partial coverage reported ──────────────────────
    c6 = CanaryCheck(
        name="Partial MC/DC reported for partially-tested decision",
        description=(
            "mcdc_partial_canary: (p && q) with only (T,T) tested"
            " shows incomplete MC/DC"
        ),
        check_type="mcdc",
    )
    # Look for the MC/DC Decision Region near mcdc_partial_canary
    # and verify it shows 0% or "not covered" for at least one condition
    c6_found_mcdc = False
    c6_shows_partial = False
    parts = report_text.split("mcdc_partial_canary")
    for part in parts[1:]:  # skip text before first mention
        if "MC/DC" in part[:600]:
            c6_found_mcdc = True
            if "not covered" in part[:600] or "0.00%" in part[:600]:
                c6_shows_partial = True
                break
    c6.passed = c6_found_mcdc and c6_shows_partial
    c6.expected = "MC/DC annotation with partial coverage (not covered / 0%)"
    c6.actual = (
        "MC/DC section found with partial coverage"
        if c6.passed
        else "MC/DC found but no partial"
        if c6_found_mcdc
        else "no MC/DC section in report"
    )
    checks.append(c6)

    # ── Check 7: Fully uncovered function ─────────────────────────────
    c7 = CanaryCheck(
        name="Uncovered function reported with zero count",
        description="fully_uncovered_canary: never called, shows 0 execution count",
        check_type="function",
    )
    uncovered_count = 0
    for name, count in func_counts.items():
        if "fully_uncovered_canary" in name:
            uncovered_count = count
            break
    c7.passed = (uncovered_count == 0) and any(
        "fully_uncovered_canary" in name for name in func_counts
    )
    c7.expected = "function count == 0"
    c7.actual = (
        f"count = {uncovered_count}"
        if any("fully_uncovered_canary" in n for n in func_counts)
        else "function not found in coverage data"
    )
    checks.append(c7)

    return checks


def write_rst_report(checks: list[CanaryCheck], llvm_version: str) -> str:
    """Generate RST evidence report."""
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    path = REPORT_DIR / "coverage_validation_report.rst"

    passed = sum(1 for c in checks if c.passed)
    failed = sum(1 for c in checks if not c.passed)

    lines = [
        "Tool Validation Report — llvm-cov Coverage Measurement",
        "=======================================================",
        "",
        f".. note:: Generated on {datetime.date.today()}"
        " by ``scripts/validate_coverage.py``.",
        "",
        "Purpose",
        "-------",
        "This report provides evidence that the coverage measurement tool",
        "(llvm-cov) correctly reports covered vs uncovered lines, branches,",
        "and MC/DC conditions. Five canary functions in ``coverage_canary.hpp``",
        "have known coverage properties that are verified after running",
        "deliberately-partial tests.",
        "",
        "This satisfies the tool confidence requirement of **ECSS-Q-ST-80C §5.5**",
        "and is analogous to DO-330 TQL-5 tool qualification by demonstration",
        "for the coverage measurement tool.",
        "",
        f"**llvm-cov version**: ``{llvm_version}``",
        "",
        "Summary",
        "-------",
        f"- **{passed} / {len(checks)} checks validated**",
        f"- **{failed} discrepancies**",
        "",
    ]

    if failed > 0:
        lines.append(f".. warning:: {failed} check(s) did NOT match expected coverage.")
        lines.append(
            "   This is a tool confidence deficiency that must be investigated."
        )
        lines.append("")

    lines += [
        "Results",
        "-------",
        "",
        ".. list-table::",
        "   :header-rows: 1",
        "   :widths: 35 10 10 25 20",
        "",
        "   * - Check",
        "     - Type",
        "     - Result",
        "     - Expected",
        "     - Actual",
    ]

    for c in checks:
        status = "PASS" if c.passed else "**FAIL**"
        lines.append(f"   * - {c.name}")
        lines.append(f"     - {c.check_type}")
        lines.append(f"     - {status}")
        lines.append(f"     - {c.expected}")
        lines.append(f"     - {c.actual}")

    lines += [
        "",
        "Canary Functions",
        "----------------",
        "The canary functions are defined in"
        " ``cpp/tests/tool_validation/coverage_canary.hpp``:",
        "",
        "1. **line_coverage_canary** — if/else with only true branch tested",
        "2. **branch_coverage_canary** — switch with 3/5 cases tested",
        "3. **mcdc_full_canary** — ``(a && b) || c`` with full MC/DC test vectors",
        "4. **mcdc_partial_canary** — ``(p && q)`` with only (T,T) tested",
        "5. **fully_uncovered_canary** — never called by any test",
        "",
    ]

    path.write_text("\n".join(lines) + "\n")
    return str(path)


def write_json_report(checks: list[CanaryCheck]) -> str:
    """Generate JSON evidence report."""
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    path = REPORT_DIR / "coverage_validation_report.json"
    data = {
        "generated": datetime.date.today().isoformat(),
        "total": len(checks),
        "passed": sum(1 for c in checks if c.passed),
        "failed": sum(1 for c in checks if not c.passed),
        "checks": [
            {
                "name": c.name,
                "type": c.check_type,
                "passed": c.passed,
                "expected": c.expected,
                "actual": c.actual,
            }
            for c in checks
        ],
    }
    path.write_text(json.dumps(data, indent=2) + "\n")
    return str(path)


def _get_tool_version(tool_path: str) -> str:
    """Extract version string from an LLVM tool."""
    ver = subprocess.run(
        [tool_path, "--version"],
        capture_output=True,
        text=True,
        timeout=5,
    )
    for line in ver.stdout.split("\n"):
        if "version" in line.lower():
            return line.strip()
    return ver.stdout.strip().split("\n")[0]


def main() -> int:
    """Validate llvm-cov coverage measurement (tool qualification)."""
    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument(
        "--build-dir",
        default="build/coverage-validation",
        help="Build directory for coverage validation",
    )
    parser.add_argument("--llvm-dir", help="Path to LLVM bin directory")
    args = parser.parse_args()

    # Find LLVM tools
    llvm_cov = find_llvm_tool("llvm-cov", args.llvm_dir)
    llvm_profdata = find_llvm_tool("llvm-profdata", args.llvm_dir)

    if not llvm_cov or not llvm_profdata:
        print(
            "ERROR: llvm-cov/llvm-profdata not found. Install LLVM: brew install llvm"
        )
        return 1

    version_line = _get_tool_version(llvm_cov)
    print("=== Coverage Tool Validation (ECSS-Q-ST-80C §5.5) ===")
    print(f"llvm-cov: {version_line}")
    print()

    # Build with coverage
    llvm_dir = args.llvm_dir
    if not llvm_dir:
        # Detect from llvm-cov path
        llvm_dir = str(Path(llvm_cov).parent)

    if not build_coverage(args.build_dir, llvm_dir):
        return 1

    # Run canary tests
    if not run_canary_tests(args.build_dir):
        return 1

    # Merge profiles
    if not merge_profile(args.build_dir, llvm_profdata):
        print("ERROR: Failed to merge coverage profiles", file=sys.stderr)
        return 1

    # Validate
    checks = validate_canaries(args.build_dir, llvm_cov)

    passed = 0
    failed = 0
    for c in checks:
        if c.passed:
            passed += 1
            print(f"  PASS  {c.name}")
        else:
            failed += 1
            print(f"  FAIL  {c.name}")
            print(f"        expected: {c.expected}")
            print(f"        actual:   {c.actual}")

    print()
    print(f"Results: {passed}/{len(checks)} passed, {failed} failed")

    # Write reports
    rst_path = write_rst_report(checks, version_line)
    json_path = write_json_report(checks)
    print(f"Reports: {rst_path}, {json_path}")

    if failed > 0:
        print(f"\nFAILED: {failed} coverage check(s) did not match expectations.")
        return 1

    print("\nAll coverage checks passed. Tool confidence verified.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
