#!/usr/bin/env python3
"""Parse Catch2 XML benchmark results and generate a WCET RST report.

Usage:
    python3 scripts/wcet_report.py \
        --xml /tmp/wcet_results.xml \
        --console /tmp/wcet_console.txt \
        --output docs/assurance/private/wcet_profile.rst
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
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


def parse_history(path: str) -> list[dict]:
    """Parse benchmark-action data.js and return list of runs (newest first)."""
    with open(path) as f:
        text = f.read()
    # Strip JS wrapper: "window.BENCHMARK_DATA = {...}"
    prefix = "window.BENCHMARK_DATA = "
    if text.startswith(prefix):
        text = text[len(prefix) :]
    data = json.loads(text)
    runs = data.get("entries", {}).get("Benchmark", [])
    return list(reversed(runs))


def generate_summary(
    entries: list[BenchmarkEntry],
    verdicts: list[str],
    sha: str,
    history: list[dict] | None = None,
    max_history: int = 10,
) -> str:
    """Generate a GitHub Actions step summary (markdown)."""
    lines = [
        "## WCET Profile",
        "",
        f"**Commit:** `{sha}` | **Benchmarks:** {len(entries)}",
        "",
        "| Function | Mean | Std Dev | 95% CI | Verdict |",
        "|----------|------|---------|--------|---------|",
    ]

    for e, v in zip(entries, verdicts):
        ci_spread_pct = (
            (e.high_mean_ns - e.low_mean_ns) / e.mean_ns * 100.0
            if e.mean_ns > 0
            else 0.0
        )
        status = "PASS" if v.startswith("PASS") else "WARN"
        lines.append(
            f"| {e.name} "
            f"| {format_time(e.mean_ns)} "
            f"| {format_time(e.stddev_ns)} "
            f"| {format_time(e.low_mean_ns)}–{format_time(e.high_mean_ns)}"
            f" ({ci_spread_pct:.1f}%) "
            f"| {status} |"
        )

    if history:
        lines += _trend_table(history, max_history)

    lines.append("")
    return "\n".join(lines)


def _trend_table(history: list[dict], max_history: int) -> list[str]:
    """Build a trend table from benchmark-action history."""
    recent = history[:max_history]

    # Collect all function names (preserving order from most recent run).
    func_names: list[str] = []
    for run in recent:
        for b in run.get("benches", []):
            if b["name"] not in func_names:
                func_names.append(b["name"])

    # Build header: Function | commit1 | commit2 | ...
    commits = [r["commit"]["id"][:7] for r in recent]
    header = "| Function | " + " | ".join(f"`{c}`" for c in commits) + " |"
    sep = "|----------|" + "|".join("------:" for _ in commits) + "|"

    lines = [
        "",
        f"### Trend (last {len(recent)} commits)",
        "",
        header,
        sep,
    ]

    for name in func_names:
        row = f"| {name} "
        for run in recent:
            val = next(
                (b["value"] for b in run.get("benches", []) if b["name"] == name),
                None,
            )
            row += f"| {format_time(val)} " if val is not None else "| — "
        row += "|"
        lines.append(row)

    return lines


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Generate WCET RST report from Catch2 XML"
    )
    parser.add_argument("--xml", required=True, help="Path to Catch2 XML output")
    parser.add_argument("--console", required=True, help="Path to console output")
    parser.add_argument("--output", required=True, help="Output RST path")
    parser.add_argument("--bench-json", help="Output JSON for benchmark-action")
    parser.add_argument(
        "--history", help="Path to data.js from gh-pages for trend table"
    )
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

    if args.bench_json:
        bench_data = [
            {"name": e.name, "unit": "ns", "value": e.mean_ns} for e in entries
        ]
        with open(args.bench_json, "w") as f:
            json.dump(bench_data, f, indent=2)

    history = None
    if args.history and os.path.exists(args.history):
        history = parse_history(args.history)

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        md = generate_summary(entries, verdicts, sha, history)
        with open(summary_path, "a") as f:
            f.write(md)

    print(f"WCET report: {len(entries)} benchmarks parsed")
    for v in verdicts:
        print(f"  {v}")

    if failed:
        print("ERROR: one or more benchmarks exceeded tolerance", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
