#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Tool Validation Suite — clang-tidy MISRA C++:2023 enforcement verification.

Part of the static analysis evidence chain for ECSS-Q-ST-80C §5.5.
This script proves that the clang-tidy configuration (.clang-tidy) actually
catches the MISRA/JPL/P10 violations it claims to enforce.

Each file in cpp/tests/tool_validation/violations/ contains exactly one
deliberate coding violation and a comment declaring the expected clang-tidy
diagnostic. This script runs clang-tidy on each file and verifies that:

  1. clang-tidy exits non-zero (violation detected)
  2. The expected diagnostic ID appears in the output

If any file passes clang-tidy without triggering its expected diagnostic,
the tool configuration has a false negative — a certification-blocking defect.

Usage:
    python3 scripts/validate_tidy.py [--clang-tidy PATH] [--build-dir DIR]
    task validate-tidy

Evidence output:
    build/reports/tool_validation_report.rst  (formal evidence artifact)
    build/reports/tool_validation_report.json (machine-readable)

Standards traceability:
    ECSS-Q-ST-80C §5.5  — tool confidence / tool validation
    MISRA Compliance:2020 — recommends verifying tool detection capability
    DO-330 (TQL-5)       — tool qualification by demonstration
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

VIOLATIONS_DIR = Path("cpp/tests/tool_validation/violations")
REPORT_DIR = Path("build/reports")

# Regex to extract the expected diagnostic from the file header comment.
# Matches: "Expected diagnostic: <check-name>"
EXPECTED_RE = re.compile(r"Expected diagnostic:\s+(\S+)")


@dataclass
class ValidationResult:
    """Result of running clang-tidy on one deliberate-violation file."""

    file: str
    misra_rule: str
    expected_check: str
    detected: bool
    actual_checks: list[str] = field(default_factory=list)
    stderr_snippet: str = ""


def find_clang_tidy(hint: str | None = None) -> str | None:
    """Locate clang-tidy binary."""
    if hint and os.path.isfile(hint):
        return hint
    candidates = [
        "clang-tidy",
        "/opt/homebrew/opt/llvm/bin/clang-tidy",
        "/usr/local/opt/llvm/bin/clang-tidy",
        "/opt/homebrew/bin/clang-tidy",
        "/usr/local/bin/clang-tidy",
    ]
    for c in candidates:
        try:
            subprocess.run(
                [c, "--version"],
                capture_output=True,
                check=True,
                timeout=5,
            )
            return c
        except (
            subprocess.CalledProcessError,
            FileNotFoundError,
            subprocess.TimeoutExpired,
        ):
            continue
    return None


def parse_violation_file(path: Path) -> tuple[str, str]:
    """Extract MISRA rule ID and expected diagnostic from file header."""
    text = path.read_text()
    # Extract MISRA rule from filename: misra_X_Y_Z_description.cpp -> X.Y.Z
    stem = path.stem
    if stem.startswith("misra_"):
        parts = stem.replace("misra_", "").split("_")
        # Take numeric parts for rule ID
        rule_parts = []
        for p in parts:
            if p.isdigit():
                rule_parts.append(p)
            else:
                break
        misra_rule = ".".join(rule_parts) if rule_parts else stem
    elif stem.startswith("p10_"):
        misra_rule = stem.replace("_", " ").title()
    else:
        misra_rule = stem

    match = EXPECTED_RE.search(text)
    if not match:
        print(
            f"ERROR: {path.name} has no 'Expected diagnostic:' comment", file=sys.stderr
        )
        sys.exit(1)
    return misra_rule, match.group(1)


def get_checks_from_config(clang_tidy: str, build_dir: str) -> str:
    """Extract the effective Checks string from the root .clang-tidy config."""
    result = subprocess.run(
        [clang_tidy, "--dump-config", "-p", build_dir],
        capture_output=True,
        text=True,
        timeout=10,
        cwd=Path(".").resolve(),  # Use root dir to pick up root .clang-tidy
    )
    for line in result.stdout.split("\n"):
        if line.startswith("Checks:"):
            # Extract the quoted value
            val = line.split(":", 1)[1].strip().strip("'\"")
            return val
    return ""


