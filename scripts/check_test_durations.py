#!/usr/bin/env python3
"""Fail if any test in a JUnit XML report took >threshold seconds without [slow].

Routing contract: `test-cpp` filters `~[slow]` so fast tests stay fast. A test
that crosses the threshold without being tagged `[slow]` has either been
mis-tagged or has drifted slower over time. This check runs after `test-cpp`
(locally via `task ci-checks`) to catch that drift on the PR that introduced it.

Usage:
    check_test_durations.py --xml FILE... --binary PATH... [--threshold 5.0]

--binary is a Catch2 test binary used to build a name→tags map via
`--list-tests --verbosity high`. Pass once per binary referenced by the XMLs.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def build_tag_map(binaries: list[Path]) -> dict[str, str]:
    """Return {test_name: tag_string} by invoking each binary's --list-tests."""
    tag_map: dict[str, str] = {}
    for binary in binaries:
        out = subprocess.check_output(
            [str(binary), "--list-tests", "--verbosity", "high"],
            text=True,
        )
        # Catch2 verbose list format:
        #   Name Of Test
        #       /path/to/file.cpp:LINE
        #       tags: [tag1], [tag2]
        current_name: str | None = None
        for line in out.splitlines():
            stripped = line.strip()
            if (
                not line.startswith(" ")
                and stripped
                and not stripped.startswith(("Matching", "All available", "test cases"))
            ):
                current_name = stripped
            elif stripped.startswith("tags:") and current_name is not None:
                tag_map[current_name] = stripped[len("tags:") :].strip()
    return tag_map


def main() -> int:
    """Entry point: parse JUnit reports and flag slow tests missing [slow]."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--xml", action="append", required=True, type=Path)
    parser.add_argument("--binary", action="append", required=True, type=Path)
    parser.add_argument("--threshold", type=float, default=5.0)
    args = parser.parse_args()

    tag_map = build_tag_map(args.binary)

    offenders: list[tuple[str, float]] = []
    for xml_path in args.xml:
        if not xml_path.exists():
            continue
        root = ET.parse(xml_path).getroot()
        for tc in root.iter("testcase"):
            name = tc.get("name", "")
            time = float(tc.get("time", "0") or 0.0)
            if time <= args.threshold:
                continue
            tags = tag_map.get(name, "")
            if "[slow]" in tags:
                continue
            offenders.append((name, time))

    if offenders:
        print(
            f"Tests exceeding {args.threshold:.1f}s without [slow] tag:",
            file=sys.stderr,
        )
        for name, time in sorted(offenders, key=lambda x: -x[1]):
            print(f"  {time:6.2f}s  {name}", file=sys.stderr)
        print(
            f"\n{len(offenders)} test(s) over threshold. "
            "Add [slow] tag or optimise the test.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
