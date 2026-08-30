r"""Generate Level 2 interoperability test vectors (encoded frames).

Writes one ``frame_{name}.bin`` file per test message into the target
directory, in the binary format defined by the competition interoperability
plan (64-byte ``LSISAFS\\0`` header + 6000 unpacked symbols).

Six frames are produced:

  - ``frame_message_1.bin`` — Test Message 1: all-zeros input bits.
  - ``frame_message_2.bin`` — Test Message 2: all-ones input bits.
  - ``frame_message_3.bin`` — Test Message 3: alternating bit pattern
    starting with 1 (first packed byte = 0xAA, MSB-first), matching the
    interop doc's ``"Alternating pattern (0xAA…)"`` annotation literally.
    FAQ Q21's start-with-0 rule is scoped to spare bits, not test
    messages, so it does not apply here.
  - ``frame_message_4.bin`` — Test Message 4: bytewise marker pattern.
    The SB2/SB3/SB4 input bytes are ``bytes(range(0, ceil(N/8)))``,
    bit-unpacked MSB-first and truncated to the subframe data-bit
    count.  Each byte position carries a unique signature, so a single
    encoder bug surfaces at a localised offset.
  - ``frame_message_5.bin`` — Test Message 5: xorshift32 PRNG with
    seed = 0xAF52.  After each iteration, the LSB of the new state
    is one input bit.  See CORRECTNESS.md in the public test-vector
    repo for the algorithm written in portable C.
  - ``frame_boundary.bin`` — Test Case 4 boundary frame: PRN=210 (max),
    FID=3 (max), TOI=99 (per Test Case 4 in the interop doc).  Input
    bits use the alternating pattern.
  - ``frame_boundary_max_fields.bin`` — Test Case 4 SB2 maxima: WN=8191
    (max, 13 bits all-ones) and ITOW=503 (max, 9-bit MSB-first bit
    pattern) packed into SB2[0..21] per V1.0 Table 22; remaining SB2,
    SB3, SB4 bits are all-ones.  PRN=210, FID=3, TOI=99 as for
    ``frame_boundary``.

Intended to target the public ``lsis-afs-test-vectors`` repository so that
regenerating the distribution is a one-shot command.  Companion to
``export_test_vectors_l1.py``.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from collections.abc import Callable
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from lunalink.afs.vectors import export_frame_vector

SB2_BITS = 1176
SB3_BITS = 846
SB4_BITS = 846
TOTAL_BITS = SB2_BITS + SB3_BITS + SB4_BITS  # 2868

XORSHIFT32_SEED = 0xAF52  # documented in CORRECTNESS.md

# Deterministic header timestamp: LSIS V1.0 publication date,
# 2025-01-29T00:00:00Z, expressed as Unix epoch seconds. Pinning this
# keeps the SHA-256 of every frame_*.bin reproducible across rebuilds.
HEADER_TIMESTAMP = 1738108800


def _zeros() -> tuple[NDArray, NDArray, NDArray]:
    return (
        np.zeros(SB2_BITS, dtype=np.uint8),
        np.zeros(SB3_BITS, dtype=np.uint8),
        np.zeros(SB4_BITS, dtype=np.uint8),
    )


def _ones() -> tuple[NDArray, NDArray, NDArray]:
    return (
        np.ones(SB2_BITS, dtype=np.uint8),
        np.ones(SB3_BITS, dtype=np.uint8),
        np.ones(SB4_BITS, dtype=np.uint8),
    )


def _alternating0() -> tuple[NDArray, NDArray, NDArray]:
    # bit_i = i mod 2  →  [0, 1, 0, 1, ...], MSB-first packed first byte = 0x55.
    # Used by frame_boundary (Test Case 4): the interop doc pins no starting
    # bit there, so we keep the existing convention.
    return (
        (np.arange(SB2_BITS) % 2).astype(np.uint8),
        (np.arange(SB3_BITS) % 2).astype(np.uint8),
        (np.arange(SB4_BITS) % 2).astype(np.uint8),
    )


def _alternating1() -> tuple[NDArray, NDArray, NDArray]:
    # bit_i = (i + 1) mod 2  →  [1, 0, 1, 0, ...], MSB-first packed first byte = 0xAA.
    # Used by Test Message 3: matches the interop doc's "0xAA…" annotation.
    return (
        ((np.arange(SB2_BITS) + 1) % 2).astype(np.uint8),
        ((np.arange(SB3_BITS) + 1) % 2).astype(np.uint8),
        ((np.arange(SB4_BITS) + 1) % 2).astype(np.uint8),
    )


def _bytewise_marker() -> tuple[NDArray, NDArray, NDArray]:
    """Each input byte position carries its index mod 256.

    Bytes are unpacked MSB-first and truncated to the subframe bit count.
    """

    def fill(bit_count: int) -> NDArray:
        n_bytes = (bit_count + 7) // 8
        seq = bytes(i % 256 for i in range(n_bytes))
        bits = np.unpackbits(np.frombuffer(seq, dtype=np.uint8))
        return bits[:bit_count].astype(np.uint8)

    return (fill(SB2_BITS), fill(SB3_BITS), fill(SB4_BITS))


def _xorshift32_bits(seed: int, count: int) -> NDArray:
    """Generate ``count`` bits via xorshift32; bit i is state & 1 after iter i+1.

    Reference C:
        uint32_t s = seed;
        for (int i = 0; i < count; i++) {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            bits[i] = s & 1u;
        }
    """
    state = seed & 0xFFFFFFFF
    out = np.empty(count, dtype=np.uint8)
    for i in range(count):
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        out[i] = state & 1
    return out


def _random_seeded() -> tuple[NDArray, NDArray, NDArray]:
    bits = _xorshift32_bits(XORSHIFT32_SEED, TOTAL_BITS)
    return (
        bits[:SB2_BITS],
        bits[SB2_BITS : SB2_BITS + SB3_BITS],
        bits[SB2_BITS + SB3_BITS :],
    )


# SB2 WN/ITOW field widths and maxima per V1.0 Table 22 / §2.4.3.1.6.
_WN_BITS = 13
_ITOW_BITS = 9
_WN_MAX = 8191  # 2**13 - 1
_ITOW_MAX = 503


def _max_fields() -> tuple[NDArray, NDArray, NDArray]:
    """SB2 boundary maxima: WN=8191, ITOW=503 packed MSB-first into SB2[0..21].

    Bits 0..12 carry WN, bits 13..21 carry ITOW (V1.0 Table 22).  Remaining
    SB2 bits and the entire SB3/SB4 are all-ones, mirroring ``_ones``.  The
    spare-bits region SB2[1150..1175] is overwritten by the per-spec
    FAQ-Q21 fixup applied in the encoder pipeline.
    """
    sb2 = np.ones(SB2_BITS, dtype=np.uint8)
    for i in range(_WN_BITS):
        sb2[i] = (_WN_MAX >> (_WN_BITS - 1 - i)) & 1
    for i in range(_ITOW_BITS):
        sb2[_WN_BITS + i] = (_ITOW_MAX >> (_ITOW_BITS - 1 - i)) & 1
    return (sb2, np.ones(SB3_BITS, dtype=np.uint8), np.ones(SB4_BITS, dtype=np.uint8))


PatternFn = Callable[[], tuple[NDArray, NDArray, NDArray]]

# (filename, prn, fid, toi, pattern_fn)
TEST_MESSAGES: list[tuple[str, int, int, int, PatternFn]] = [
    ("frame_message_1.bin", 1, 0, 0, _zeros),
    ("frame_message_2.bin", 1, 0, 0, _ones),
    ("frame_message_3.bin", 1, 0, 0, _alternating1),
    ("frame_message_4.bin", 1, 0, 0, _bytewise_marker),
    ("frame_message_5.bin", 1, 0, 0, _random_seeded),
    ("frame_boundary.bin", 210, 3, 99, _alternating0),
    ("frame_boundary_max_fields.bin", 210, 3, 99, _max_fields),
]


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    """Emit the Level 2 encoded-frame vectors for the public repo."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        help="Target directory (typically the frames/ subdir of lsis-afs-test-vectors)",
    )
    args = ap.parse_args(argv)

    out_dir: Path = args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    for filename, prn, fid, toi, pattern_fn in TEST_MESSAGES:
        sb2, sb3, sb4 = pattern_fn()
        out_path = out_dir / filename
        export_frame_vector(
            prn, fid, toi, sb2, sb3, sb4, out_path, timestamp=HEADER_TIMESTAMP
        )
        size = out_path.stat().st_size
        sha = _sha256(out_path)[:16]
        print(
            f"  {filename:<22}  PRN={prn:>3}  FID={fid}  TOI={toi:>3}  "
            f"({size} bytes, sha256={sha}…)",
            file=sys.stderr,
        )

    print(f"OK: {len(TEST_MESSAGES)} frames in {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
