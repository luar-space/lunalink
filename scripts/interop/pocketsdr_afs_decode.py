#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""L4/L5 — have PocketSDR-AFS decode a LunaLink signal, and check what it read.

This is the reverse direction of the interoperability plan.  L1-L3 compare
LunaLink against LANS-AFS-SIM with LunaLink doing the decoding; here an
independent *receiver* decodes a signal LunaLink generated, and its output is
compared against what LunaLink put in.

    L4  PocketSDR-AFS acquires, tracks, frame-synchronises, LDPC-decodes and
        CRC-verifies a LunaLink signal.
    L5  The navigation payload it recovered equals the payload LunaLink
        transmitted, bit for bit, including the CRC-24.

Signal format
-------------
``pocket_trk`` reads headerless interleaved int8 I/Q (``SDR_FMT_INT8X2``).
LANS-AFS-SIM's ``-b 2`` mode writes the same container with only four levels,
{-3,-1,+1,+3}, quantised about the 1-sigma point of the thermal noise
(``afs_sim.c``: ``thresh``).  This script reproduces that convention so the
receiver sees the sample statistics it was written for: unit-RMS signal, AWGN
at a chosen C/N0, then the same four-level quantiser.

Comparison
----------
``sdr_nav.c`` logs ``$SB2,<time>,<sig>,<prn>,<hex>,<crc>`` where ``<hex>`` is
the first 80 hex characters of the 1200 decoded bits and ``<crc>`` is
characters 294-299 — the CRC-24.  Both are compared against LunaLink's own
frame, so a mismatch anywhere in the chain shows up as a difference rather
than a silent pass.

Usage:
    python scripts/interop/pocketsdr_afs_decode.py [--frames 3] [--prn 3]
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
PSDR_DIR = REPO_ROOT / "third_party" / "PocketSDR-AFS"
POCKET_TRK = PSDR_DIR / "app" / "pocket_trk" / "pocket_trk"
EVIDENCE = REPO_ROOT / "docs" / "assurance" / "evidence" / "interop_l4_receiver.json"

SAMPLE_RATE = 12.0e6  # matches pocket_trk.sh's `-f 12`, and afs_sim's default
CN0_DBHZ = 45.0  # comfortably above the ~38-44 dB-Hz LANS-AFS-SIM produces
PAYLOAD_SEED = 0x12345678


def quantise_2bit(samples: np.ndarray) -> np.ndarray:
    """Four-level {-3,-1,+1,+3} int8 I/Q, interleaved — afs_sim's `-b 2` format."""
    out = np.empty(samples.size * 2, dtype=np.int8)
    real = np.real(samples)
    imag = np.imag(samples)
    thresh = np.sqrt(np.mean(real**2))  # 1-sigma, as in afs_sim.c
    for offset, component in ((0, real), (1, imag)):
        level = np.where(np.abs(component) > thresh, 3, 1).astype(np.int8)
        out[offset::2] = np.where(component >= 0, level, -level)
    return out


def generate_signal(path: Path, prn: int, frames: int, seed: int) -> dict:
    """Write `frames` consecutive LunaLink frames in the receiver's format."""
    from lunalink.afs import crc24_calculate, make_test_payload, transmit_frame

    sb2, sb3, sb4 = make_test_payload("random", seed=seed)
    rng = np.random.default_rng(42)
    snr = 10 ** ((CN0_DBHZ - 10 * np.log10(SAMPLE_RATE)) / 10)

    with path.open("wb") as fh:
        for k in range(frames):
            started = time.time()
            _, iq = transmit_frame(
                prn_id=prn,
                fid=0,
                toi=(40 + k) % 100,
                sb2_data=sb2,
                sb3_data=sb3,
                sb4_data=sb4,
                sample_rate=SAMPLE_RATE,
            )
            samples = np.asarray(iq, dtype=np.float32)
            if samples.ndim == 2:
                samples = samples[:, 0] + 1j * samples[:, 1]
            samples = samples / np.sqrt(np.mean(np.abs(samples) ** 2))
            noise = (
                rng.standard_normal(samples.size)
                + 1j * rng.standard_normal(samples.size)
            ) / np.sqrt(2)
            quantise_2bit(samples * np.sqrt(snr) + noise).tofile(fh)
            print(f"  frame {k}: TOI={(40 + k) % 100} ({time.time() - started:.1f} s)")

    bits = np.concatenate(
        [
            np.asarray(sb2, dtype=np.uint8),
            np.asarray(crc24_calculate(sb2), dtype=np.uint8),
        ]
    )
    hex_bits = "".join(f"{b:02X}" for b in np.packbits(bits))
    return {"sb2_hex80": hex_bits[:80], "sb2_crc24": hex_bits[294:300]}


