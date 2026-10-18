# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate Level-2/3 FEC component test vectors (Phase 3) — full, byte-exact.

Writes four JSON files into ``<output_dir>`` (typically the ``fec/`` subdir of
the public ``lsis-afs-test-vectors`` checkout):

  - ``bch_vectors.json``        — BCH(51,8) encode/decode, full 52-symbol codewords
  - ``crc24_vectors.json``      — CRC-24Q input → 24-bit CRC, 8 patterns
  - ``interleaver_vectors.json``— 60×98 block interleaver, full 5880-bit I/O
  - ``ldpc_vectors.json``       — LDPC(1/2) SF2/SF3/SF4 encode, full codewords

These ship **full** inputs and outputs so a third party can byte-compare each
FEC stage in isolation — the consumer tool is ``validate.py diff-fec`` in the
public repo.  ``scripts/interop/conformance.py`` runs that comparison on
LunaLink's own output.

If the target's sibling ``frames/`` and ``inputs/`` directories are present,
a stitch-to-frames self-check runs: ``inputs/ → CRC → LDPC → interleave →
frame`` must reconstruct each shipped frame byte-exact, and ``BCH(fid, toi)``
must equal each frame's SB1 region.  A mismatch fails the export.

Companion to ``export_test_vectors_l2.py`` (frame producer); registered in
``export_test_vectors_all.py``.

Usage:
    python scripts/export_test_vectors_fec.py <output_dir>

Defaults to writing into ``../lsis-afs-test-vectors/fec/`` (sibling checkout).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

from lunalink.afs._afs import (
    BCH_CODEWORD_LENGTH,
    INTERLEAVER_COLS,
    INTERLEAVER_ROWS,
    INTERLEAVER_SIZE,
    LdpcSubframe,
    bch_decode,
    bch_encode,
    block_deinterleave,
    block_interleave,
    crc24_calculate,
    crc24_verify,
    ldpc_encode,
)

# Pinned to the LSIS V1.0 publication date for byte-stable output (consistent
# with the L1/L2/L5 producers) so two runs yield byte-identical files.
_PUB_ISO = "2025-01-29T00:00:00Z"

# SB2/SB3/SB4 pre-encode data-bit counts (LSIS V1.0 §2.4); inputs/ ships these
# unpacked (one byte per bit).  Frame payload layout: sync(68) ‖ SB1(52) ‖
# interleaved(SB2+SB3+SB4 = 5880).
_SB2_BITS, _SB3_BITS, _SB4_BITS = 1176, 846, 846
_FRAME_HEADER_LEN = 64
_SB1_OFFSET = 68

# (frame stem, fid, toi) for the stitch self-check, mirroring the public set.
# Public so a smoke test can guard it against drift from the L2 frame set.
FRAME_CASES: list[tuple[str, int, int]] = [
    ("message_1", 0, 0),
    ("message_2", 0, 0),
    ("message_3", 0, 0),
    ("message_4", 0, 0),
    ("message_5", 0, 0),
    ("boundary", 3, 99),
    ("boundary_max_fields", 3, 99),
]


def _bits_to_hex(bits: np.ndarray) -> str:
    """Pack a bit array (0/1) into MSB-first uppercase hex."""
    n = len(bits)
    pad = (8 - n % 8) % 8
    padded = np.concatenate(
        [np.asarray(bits, dtype=np.uint8), np.zeros(pad, dtype=np.uint8)]
    )
    out = []
    for i in range(0, len(padded), 8):
        byte = 0
        for j in range(8):
            byte = (byte << 1) | int(padded[i + j])
        out.append(f"{byte:02X}")
    return "".join(out)


def _bits_to_list(bits: np.ndarray) -> list[int]:
    return [int(b) for b in bits]


def _build_bch() -> dict:
    vectors = []
    for fid in range(4):
        for toi in [0, 10, 42, 55, 69, 99]:
            cw = bch_encode(fid, toi)
            r = bch_decode(cw)
            vectors.append(
                {
                    "fid": fid,
                    "toi": toi,
                    "codeword_bits": _bits_to_list(cw),
                    "codeword_hex": _bits_to_hex(cw),
                    "decoded_fid": int(r.fid),
                    "decoded_toi": int(r.toi),
                    "hamming_distance": int(r.hamming_distance),
                }
            )
    return {
        "description": "BCH(51,8) encode/decode test vectors. codeword_bits[i] is the "
        "i-th binary symbol (0 or 1, 52 total). Full codewords.",
        "spec_ref": "LSIS-AFS-501",
        "codeword_length": int(BCH_CODEWORD_LENGTH),
        "generated": _PUB_ISO,
        "vectors": vectors,
    }


