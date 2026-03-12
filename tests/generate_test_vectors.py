"""LSIS-AFS interoperability test vector generator.

Generates test vectors per docs/references/interoperability.pdf.

Repository structure produced::

  test-vectors/
    codes/   prn{NNN}.hex          Gold + Weil codes in sectioned hex format
    frames/  test_message_{N}.bin  64-byte header + 6000 binary symbols
    fec/     bch_vectors.json      BCH encode/decode pairs
             crc24_vectors.json    CRC-24 input -> 24-bit CRC pairs
             interleaver_vectors.json  5880-bit round-trip pairs
             ldpc_vectors.json     LDPC SF2/SF3/SF4 encode pairs
    parsed/  frame_{N}.json        parsed subframe1 fields per interop spec

Run after building the extension module::

  python -m pip install -e . --no-build-isolation
  python tests/generate_test_vectors.py
"""

from __future__ import annotations

import json
import struct

# Adjust sys.path if running from the repo root without an install
import sys
import time
from datetime import UTC, datetime
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

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
    prn_code,
    weil1500_code,
    weil10230_code,
)

ROOT = Path(__file__).resolve().parent.parent
VECTORS = ROOT / "test-vectors"
NOW_ISO = datetime.now(UTC).isoformat(timespec="seconds")

# ---------------------------------------------------------------------------
# Secondary codes — fixed in LSIS spec (Table 11, §4.4.2).
# Each code is 4 chips, stored as a hex nibble: E=1110, 7=0111, B=1011, D=1101.
# ---------------------------------------------------------------------------
SECONDARY_CODES_HEX = ["E", "7", "B", "D"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def bits_to_hex(bits: np.ndarray) -> str:
    """Pack a bit array (values 0/1) into a compact hex string, MSB first."""
    n = len(bits)
    # Pad to multiple of 8
    pad = (8 - n % 8) % 8
    padded = np.concatenate([bits, np.zeros(pad, dtype=np.uint8)])
    out = []
    for i in range(0, len(padded), 8):
        byte = 0
        for j in range(8):
            byte = (byte << 1) | int(padded[i + j])
        out.append(f"{byte:02X}")
    return "".join(out)


def bits_to_list(bits: np.ndarray) -> list[int]:
    """Convert a bit array to a plain Python list of ints."""
    return [int(b) for b in bits]


# ---------------------------------------------------------------------------
# Level 1 — Code vectors
# ---------------------------------------------------------------------------


def generate_code_vectors(prn_ids: list[int]) -> None:
    """Write one .hex file per PRN in the interop sectioned-hex format."""
    out_dir = VECTORS / "codes"
    out_dir.mkdir(parents=True, exist_ok=True)

    for prn_id in prn_ids:
        gold = prn_code(prn_id)  # 2046 chips
        weil_p = weil10230_code(prn_id)  # 10230 chips
        weil_t = weil1500_code(prn_id)  # 1500 chips

        lines = [
            "# LSIS-AFS Code Test Vector",
            f"# PRN: {prn_id}",
            f"# Generated: {NOW_ISO}",
            "",
            "[GOLD_CODE]",
            f"length: {len(gold)}",
            f"hex: {bits_to_hex(gold)}",
            "",
            "[WEIL_PRIMARY]",
            f"length: {len(weil_p)}",
            f"hex: {bits_to_hex(weil_p)}",
            "",
            "[WEIL_TERTIARY]",
            f"length: {len(weil_t)}",
            f"hex: {bits_to_hex(weil_t)}",
            "",
        ]
        for idx, code_hex in enumerate(SECONDARY_CODES_HEX):
            lines += [f"[SECONDARY_S{idx}]", f"hex: {code_hex}", ""]

        path = out_dir / f"prn{prn_id:03d}.hex"
        path.write_text("\n".join(lines))

    print(f"[codes]  wrote {len(prn_ids)} .hex files → {out_dir}")


# ---------------------------------------------------------------------------
# Level 2 — Frame vectors (BCH-only; LDPC subframes stubbed until framer done)
# ---------------------------------------------------------------------------
#
# Binary header per interop spec (64 bytes):
#   Magic     8 B  "LSISAFS\0"
#   Version   4 B  uint32 LE = 1
#   FrameLen  4 B  uint32 LE = 6000
#   PRN       4 B  uint32 LE
#   Timestamp 8 B  int64 LE  (Unix epoch seconds)
#   Reserved 36 B  zeros
# Data: 6000 × uint8 (0x00 or 0x01)
# ---------------------------------------------------------------------------

TEST_MESSAGES = [
    # (id, description, fid, toi)
    ("1_all_zeros", "All zeros (baseline)", 0, 0),
    ("2_all_ones", "All ones (inverse baseline)", 3, 99),
    ("3_alternating", "Alternating FID/TOI", 1, 42),
    ("4_boundary", "Maximum valid values", 3, 99),
    ("5_midrange", "Mid-range values", 2, 55),
]


def _build_frame_bin(fid: int, toi: int, prn: int) -> bytes:
    """Encode SB1 and zero-pad to 6000 symbols; prepend 64-byte header."""
    codeword = bch_encode(fid, toi)  # 52 × uint8
    # Frame layout per spec: 68 sync + 52 SB1 + 5880 payload = 6000
    sync = np.zeros(68, dtype=np.uint8)
    payload = np.zeros(5880, dtype=np.uint8)
    frame = np.concatenate([sync, codeword, payload])
    assert len(frame) == 6000

    ts = int(time.time())
    header = struct.pack(
        "<8sIIIq36s",
        b"LSISAFS\x00",
        1,  # version
        6000,  # frame_length
        prn,
        ts,
        b"\x00" * 36,
    )
    assert len(header) == 64
    return header + frame.tobytes()


def generate_frame_vectors() -> None:
    """Write binary frame files with 64-byte interop header + 6000 symbols."""
    out_dir = VECTORS / "frames"
    out_dir.mkdir(parents=True, exist_ok=True)

    for msg_id, _desc, fid, toi in TEST_MESSAGES:
        data = _build_frame_bin(fid, toi, prn=1)
        path = out_dir / f"test_message_{msg_id}.bin"
        path.write_bytes(data)

    print(f"[frames] wrote {len(TEST_MESSAGES)} .bin files → {out_dir}")


# ---------------------------------------------------------------------------
# FEC vectors — BCH, CRC-24, Interleaver, LDPC
# ---------------------------------------------------------------------------


def generate_bch_vectors() -> None:
    """Write BCH encode/decode test vectors (24 input/output pairs)."""
    vectors = []
    for fid in range(4):
        for toi in [0, 10, 42, 55, 69, 99]:
            cw = bch_encode(fid, toi)
            result = bch_decode(cw)
            vectors.append(
                {
                    "fid": fid,
                    "toi": toi,
                    "codeword_bits": bits_to_list(cw),
                    "codeword_hex": bits_to_hex(cw),
                    "decoded_fid": int(result.fid.value),
                    "decoded_toi": int(result.toi.value),
                    "hamming_distance": int(result.hamming_distance),
                }
            )

    out = {
        "description": "BCH(51,8) encode/decode test vectors. "
        "codeword_bits[i] is the i-th binary symbol "
        "(0 or 1, 52 total).",
        "spec_ref": "LSIS-AFS-501",
        "codeword_length": BCH_CODEWORD_LENGTH,
        "generated": NOW_ISO,
        "vectors": vectors,
    }
    path = VECTORS / "fec" / "bch_vectors.json"
    path.write_text(json.dumps(out, indent=2))
    print(f"[fec]    wrote {len(vectors)} BCH vectors → {path}")


def generate_crc24_vectors() -> None:
    """Write CRC-24Q input/output test vectors (8 patterns)."""
    test_cases = [
        # (description, input_bits as list)
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
    for desc, bits in test_cases:
        data = np.array(bits, dtype=np.uint8)
        crc = crc24_calculate(data)
        appended = np.concatenate([data, crc])
        vectors.append(
            {
                "description": desc,
                "input_bits": bits,
                "crc_bits": bits_to_list(crc),
                "crc_hex": bits_to_hex(crc),
                "verify_passes": bool(crc24_verify(appended)),
            }
        )

    out = {
        "description": "CRC-24Q test vectors. Polynomial G(X)=0x864CFB, seed=0, MSB-first.",  # noqa: E501
        "spec_ref": "LSIS-FID0-467",
        "crc_bits": 24,
        "generated": NOW_ISO,
        "vectors": vectors,
    }
    path = VECTORS / "fec" / "crc24_vectors.json"
    path.write_text(json.dumps(out, indent=2))
    print(f"[fec]    wrote {len(vectors)} CRC-24 vectors → {path}")


def generate_interleaver_vectors() -> None:
    """Write 60x98 block interleaver round-trip vectors with spot-check mapping."""
    test_cases = [
        # (description, seed or pattern)
        ("all_zeros", np.zeros(INTERLEAVER_SIZE, dtype=np.uint8)),
        ("all_ones", np.ones(INTERLEAVER_SIZE, dtype=np.uint8)),
        ("sequential_mod2", np.arange(INTERLEAVER_SIZE, dtype=np.uint8) % 2),
        (
            "sequential_mod2_offset",
            (np.arange(INTERLEAVER_SIZE, dtype=np.uint8) + 1) % 2,
        ),
    ]

    vectors = []
    for desc, data in test_cases:
        interleaved = block_interleave(data)
        deinterleaved = block_deinterleave(interleaved)
        round_trip_ok = bool(np.array_equal(data, deinterleaved))

        # Spot-check: first 8 input positions and where they map in the output
        mapping = []
        for i in range(8):
            row, col = divmod(i, INTERLEAVER_COLS)
            out_pos = col * INTERLEAVER_ROWS + row
            mapping.append({"input_pos": i, "output_pos": int(out_pos)})

        vectors.append(
            {
                "description": desc,
                "input_hex_first32": bits_to_hex(data[:32]),
                "output_hex_first32": bits_to_hex(interleaved[:32]),
                "round_trip_ok": round_trip_ok,
                "spot_check_mapping": mapping,
            }
        )

    out = {
        "description": (
            f"60×98 block interleaver test vectors. "
            f"Write row-wise ({INTERLEAVER_COLS} cols), "
            f"read column-wise ({INTERLEAVER_ROWS} rows). "
            f"Total size: {INTERLEAVER_SIZE} bits."
        ),
        "spec_ref": "LSIS-FID0-470",
        "rows": INTERLEAVER_ROWS,
        "cols": INTERLEAVER_COLS,
        "size": INTERLEAVER_SIZE,
        "generated": NOW_ISO,
        "vectors": vectors,
    }
    path = VECTORS / "fec" / "interleaver_vectors.json"
    path.write_text(json.dumps(out, indent=2))
    print(f"[fec]    wrote {len(vectors)} interleaver vectors → {path}")


def generate_ldpc_vectors() -> None:
    """Write LDPC encode test vectors for SF2, SF3, and SF4 (12 total)."""
    subframe_configs = [
        (LdpcSubframe.SF2, 1200, 2400, "SF2"),
        (LdpcSubframe.SF3, 870, 1740, "SF3"),
        (LdpcSubframe.SF4, 870, 1740, "SF4"),
    ]

    test_patterns = [
        ("all_zeros", lambda k: np.zeros(k, dtype=np.uint8)),
        ("all_ones", lambda k: np.ones(k, dtype=np.uint8)),
        ("alternating", lambda k: np.array([i % 2 for i in range(k)], dtype=np.uint8)),
        (
            "rng_seed42",
            lambda k: np.random.default_rng(42).integers(0, 2, k, dtype=np.uint8),
        ),
    ]

    vectors = []
    for sf, k, n, sf_name in subframe_configs:
        for pat_name, pat_fn in test_patterns:
            msg = pat_fn(k)
            codeword = ldpc_encode(sf, msg)
            vectors.append(
                {
                    "subframe": sf_name,
                    "k": k,
                    "n": n,
                    "pattern": pat_name,
                    "message_hex": bits_to_hex(msg),
                    "codeword_hex_first64b": bits_to_hex(codeword[:64]),
                    "codeword_hex_last64b": bits_to_hex(codeword[-64:]),
                    "codeword_length": len(codeword),
                }
            )

    out = {
        "description": (
            "5G NR LDPC encode test vectors (SF2/SF3/SF4). "
            "codeword_hex fields are truncated to first/last 64 bits; "
            "run ldpc_encode() to reproduce the full codeword."
        ),
        "spec_ref": "LSIS-AFS SF2/SF3/SF4",
        "generated": NOW_ISO,
        "vectors": vectors,
    }
    path = VECTORS / "fec" / "ldpc_vectors.json"
    path.write_text(json.dumps(out, indent=2))
    print(f"[fec]    wrote {len(vectors)} LDPC vectors → {path}")


# ---------------------------------------------------------------------------
# Parsed frame JSON (Level 5 format)
# ---------------------------------------------------------------------------


def generate_parsed_vectors() -> None:
    """Write parsed subframe JSON files in the interop Level-5 format."""
    out_dir = VECTORS / "parsed"
    out_dir.mkdir(parents=True, exist_ok=True)

    for msg_id, desc, fid, toi in TEST_MESSAGES:
        parsed = {
            "version": "1.0",
            "timestamp": NOW_ISO,
            "frame_id": f"test_{msg_id}",
            "description": desc,
            "subframe1": {
                "fid": fid,
                "toi": toi,
            },
            "subframe2": None,
            "subframe3": None,
            "subframe4": None,
            "notes": "subframes 2-4 not yet implemented (GD3.2 pending)",
        }
        path = out_dir / f"frame_{msg_id}.json"
        path.write_text(json.dumps(parsed, indent=2))

    print(f"[parsed] wrote {len(TEST_MESSAGES)} .json files → {out_dir}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    """Generate all test vector files under test-vectors/."""
    print(f"LunaLink test vector generator — {NOW_ISO}")
    print(f"Output root: {VECTORS}\n")

    # Level 1: first 12 interim test PRN IDs (Table 11)
    prn_ids = list(range(1, 13))
    generate_code_vectors(prn_ids)
    generate_frame_vectors()
    generate_bch_vectors()
    generate_crc24_vectors()
    generate_interleaver_vectors()
    generate_ldpc_vectors()
    generate_parsed_vectors()

    print("\nDone. To validate the vectors against this implementation:")
    print("  python tests/validate_test_vectors.py")


if __name__ == "__main__":
    main()