def run_receiver(signal: Path, log: Path, prn: int) -> None:
    """Invoke pocket_trk exactly as upstream's own pocket_trk.sh does."""
    cmd = [
        str(POCKET_TRK),
        "-sig", "AFSD", "-prn", str(prn),
        "-sig", "AFSP", "-prn", str(prn),
        "-f", f"{SAMPLE_RATE / 1e6:g}",
        "-IQ", "2",
        "-log", str(log),
        str(signal),
    ]  # fmt: skip
    fs_mhz = f"{SAMPLE_RATE / 1e6:g}"
    print(f"  pocket_trk -sig AFSD/AFSP -prn {prn} -f {fs_mhz} -IQ 2")
    subprocess.run(
        cmd, cwd=POCKET_TRK.parent, capture_output=True, text=True, check=True
    )


def parse_log(log: Path) -> dict:
    """Pull the receiver's acquisition and decode results out of its log."""
    acquired, decoded = [], []
    for line in log.read_text(errors="replace").splitlines():
        fields = line.split(",")
        if line.startswith("$LOG") and "SIGNAL FOUND" in line and len(fields) >= 4:
            acquired.append({"signal": fields[2], "prn": int(fields[3])})
        elif line.startswith("$SB2") and len(fields) >= 6:
            decoded.append(
                {
                    "time_s": float(fields[1]),
                    "signal": fields[2],
                    "prn": int(fields[3]),
                    "sb2_hex80": fields[4],
                    "sb2_crc24": fields[5],
                }
            )
    return {"acquired": acquired, "decoded": decoded}


def main() -> int:
    """Run the L4/L5 demonstration and write its evidence artefact."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prn", type=int, default=3)
    parser.add_argument(
        "--frames", type=int, default=3, help="12 s each; 3 covers acquisition"
    )
    parser.add_argument("--seed", type=lambda s: int(s, 0), default=PAYLOAD_SEED)
    parser.add_argument(
        "--keep-signal", type=Path, help="write the I/Q here instead of a tempdir"
    )
    args = parser.parse_args()

    if not POCKET_TRK.is_file():
        print(
            "pocket_trk not built. Run: python scripts/interop/build_pocketsdr_afs.py",
            file=sys.stderr,
        )
        return 2

    with tempfile.TemporaryDirectory() as tmp:
        signal = args.keep_signal or Path(tmp) / "lunalink_afs_iq2.bin"
        log = Path(tmp) / "pocket_trk.log"

        print(
            f"Generating {args.frames} LunaLink frames, PRN {args.prn}, "
            f"{SAMPLE_RATE / 1e6:g} MHz, C/N0 {CN0_DBHZ:g} dB-Hz:"
        )
        transmitted = generate_signal(signal, args.prn, args.frames, args.seed)
        size_mb = signal.stat().st_size / (1024 * 1024)
        print(f"  {size_mb:.0f} MB\n\nRunning PocketSDR-AFS:")
        run_receiver(signal, log, args.prn)
        result = parse_log(log)

    decoded = result["decoded"]
    print(
        f"\n  acquired: {len(result['acquired'])} channel(s); "
        f"decoded: {len(decoded)} subframe(s)"
    )

    if not decoded:
        print("\nFAIL (L4): the receiver decoded no subframe", file=sys.stderr)
        return 1

    matches = [
        d
        for d in decoded
        if d["sb2_hex80"] == transmitted["sb2_hex80"]
        and d["sb2_crc24"] == transmitted["sb2_crc24"]
    ]
    tx, rx = transmitted, decoded[0]
    print(f"  transmitted SB2: {tx['sb2_hex80'][:32]}… crc={tx['sb2_crc24']}")
    print(f"  decoded     SB2: {rx['sb2_hex80'][:32]}… crc={rx['sb2_crc24']}")

    payload = {
        "title": "L4/L5 — third-party receiver decode of a LunaLink signal",
        "receiver": "PocketSDR-AFS (osqzss), pocket_trk",
        "generated_by": "scripts/interop/pocketsdr_afs_decode.py",
        "parameters": {
            "prn": args.prn,
            "frames": args.frames,
            "sample_rate_hz": SAMPLE_RATE,
            "cn0_dbhz": CN0_DBHZ,
            "payload": f"random(seed=0x{args.seed:08X})",
            "format": "headerless int8 I/Q, four-level {-3,-1,+1,+3} (afs_sim -b 2)",
        },
        "transmitted": transmitted,
        "received": result,
        "l4_decoded": bool(decoded),
        "l5_payload_match": len(matches) == len(decoded) and bool(matches),
    }
    EVIDENCE.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"  evidence: {EVIDENCE.relative_to(REPO_ROOT)}")

    if len(matches) != len(decoded):
        print(
            "\nFAIL (L5): a decoded subframe does not match what was sent",
            file=sys.stderr,
        )
        return 1
    print("\nPASS: PocketSDR-AFS decoded LunaLink's signal, and the payload matches.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