def _build_crc() -> dict:
    cases = [
        ("all_zeros_8b", [0] * 8),
        ("all_ones_8b", [1] * 8),
        ("single_bit", [0, 0, 0, 0, 0, 0, 0, 1]),
        ("alternating_8b", [1, 0, 1, 0, 1, 0, 1, 0]),
        ("arbitrary_10b", [1, 0, 1, 1, 0, 0, 1, 1, 0, 1]),
        ("all_zeros_24b", [0] * 24),
        ("all_ones_24b", [1] * 24),
        ("arbitrary_48b", list(range(2)) * 24),
    ]
    vectors = []
    for desc, bits in cases:
        data = np.array(bits, dtype=np.uint8)
        crc = crc24_calculate(data)
        vectors.append(
            {
                "description": desc,
                "input_bits": bits,
                "crc_bits": _bits_to_list(crc),
                "crc_hex": _bits_to_hex(crc),
                "verify_passes": bool(crc24_verify(np.concatenate([data, crc]))),
            }
        )
    return {
        "description": (
            "CRC-24Q test vectors. Polynomial G(X)=0x864CFB, seed=0, MSB-first."
        ),
        "spec_ref": "LSIS-FID0-467",
        "crc_bits": 24,
        "generated": _PUB_ISO,
        "vectors": vectors,
    }


def _build_interleaver() -> dict:
    cases = [
        ("all_zeros", np.zeros(INTERLEAVER_SIZE, dtype=np.uint8)),
        ("all_ones", np.ones(INTERLEAVER_SIZE, dtype=np.uint8)),
        ("sequential_mod2", np.arange(INTERLEAVER_SIZE, dtype=np.uint8) % 2),
        (
            "sequential_mod2_offset",
            (np.arange(INTERLEAVER_SIZE, dtype=np.uint8) + 1) % 2,
        ),
    ]
    vectors = []
    for desc, data in cases:
        inter = block_interleave(data)
        deint = block_deinterleave(inter)
        mapping = []
        for i in range(8):
            row, col = divmod(i, INTERLEAVER_COLS)
            mapping.append(
                {"input_pos": i, "output_pos": int(col * INTERLEAVER_ROWS + row)}
            )
        vectors.append(
            {
                "description": desc,
                "input_hex": _bits_to_hex(data),
                "output_hex": _bits_to_hex(inter),
                "round_trip_ok": bool(np.array_equal(data, deint)),
                "spot_check_mapping": mapping,
            }
        )
    return {
        "description": f"60×98 block interleaver test vectors. Write row-wise "
        f"({INTERLEAVER_COLS} cols), read column-wise ({INTERLEAVER_ROWS} rows). "
        f"Total {INTERLEAVER_SIZE} bits. Full input+output hex (MSB-first).",
        "spec_ref": "LSIS-FID0-470",
        "rows": int(INTERLEAVER_ROWS),
        "cols": int(INTERLEAVER_COLS),
        "size": int(INTERLEAVER_SIZE),
        "generated": _PUB_ISO,
        "vectors": vectors,
    }


def _build_ldpc() -> dict:
    configs = [
        (LdpcSubframe.SF2, 1200, 2400, "SF2"),
        (LdpcSubframe.SF3, 870, 1740, "SF3"),
        (LdpcSubframe.SF4, 870, 1740, "SF4"),
    ]
    patterns = [
        ("all_zeros", lambda k: np.zeros(k, dtype=np.uint8)),
        ("all_ones", lambda k: np.ones(k, dtype=np.uint8)),
        (
            "alternating",
            lambda k: np.array([(i + 1) % 2 for i in range(k)], dtype=np.uint8),
        ),
        (
            "rng_seed42",
            lambda k: np.random.default_rng(42).integers(0, 2, k, dtype=np.uint8),
        ),
    ]
    vectors = []
    for sf, k, n, sf_name in configs:
        for pat_name, pat_fn in patterns:
            msg = pat_fn(k)
            cw = ldpc_encode(sf, msg)
            vectors.append(
                {
                    "subframe": sf_name,
                    "k": k,
                    "n": n,
                    "pattern": pat_name,
                    "message_hex": _bits_to_hex(msg),
                    "codeword_hex": _bits_to_hex(cw),
                    "codeword_length": int(len(cw)),
                }
            )
    return {
        "description": "LDPC encode test vectors (SF2/SF3/SF4, rate 1/2). "
        "Full codewords (message_hex -> codeword_hex, MSB-first).",
        "spec_ref": "LSIS-AFS SF2/SF3/SF4",
        "generated": _PUB_ISO,
        "vectors": vectors,
    }


