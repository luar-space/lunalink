#!/usr/bin/env python3
"""Parse GCC/Clang .su (stack usage) files and produce a JSON evidence artifact.

Usage:
    python3 scripts/stack_usage_report.py --build-dir build/stack-usage
    python3 scripts/stack_usage_report.py --build-dir build/stack-usage \
        --max-single 2048

Output: docs/assurance/private/evidence/stack_usage_report.json
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import subprocess
import sys
from dataclasses import asdict, dataclass


@dataclass
class FunctionEntry:
    """A single function's stack usage record."""

    file: str
    line: int
    function: str
    stack_bytes: int
    type: str  # static, dynamic, bounded


def find_su_files(build_dir: str) -> list[str]:
    """Recursively find all .su files in the build directory."""
    su_files: list[str] = []
    for root, _, files in os.walk(build_dir):
        for f in files:
            if f.endswith(".su"):
                su_files.append(os.path.join(root, f))
    return sorted(su_files)


def parse_su_file(path: str, source_root: str) -> list[FunctionEntry]:
    r"""Parse a single .su file.

    Format varies by compiler:
        GCC:   <file>:<line>:<column>:<function>\t<size>\t<type>
        Clang: <file>:<line>:<function>\t<size>\t<type>
    """
    entries: list[FunctionEntry] = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Split on tab to get: location+function, size, type
            parts = line.split("\t")
            if len(parts) < 3:
                continue

            location_func = parts[0]
            try:
                size = int(parts[1])
            except ValueError:
                continue
            usage_type = parts[2].strip()

            # Parse location — function name is the last colon-separated part
            colon_parts = location_func.split(":")
            if len(colon_parts) < 3:
                continue

            # Function name (possibly mangled) is the last part
            function = colon_parts[-1]

            # Try to find the line number: it's the part before function that
            # is a pure integer. Could be index -2 (Clang: file:line:func) or
            # -3 (GCC: file:line:col:func).
            line_no = 0
            file_end_idx = -2  # default: everything before line:func
            try:
                line_no = int(colon_parts[-2])
                file_end_idx = -2
            except ValueError:
                # GCC format with column: try -3
                if len(colon_parts) >= 4:
                    try:
                        line_no = int(colon_parts[-3])
                        file_end_idx = -3
                    except ValueError:
                        continue
                else:
                    continue

            # File is everything before the line number
            file_path = ":".join(colon_parts[:file_end_idx])

            # Make path relative to source root
            try:
                file_path = os.path.relpath(file_path, source_root)
            except ValueError:
                pass

            entries.append(
                FunctionEntry(
                    file=file_path,
                    line=line_no,
                    function=function,
                    stack_bytes=size,
                    type=usage_type,
                )
            )
    return entries


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
        description="Generate stack usage evidence artifact from .su files"
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Build directory containing .su files (default: build)",
    )
    parser.add_argument(
        "--max-single",
        type=int,
        default=4096,
        help="Per-function stack usage threshold in bytes (default: 4096)",
    )
    parser.add_argument(
        "--output",
        default="docs/assurance/private/evidence/stack_usage_report.json",
        help="Output JSON path",
    )
    parser.add_argument(
        "--exclude",
        nargs="*",
        default=["tests/", "_deps/", "catch2/"],
        help=(
            "Exclude functions whose file path contains any of these path segments"
            " from the threshold gate (default: tests/ _deps/ catch2/)"
        ),
    )
    args = parser.parse_args()

    su_files = find_su_files(args.build_dir)
    if not su_files:
        print(f"ERROR: No .su files found in {args.build_dir}", file=sys.stderr)
        print(
            "Ensure the build was configured with -DENABLE_STACK_USAGE=ON",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"Found {len(su_files)} .su file(s) in {args.build_dir}")

    # Determine source root (repo root)
    source_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    all_entries: list[FunctionEntry] = []
    for su_path in su_files:
        all_entries.extend(parse_su_file(su_path, source_root))

    if not all_entries:
        print("WARNING: No function entries parsed from .su files")

    # Build summary
    max_entry = max(all_entries, key=lambda e: e.stack_bytes) if all_entries else None
    dynamic_count = sum(1 for e in all_entries if e.type == "dynamic")
    total_static = sum(e.stack_bytes for e in all_entries if e.type == "static")

    summary = {
        "total_functions": len(all_entries),
        "max_stack_bytes": max_entry.stack_bytes if max_entry else 0,
        "max_stack_function": max_entry.function if max_entry else "",
        "dynamic_count": dynamic_count,
        "total_static_bytes": total_static,
    }

    report = {
        "generated": datetime.datetime.now(datetime.UTC).isoformat(),
        "git_sha": get_git_sha(),
        "functions": [asdict(e) for e in all_entries],
        "summary": summary,
    }

    # Write output
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(report, f, indent=2)
        f.write("\n")

    print(f"Stack usage report: {args.output}")
    print(f"  Total functions: {summary['total_functions']}")
    print(
        f"  Max stack: {summary['max_stack_bytes']} bytes"
        f" ({summary['max_stack_function']})"
    )
    print(f"  Dynamic allocations: {summary['dynamic_count']}")

    # Check threshold (exclude test/vendored code from the gate)
    gated = [e for e in all_entries if not any(ex in e.file for ex in args.exclude)]
    violations = [e for e in gated if e.stack_bytes > args.max_single]
    dynamic_in_prod = [e for e in gated if e.type in ("dynamic", "bounded")]
    exit_code = 0

    if violations:
        print(
            f"\nERROR: {len(violations)} function(s) exceed"
            f" {args.max_single}-byte threshold:",
            file=sys.stderr,
        )
        for v in violations:
            print(
                f"  {v.function} ({v.file}:{v.line}): {v.stack_bytes} bytes",
                file=sys.stderr,
            )
        exit_code = 1
    else:
        print(f"  All functions under {args.max_single}-byte threshold: PASS")

    if dynamic_in_prod:
        print(
            f"\nERROR: {len(dynamic_in_prod)} production function(s)"
            " use dynamic/bounded stack allocation:",
            file=sys.stderr,
        )
        for d in dynamic_in_prod:
            print(
                f"  {d.function} ({d.file}:{d.line}): {d.stack_bytes} bytes [{d.type}]",
                file=sys.stderr,
            )
        exit_code = 1
    else:
        print("  No dynamic/bounded stack allocations in production code: PASS")

    if exit_code != 0:
        sys.exit(exit_code)


if __name__ == "__main__":
    main()