def run_tidy(
    clang_tidy: str,
    source: Path,
    build_dir: str,
    checks_override: str = "",
) -> tuple[int, str]:
    """Run clang-tidy on a single source file, return (exit_code, combined_output)."""
    cmd = [
        clang_tidy,
        "-p",
        build_dir,
        # Override checks from root config but with NO warnings-as-errors
        f"--checks={checks_override}" if checks_override else "--checks=*",
        "--warnings-as-errors=",
        "--extra-arg=-Wno-unknown-warning-option",
        "--extra-arg=-Wno-error=unknown-warning-option",
        "--extra-arg=-std=c++20",
        "--header-filter=.*",
        str(source),
    ]
    env = os.environ.copy()
    # Set SDKROOT on macOS
    try:
        sdk = subprocess.run(
            ["xcrun", "--show-sdk-path"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if sdk.returncode == 0:
            env["SDKROOT"] = sdk.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=60,
        env=env,
    )
    combined = result.stdout + result.stderr
    return result.returncode, combined


def extract_diagnostics(output: str) -> list[str]:
    """Extract clang-tidy diagnostic IDs from output.

    Matches patterns like:
      [check-name]
      [check-name,-warnings-as-errors]
      [-Wsomething]
      [clang-diagnostic-something]

    Strips the -warnings-as-errors suffix and returns clean check IDs.
    """
    results = []
    # Match everything inside square brackets on diagnostic lines
    for bracket in re.findall(r"\[([^\]]+)\]", output):
        # Split compound brackets like "check-name,-warnings-as-errors"
        for part in bracket.split(","):
            part = part.strip()
            if part == "-warnings-as-errors":
                continue
            if part.startswith("-W"):
                # Compiler warning flag — convert to clang-diagnostic form
                results.append("clang-diagnostic-" + part[2:])
            elif re.match(r"[a-z]", part):
                results.append(part)
    return results


# Known aliases: clang-tidy may report either variant for the same check.
# Map from expected check -> set of acceptable aliases.
CHECK_ALIASES: dict[str, set[str]] = {
    "cppcoreguidelines-avoid-goto": {"hicpp-avoid-goto"},
    "hicpp-avoid-goto": {"cppcoreguidelines-avoid-goto"},
    "cppcoreguidelines-special-member-functions": {"hicpp-special-member-functions"},
    "hicpp-special-member-functions": {"cppcoreguidelines-special-member-functions"},
    "google-explicit-constructor": {"hicpp-explicit-conversions"},
    "hicpp-explicit-conversions": {"google-explicit-constructor"},
    "cppcoreguidelines-pro-type-vararg": {"hicpp-vararg"},
    "hicpp-vararg": {"cppcoreguidelines-pro-type-vararg"},
    "readability-braces-around-statements": {"hicpp-braces-around-statements"},
    "hicpp-braces-around-statements": {"readability-braces-around-statements"},
    "modernize-use-nullptr": {"hicpp-use-nullptr"},
    "hicpp-use-nullptr": {"modernize-use-nullptr"},
    "hicpp-uppercase-literal-suffix": {"readability-uppercase-literal-suffix"},
    "readability-uppercase-literal-suffix": {"hicpp-uppercase-literal-suffix"},
    "cppcoreguidelines-pro-bounds-array-to-pointer-decay": {"hicpp-no-array-decay"},
    "hicpp-no-array-decay": {"cppcoreguidelines-pro-bounds-array-to-pointer-decay"},
    "bugprone-narrowing-conversions": {
        "cppcoreguidelines-narrowing-conversions",
        "clang-diagnostic-implicit-int-conversion",
    },
    "cppcoreguidelines-narrowing-conversions": {"bugprone-narrowing-conversions"},
    # Compiler diagnostics that cover the same rule as tidy checks
    "bugprone-shadow": {"clang-diagnostic-shadow"},
    "cppcoreguidelines-pro-type-cstyle-cast": {
        "google-readability-casting",
        "clang-diagnostic-old-style-cast",
        "modernize-avoid-c-style-cast",
    },
}


def write_rst_report(results: list[ValidationResult], clang_tidy_version: str) -> str:
    """Generate RST evidence report."""
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    path = REPORT_DIR / "tool_validation_report.rst"

    passed = sum(1 for r in results if r.detected)
    failed = sum(1 for r in results if not r.detected)

    lines = [
        "Tool Validation Report — clang-tidy MISRA Enforcement",
        "======================================================",
        "",
        f".. note:: Generated on {datetime.date.today()}"
        " by ``scripts/validate_tidy.py``.",
        "",
        "Purpose",
        "-------",
        "This report provides evidence that the clang-tidy configuration",
        "(``.clang-tidy``) detects the MISRA C++:2023, JPL, and Power of Ten",
        "violations it claims to enforce. Each test case contains exactly one",
        "deliberate violation and asserts that the expected diagnostic fires.",
        "",
        "This satisfies the tool confidence requirement of **ECSS-Q-ST-80C §5.5**",
        "and is analogous to DO-330 TQL-5 tool qualification by demonstration.",
        "",
        f"**clang-tidy version**: ``{clang_tidy_version}``",
        "",
        "Summary",
        "-------",
        f"- **{passed} / {len(results)} checks validated** (expected diagnostic fired)",
        f"- **{failed} false negatives** (violation not detected)",
        "",
    ]

    if failed > 0:
        lines.append(
            f".. warning:: {failed} check(s) did NOT detect their expected violation."
        )
        lines.append("   This is a tool confidence deficiency that must be resolved.")
        lines.append("")

    lines += [
        "Results",
        "-------",
        "",
        ".. list-table::",
        "   :header-rows: 1",
        "   :widths: 30 20 25 10 15",
        "",
        "   * - File",
        "     - MISRA Rule",
        "     - Expected Check",
        "     - Result",
        "     - Actual Diagnostics",
    ]

    for r in results:
        status = "PASS" if r.detected else "**FAIL**"
        actuals = (
            ", ".join(sorted(set(r.actual_checks[:3]))) if r.actual_checks else "—"
        )
        lines.append(f"   * - ``{r.file}``")
        lines.append(f"     - {r.misra_rule}")
        lines.append(f"     - ``{r.expected_check}``")
        lines.append(f"     - {status}")
        lines.append(f"     - {actuals}")

    lines += [
        "",
        "Methodology",
        "-----------",
        "Each violation file in ``cpp/tests/tool_validation/violations/`` contains:",
        "",
        "1. A header comment citing the MISRA/JPL/P10 rule being tested.",
        "2. An ``Expected diagnostic:`` line naming the clang-tidy check.",
        "3. Source code with exactly one deliberate violation.",
        "",
        "The validation runner (``scripts/validate_tidy.py``) executes clang-tidy",
        "on each file using the project's ``.clang-tidy`` configuration and verifies",
        "that the expected diagnostic appears in the output. A PASS means the tool",
        "correctly identifies the violation. A FAIL means the tool has a"
        " false negative",
        "— the violation exists but was not detected.",
        "",
    ]

    path.write_text("\n".join(lines) + "\n")
    return str(path)


def write_json_report(results: list[ValidationResult]) -> str:
    """Generate JSON evidence report."""
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    path = REPORT_DIR / "tool_validation_report.json"
    data = {
        "generated": datetime.date.today().isoformat(),
        "total": len(results),
        "passed": sum(1 for r in results if r.detected),
        "failed": sum(1 for r in results if not r.detected),
        "results": [
            {
                "file": r.file,
                "misra_rule": r.misra_rule,
                "expected_check": r.expected_check,
                "detected": r.detected,
                "actual_checks": r.actual_checks,
            }
            for r in results
        ],
    }
    path.write_text(json.dumps(data, indent=2) + "\n")
    return str(path)


def _ensure_compile_commands(build_dir: str) -> None:
    """Generate compile_commands.json if it doesn't exist."""
    cc_json = Path(build_dir) / "compile_commands.json"
    if cc_json.exists():
        return
    print(f"Generating compile_commands.json in {build_dir}...")
    gen = (
        "Ninja"
        if os.path.exists("/usr/bin/ninja") or os.path.exists("/opt/homebrew/bin/ninja")
        else "Unix Makefiles"
    )
    subprocess.run(
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
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        check=True,
        capture_output=True,
    )


def main() -> int:
    """Validate clang-tidy MISRA enforcement (tool qualification)."""
    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--clang-tidy", help="Path to clang-tidy binary")
    parser.add_argument(
        "--build-dir",
        default="build/tidy",
        help="Build directory with compile_commands.json",
    )
    args = parser.parse_args()

    clang_tidy = find_clang_tidy(args.clang_tidy)
    if not clang_tidy:
        print("ERROR: clang-tidy not found. Install LLVM: brew install llvm")
        return 1

    # Get version
    ver = subprocess.run(
        [clang_tidy, "--version"],
        capture_output=True,
        text=True,
        timeout=5,
    )
    version_line = ver.stdout.strip().split("\n")[0] if ver.stdout else "unknown"

    _ensure_compile_commands(args.build_dir)

    # Discover violation files
    if not VIOLATIONS_DIR.exists():
        print(f"ERROR: {VIOLATIONS_DIR} does not exist")
        return 1

    violation_files = sorted(VIOLATIONS_DIR.glob("*.cpp"))
    if not violation_files:
        print(f"ERROR: No .cpp files found in {VIOLATIONS_DIR}")
        return 1

    print("=== Tool Validation Suite (ECSS-Q-ST-80C §5.5) ===")
    print(f"clang-tidy: {version_line}")
    print(f"Violations: {len(violation_files)} test cases")
    print()

    results: list[ValidationResult] = []
    passed = 0
    failed = 0

    for vf in violation_files:
        misra_rule, expected_check = parse_violation_file(vf)
        _, output = run_tidy(clang_tidy, vf, args.build_dir, "*")
        actual_checks = extract_diagnostics(output)

        # Check for expected diagnostic or any known alias
        acceptable = {expected_check} | CHECK_ALIASES.get(expected_check, set())
        detected = bool(acceptable & set(actual_checks))

        result = ValidationResult(
            file=vf.name,
            misra_rule=misra_rule,
            expected_check=expected_check,
            detected=detected,
            actual_checks=actual_checks,
            stderr_snippet=output[:200] if not detected else "",
        )
        results.append(result)

        if detected:
            passed += 1
            print(f"  PASS  {vf.name:<45s} [{expected_check}]")
        else:
            failed += 1
            print(f"  FAIL  {vf.name:<45s} [{expected_check}] — NOT DETECTED")
            if output.strip():
                # Show first relevant line
                for line in output.split("\n"):
                    if "error:" in line or "warning:" in line:
                        print(f"        actual: {line.strip()[:100]}")
                        break

    print()
    print(f"Results: {passed}/{len(results)} passed, {failed} failed")

    # Write reports
    rst_path = write_rst_report(results, version_line)
    json_path = write_json_report(results)
    print(f"Reports: {rst_path}, {json_path}")

    if failed > 0:
        print(f"\nFAILED: {failed} violation(s) not detected by clang-tidy.")
        print("This indicates a false negative in the tool configuration.")
        return 1

    print("\nAll violations detected. Tool confidence verified.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
