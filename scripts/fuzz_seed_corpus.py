#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate a seed corpus for the libFuzzer harnesses in ``cpp/fuzz/``.

Each harness interprets its raw input in a specific way (see the harness
sources). This script builds a *valid* AFS frame with the LunaLink encoder and
serialises it into the exact byte layout each harness expects, so the fuzzer
starts from inputs that already reach deep code (a decodable frame, convergent
LDPC/BCH codewords) instead of bouncing off the early size/sync checks.

Seeds are written to ``cpp/fuzz/seeds/<harness>/`` (committed, read-only at fuzz
time). The growing corpus lives in ``corpus/`` (git-ignored).

Run:  python3 scripts/fuzz_seed_corpus.py
"""

from __future__ import annotations

import os

import numpy as np

import lunalink.afs as afs

SEEDS_ROOT = os.path.join(os.path.dirname(__file__), os.pardir, "cpp", "fuzz", "seeds")

# Frame segment layout (symbols): SYNC | SB1 | SB2 | SB3 | SB4.
SB1_START = afs.SYNC_LENGTH  # 68
PAYLOAD_START = afs.SYNC_LENGTH + afs.SB1_LENGTH  # 120
SB2_START = PAYLOAD_START  # 120
SB2_END = SB2_START + afs.SB2_LENGTH  # 2520


def _write(harness: str, name: str, payload: bytes) -> None:
    out_dir = os.path.join(SEEDS_ROOT, harness)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, name), "wb") as fh:
        fh.write(payload)


def _bipolar(symbols: np.ndarray) -> np.ndarray:
    """Map hard symbols (0/1) to bipolar soft floats: 0 -> +1.0, 1 -> -1.0."""
    return np.where(symbols == 0, 1.0, -1.0).astype(np.float32)


def gen_for_pattern(pattern: str) -> None:
    sb2, sb3, sb4 = afs.make_test_payload(pattern)
    frame_obj, _iq = afs.transmit_frame(
        prn_id=1, fid=0, toi=42, sb2_data=sb2, sb3_data=sb3, sb4_data=sb4
    )
    symbols = np.asarray(frame_obj.symbols, dtype=np.uint8)
    if symbols.size != afs.FRAME_LENGTH:
        raise SystemExit(
            f"{pattern}: encoder produced {symbols.size} symbols, expected {afs.FRAME_LENGTH}"
        )

    bipolar = _bipolar(symbols)

    # The frame must actually decode, or the seed is worthless. Raise (not
    # assert) so the check survives `python -O`.
    result = afs.receive_frame(bipolar)
    if not result.all_ok:
        raise SystemExit(f"{pattern}: encoded frame did not decode — cannot seed")

    # --- fuzz_frame_decode: full bipolar soft-symbol stream (float32 LE bytes).
    _write("frame_decode", f"frame_{pattern}", bipolar.tobytes())

    # --- fuzz_message_parser: real decoded subframe bits (one bit per byte),
    # prefixed with the selector byte that routes to the matching parser.
    _write(
        "message_parser",
        f"sb2_{pattern}",
        bytes([0]) + np.asarray(result.sb2.data, np.uint8).tobytes(),
    )
    _write(
        "message_parser",
        f"sb3_{pattern}",
        bytes([1]) + np.asarray(result.sb3.data, np.uint8).tobytes(),
    )
    _write(
        "message_parser",
        f"sb4_{pattern}",
        bytes([2]) + np.asarray(result.sb4.data, np.uint8).tobytes(),
    )

    # --- fuzz_fec: selector byte (data[0] % 6) + payload.
    # 5: LDPC SF2 codeword soft symbols (2400) -> exercises convergence path.
    _write(
        "fec", f"ldpc_sf2_{pattern}", bytes([5]) + bipolar[SB2_START:SB2_END].tobytes()
    )
    # 4: full payload soft symbols (5880) for the block deinterleaver.
    _write(
        "fec", f"deinterleave_{pattern}", bytes([4]) + bipolar[PAYLOAD_START:].tobytes()
    )
    # 3: soft BCH over the 52-symbol SB1 region.
    _write(
        "fec",
        f"bch_soft_{pattern}",
        bytes([3]) + bipolar[SB1_START:PAYLOAD_START].tobytes(),
    )
    # 2: hard BCH over the same region (0/1 symbols).
    _write(
        "fec",
        f"bch_hard_{pattern}",
        bytes([2]) + symbols[SB1_START:PAYLOAD_START].tobytes(),
    )


def gen_static_seeds() -> None:
    """Seeds for the harness branches not derived from a frame.

    Covers bit extraction, spare-bit/type-field mapping, and a valid CRC-24Q
    codeword.
    """
    rng = np.random.default_rng(0xC0FFEE)

    def bits(n: int) -> np.ndarray:
        return rng.integers(0, 2, size=n, dtype=np.uint8)

    # selector 3 -> extract_bits: offset(2) + n_bits(1) + bit array.
    _write(
        "message_parser",
        "extract_bits",
        bytes([3, 0x00, 0x10, 24]) + bits(256).tobytes(),
    )
    # selector 4 -> validate_spare_bits over an alternating 0/1 pattern.
    spare = np.tile(np.array([0, 1], dtype=np.uint8), 64).tobytes()
    _write("message_parser", "spare", bytes([4]) + spare)
    # selector 5 -> type-field mapping across the valid 0..63 range.
    _write("message_parser", "types", bytes([5]) + bytes(range(64)))

    # A valid CRC-24Q seed (selector 0 of fuzz_fec): data || crc(data) -> the
    # verify "match" branch.
    data = bits(120)
    crc = bytes(afs.crc24_calculate(data.copy()))
    _write("fec", "crc_valid", bytes([0]) + data.tobytes() + crc)


def main() -> None:
    for pattern in ("zeros", "alternating", "random"):
        try:
            gen_for_pattern(pattern)
        except Exception as exc:  # noqa: BLE001 - report and continue other patterns
            print(f"  skip pattern {pattern}: {exc}")
    gen_static_seeds()

    total = sum(len(files) for _, _, files in os.walk(SEEDS_ROOT))
    print(f"seed corpus written under {os.path.normpath(SEEDS_ROOT)} ({total} files)")


if __name__ == "__main__":
    main()
