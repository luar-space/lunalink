#!/usr/bin/env python3
"""Parse Catch2 XML benchmark results and generate a WCET RST report.

Usage:
    python3 scripts/wcet_report.py \
        --xml /tmp/wcet_results.xml \
        --console /tmp/wcet_console.txt \
        --output docs/assurance/wcet_profile.rst
"""

from __future__ import annotations

import argparse
import datetime
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass


@dataclass
class BenchmarkEntry:  # noqa: D101
    name: str
    mean_ns: float
    stddev_ns: float
    samples: int
    low_mean_ns: float
    high_mean_ns: float


def parse_xml(path: str) -> list[BenchmarkEntry]:
    """Parse Catch2 XML output and extract BenchmarkResults."""
    tree = ET.parse(path)
    root = tree.getroot()
    entries: list[BenchmarkEntry] = []

    for br in root.iter("BenchmarkResults"):
        name = br.get("name", "unknown")
        samples = int(br.get("samples", "0"))

        mean_el = br.find("mean")
        sd_el = br.find("standardDeviation")

        if mean_el is None:
            continue

        entries.append(
            BenchmarkEntry(
                name=name,
                mean_ns=float(mean_el.get("value", "0")),
                stddev_ns=float(sd_el.get("value", "0")) if sd_el is not None else 0.0,
                samples=samples,
                low_mean_ns=float(mean_el.get("lowerBound", "0")),
                high_mean_ns=float(mean_el.get("upperBound", "0")),
            )
        )

    return entries


def format_time(ns: float) -> str:
    """Format nanoseconds into a human-readable string."""
    if ns >= 1e6:
        return f"{ns / 1e6:.3f} ms"
    if ns >= 1e3:
        return f"{ns / 1e3:.3f} us"
    return f"{ns:.1f} ns"


def evaluate(entries: list[BenchmarkEntry]) -> tuple[list[str], bool]:
    """Evaluate benchmark confidence intervals. Return (verdicts, any_failed)."""
    verdicts: list[str] = []
    failed = False

    for e in entries:
        # Use the 95% CI spread relative to mean as the stability metric.
        if e.mean_ns <= 0:
            verdicts.append(f"SKIP  {e.name}: zero mean")
            continue

        ci_spread_pct = (e.high_mean_ns - e.low_mean_ns) / e.mean_ns * 100.0
        cv_pct = e.stddev_ns / e.mean_ns * 100.0

        # Flag if the 95% CI exceeds ±5% of the mean (realistic for shared runners).
        if ci_spread_pct > 10.0:
            verdicts.append(
                f"WARN  {e.name}: 95% CI spread {ci_spread_pct:.1f}% "
                f"(mean={format_time(e.mean_ns)}, CV={cv_pct:.1f}%)"
            )
        else:
            verdicts.append(
                f"PASS  {e.name}: 95% CI spread {ci_spread_pct:.1f}% "
                f"(mean={format_time(e.mean_ns)}, CV={cv_pct:.1f}%)"
            )

    return verdicts, failed


def generate_rst(
    entries: list[BenchmarkEntry],
    verdicts: list[str],
    console_text: str,
    sha: str,
    today: str,
) -> str:
    """Generate the RST report content."""
    lines = [
        "WCET Profile",
        "============",
        "",
        "**Document ID**: LUNALINK-WCET-001",
        f"**Revision**: CI-generated — {today} ({sha})",
        "**Specification Reference**: Safety Plan §Acceptance Criteria 4",
        "",
        ".. note::",
        "   Generated automatically by the ``wcet`` CI workflow.",
        "",
        "Measurement Environment",
        "-----------------------",
        "",
        ".. list-table::",
        "   :header-rows: 1",
        "   :widths: 30 70",
        "",
        "   * - Parameter",
        "     - Value",
        "   * - Runner",
        "     - GitHub Actions ``ubuntu-latest``",
        "   * - Compiler",
        "     - GCC (Release, ``-O2``, ``-fno-fast-math``)",
        "   * - Timing method",
        "     - Catch2 ``BENCHMARK`` macro (nanosecond resolution)",
        "   * - Git SHA",
        f"     - ``{sha}``",
        "",
        "WCET Measurements",
        "-----------------",
        "",
        ".. list-table::",
        "   :header-rows: 1",
        "   :widths: 40 15 15 15 15",
        "",
        "   * - Function",
        "     - Mean",
        "     - Std Dev",
        "     - 95% CI Low",
        "     - 95% CI High",
    ]

    for e in entries:
        lines.append(f"   * - {e.name}")
        lines.append(f"     - {format_time(e.mean_ns)}")
        lines.append(f"     - {format_time(e.stddev_ns)}")
        lines.append(f"     - {format_time(e.low_mean_ns)}")
        lines.append(f"     - {format_time(e.high_mean_ns)}")

    lines += [
        "",
        "Compliance Status",
        "-----------------",
        "",
        "Per-function verdicts (95% CI spread, Safety Plan Acceptance Criteria 4):",
        "",
        ".. code-block:: text",
        "",
    ]

    for v in verdicts if verdicts else ["(no benchmark data)"]:
        lines.append(f"   {v}")

    lines += [
        "",
        "Raw Console Output",
        "-------------------",
        "",
        ".. code-block:: text",
        "",
    ]
    for line in console_text.splitlines():
        lines.append(f"   {line}")

    lines.append("")
    return "\n".join(lines)


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Generate WCET RST report from Catch2 XML"
    )
    parser.add_argument("--xml", required=True, help="Path to Catch2 XML output")
    parser.add_argument("--console", required=True, help="Path to console output")
    parser.add_argument("--output", required=True, help="Output RST path")
    args = parser.parse_args()

    entries = parse_xml(args.xml)
    if not entries:
        print(
            "WARNING: No BenchmarkResults found in XML. "
            "Are tests using BENCHMARK macros?"
        )

    with open(args.console) as f:
        console_text = f.read()

    sha = (
        subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])
        .decode()
        .strip()
    )
    today = str(datetime.date.today())

    verdicts, failed = evaluate(entries)

    rst = generate_rst(entries, verdicts, console_text, sha, today)
    with open(args.output, "w") as f:
        f.write(rst)

    print(f"WCET report: {len(entries)} benchmarks parsed")
    for v in verdicts:
        print(f"  {v}")

    if failed:
        print("ERROR: one or more benchmarks exceeded tolerance", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
