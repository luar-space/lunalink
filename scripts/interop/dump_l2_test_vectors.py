#!/usr/bin/env python3
"""Orchestrator: produce all 6 Level-2 LANS-AFS-SIM frame dumps.

Invokes the ``dump_lans_frame`` harness once per test message defined
in the competition interoperability plan, plus the boundary frame from
Test Case 4.  Output filenames match the public ``lsis-afs-test-vectors``
``frame_message_*.bin`` set so that bit-exact comparison is one ``cmp``
away (skipping the 64-byte header on the LunaLink side).

Usage:
    python scripts/interop/dump_l2_test_vectors.py <output_dir>

The harness binary lives at ``third_party/LANS-AFS-SIM/dump_lans_frame``.
Run ``python scripts/interop/build_lans_afs_sim.py`` first if not built.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
HARNESS = REPO_ROOT / "third_party" / "LANS-AFS-SIM" / "dump_lans_frame"

# Mirrors TEST_MESSAGES in scripts/export_test_vectors_l2.py.
# (name, fid, toi, pattern, seed_or_None)
TEST_MESSAGES: list[tuple[str, int, int, str, int | None]] = [
    ("message_1", 0, 0, "zeros", None),
    ("message_2", 0, 0, "ones", None),
    ("message_3", 0, 0, "alternating1", None),
    ("message_4", 0, 0, "marker", None),
    ("message_5", 0, 0, "random", 0xAF52),
    ("boundary", 3, 99, "alternating", None),
]


def main(argv: list[str] | None = None) -> int:
    """Dump Level 2 frame vectors as human-readable text for interop debugging."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        help="Target directory (typically references/lans-afs-sim/frames/ "
        "of the public lsis-afs-test-vectors repo)",
    )
    args = ap.parse_args(argv)

    if not HARNESS.is_file():
        print(
            f"error: harness not built at {HARNESS}\n"
            f"       run: python scripts/interop/build_lans_afs_sim.py",
            file=sys.stderr,
        )
        return 2

    out_dir: Path = args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    for name, fid, toi, pattern, seed in TEST_MESSAGES:
        cmd = [str(HARNESS), str(out_dir), name, str(fid), str(toi), pattern]
        if seed is not None:
            cmd.append(f"0x{seed:X}")
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            print(
                f"error: harness failed for {name}: {result.stderr}",
                file=sys.stderr,
            )
            return result.returncode
        # Echo the harness's confirmation line for the operator
        for line in result.stdout.splitlines():
            print(f"  {line}", file=sys.stderr)

    print(f"OK: {len(TEST_MESSAGES)} LANS-AFS-SIM dumps in {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
