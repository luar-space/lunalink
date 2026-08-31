#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""I2 — Generate I/Q signal from LANS-AFS-SIM for interoperability testing.

Usage:
    python scripts/interop/generate_lans_signal.py [options]

Output: third_party/LANS-AFS-SIM/interop_iq16.bin  (headerless int16 I/Q)
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
LANS_DIR = REPO_ROOT / "third_party" / "LANS-AFS-SIM"
AFS_SIM = LANS_DIR / "afs_sim"
OUTPUT_DIR = REPO_ROOT / "third_party" / "interop_signals"


def generate(
    duration: float = 12.0,
    sample_rate: float = 10.23e6,
    bit_depth: int = 16,
) -> Path:
    """Run LANS-AFS-SIM to generate an I/Q signal file."""
    if not AFS_SIM.is_file():
        print(
            "ERROR: afs_sim not built. Run build_lans_afs_sim.py first.",
            file=sys.stderr,
        )
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    outfile = (
        OUTPUT_DIR
        / f"lans_iq{bit_depth}_{sample_rate / 1e6:.2f}MHz_{duration:.0f}s.bin"
    )

    cmd = [
        str(AFS_SIM),
        "-t",
        str(duration),
        "-s",
        str(sample_rate),
    ]
    # -b only accepts 2; 16-bit is the default (no flag)
    if bit_depth == 2:
        cmd += ["-b", "2"]
    cmd.append(str(outfile))

    print("Generating LANS-AFS-SIM signal:")
    print(f"  Duration:    {duration} s")
    print(f"  Sample rate: {sample_rate / 1e6:.2f} MHz")
    print(f"  Bit depth:   {bit_depth}")
    print(f"  Output:      {outfile}")
    print(f"  Command:     {' '.join(cmd)}")
    print()

    subprocess.run(cmd, cwd=LANS_DIR, check=True)

    size_mb = outfile.stat().st_size / (1024 * 1024)
    print(f"\nGenerated: {outfile} ({size_mb:.1f} MB)")
    return outfile


def main() -> None:
    """Parse arguments and generate LANS-AFS-SIM I/Q signal."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--duration",
        "-t",
        type=float,
        default=12.0,
        help="Signal duration in seconds (default: 12 = one frame)",
    )
    parser.add_argument(
        "--sample-rate",
        "-s",
        type=float,
        default=10.23e6,
        help="Sample rate in Hz (default: 10.23 MHz)",
    )
    parser.add_argument(
        "--bit-depth",
        "-b",
        type=int,
        default=16,
        choices=[2, 16],
        help="ADC bit width (default: 16)",
    )
    args = parser.parse_args()

    generate(
        duration=args.duration,
        sample_rate=args.sample_rate,
        bit_depth=args.bit_depth,
    )


if __name__ == "__main__":
    main()