def _stitch_check(repo_root: Path) -> bool | None:
    """Reconstruct each shipped frame from inputs/ via the FEC chain.

    Returns True/False if frames/ and inputs/ are present, else None (skipped).
    """
    frames_dir, inputs_dir = repo_root / "frames", repo_root / "inputs"
    if not (frames_dir.is_dir() and inputs_dir.is_dir()):
        return None
    all_ok = True
    for stem, fid, toi in FRAME_CASES:
        frame_path = frames_dir / f"frame_{stem}.bin"
        input_path = inputs_dir / f"frame_{stem}_input.bin"
        if not (frame_path.exists() and input_path.exists()):
            print(f"  SKIP stitch {stem}: frame/input missing", file=sys.stderr)
            continue
        payload = np.frombuffer(
            frame_path.read_bytes()[_FRAME_HEADER_LEN:], dtype=np.uint8
        )
        inp = np.frombuffer(input_path.read_bytes(), dtype=np.uint8)
        sb2 = inp[:_SB2_BITS]
        sb3 = inp[_SB2_BITS : _SB2_BITS + _SB3_BITS]
        sb4 = inp[_SB2_BITS + _SB3_BITS : _SB2_BITS + _SB3_BITS + _SB4_BITS]

        def _codeword(data: np.ndarray, sf: LdpcSubframe) -> np.ndarray:
            data = np.array(data, dtype=np.uint8)
            return ldpc_encode(sf, np.concatenate([data, crc24_calculate(data)]))

        bch_ok = np.array_equal(
            bch_encode(fid, toi),
            payload[_SB1_OFFSET : _SB1_OFFSET + BCH_CODEWORD_LENGTH],
        )
        cat = np.concatenate(
            [
                _codeword(sb2, LdpcSubframe.SF2),
                _codeword(sb3, LdpcSubframe.SF3),
                _codeword(sb4, LdpcSubframe.SF4),
            ]
        )
        data_ok = np.array_equal(
            block_interleave(cat), payload[_SB1_OFFSET + BCH_CODEWORD_LENGTH :]
        )
        if not (bch_ok and data_ok):
            all_ok = False
            print(f"  STITCH FAIL {stem}: BCH={bch_ok} DATA={data_ok}", file=sys.stderr)
    return all_ok


def main(argv: list[str] | None = None) -> int:
    """Write the four FEC component vector files; run the stitch self-check."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        nargs="?",
        default=None,
        help="Target fec/ directory. Defaults to ../lsis-afs-test-vectors/fec/.",
    )
    args = ap.parse_args(argv)
    out_dir: Path = args.output_dir or (
        Path(__file__).resolve().parents[1].parent / "lsis-afs-test-vectors" / "fec"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    files = {
        "bch_vectors.json": _build_bch(),
        "crc24_vectors.json": _build_crc(),
        "interleaver_vectors.json": _build_interleaver(),
        "ldpc_vectors.json": _build_ldpc(),
    }
    for name, obj in files.items():
        path = out_dir / name
        path.write_text(json.dumps(obj, indent=2) + "\n")
        print(
            f"  {name:<28} {len(obj['vectors'])} vectors ({path.stat().st_size} bytes)",
            file=sys.stderr,
        )

    stitch = _stitch_check(out_dir.parent)
    if stitch is None:
        print(
            "  stitch-to-frames: skipped (no sibling frames/ + inputs/)",
            file=sys.stderr,
        )
    elif stitch:
        print(
            "  stitch-to-frames: OK (all frames reconstruct byte-exact)",
            file=sys.stderr,
        )
    else:
        print(
            "ERROR: stitch-to-frames mismatch — FEC chain does not reproduce frames/",
            file=sys.stderr,
        )
        return 1

    print(f"OK: {len(files)} FEC vector files in {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
