"""LSIS-AFS interoperability test vector generator.

Generates test vectors per docs/references/interoperability.pdf.

Repository structure produced::

  test-vectors/
    codes/   codes_prn{N}.hex       Gold + Weil codes in sectioned hex format
    frames/  frame_{N}.bin          64-byte LSISAFS header + 6000 binary symbols
    fec/     bch_vectors.json       BCH encode/decode pairs
             crc24_vectors.json     CRC-24 input -> 24-bit CRC pairs
             interleaver_vectors.json  5880-bit round-trip pairs
             ldpc_vectors.json      LDPC SF2/SF3/SF4 encode pairs
    signals/ signal_prn{N}_{rate}.iq.gz  128-byte LSISIQ header + float32 I/Q
    parsed/  parsed_{N}.json        parsed subframe fields per interop spec
    decode/  decode_{N}.json.gz     expected decoder outputs

Run after building the extension module::

  python -m pip install -e . --no-build-isolation
  python tests/generate_test_vectors.py
"""

from __future__ import annotations

import gzip
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
from lunalink.afs.signal import (
    frame_build,
    generate_iq_epoch,
    generate_iq_frame,
    write_iq_file,
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


def _gzip_replace(path: Path) -> None:
    """Gzip a file in-place: read, compress, write as path.gz, remove original."""
    data = path.read_bytes()
    gz_path = path.with_suffix(path.suffix + ".gz")
    gz_path.write_bytes(gzip.compress(data))
    path.unlink()


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

        path = out_dir / f"codes_prn{prn_id}.hex"
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
    # (id, description, fid, toi, sb_pattern)
    (1, "All zeros (baseline)", 0, 0, "zeros"),
    (2, "All ones (inverse baseline)", 3, 99, "ones"),
    (3, "Alternating FID/TOI", 1, 42, "alternating"),
    (4, "Keplerian ephemeris (NRHO)", 0, 42, "ephemeris"),
    (5, "Mid-range values", 2, 55, "random"),
]


def _make_ephemeris_sb2(n: int) -> np.ndarray:
    """Build a realistic SB2 payload with Keplerian CED from a lunar NRHO orbit.

    Uses PRN 1 orbital elements from LANS-AFS-SIM's default almanac:
    ecc=0.6, sqrtA=2557.34, inc0=0.9826 rad, omg0=0, aop=pi/2, M0=0.
    Encoding per LSIS V1.0 §2.4.3.1.6 / LANS-AFS-SIM eph2sbf().
    """
    import math

    PI = math.pi  # noqa: N806
    POW2_M19 = 2**-19  # noqa: N806
    POW2_M31 = 2**-31  # noqa: N806
    POW2_M32 = 2**-32  # noqa: N806
    POW2_M43 = 2**-43  # noqa: N806

    # PRN 1 Keplerian elements (LANS-AFS-SIM default_almanac.txt)
    toe_sec = 319488.0
    ecc = 0.6
    sqrta = 2557.342371
    inc0 = 0.9826203689
    omg0 = 0.0
    aop = PI / 2
    m0 = 0.0
    af0 = 0.0
    af1 = 0.0

    def _pack_bits(val: int, nbits: int) -> list[int]:
        """Unpack integer to MSB-first bit list."""
        return [(val >> (nbits - 1 - i)) & 1 for i in range(nbits)]

    # Quantise per LANS-AFS-SIM scaling
    toe_q = int(toe_sec / 16.0) & 0xFFFF
    ecc_q = int(ecc / POW2_M32) & 0xFFFFFFFF
    sqrta_q = int(sqrta / POW2_M19) & 0xFFFFFFFF
    inc0_q = int(inc0 / POW2_M31 / PI) & 0xFFFFFFFF
    omg0_q = int(omg0 / POW2_M31 / PI) & 0xFFFFFFFF
    aop_q = int(aop / POW2_M31 / PI) & 0xFFFFFFFF
    m0_q = int(m0 / POW2_M31 / PI) & 0xFFFFFFFF
    toc_q = toe_q
    af0_q = int(af0 / POW2_M31) & 0x3FFFFF  # 22 bits
    af1_q = int(af1 / POW2_M43) & 0xFFFF  # 16 bits

    # Build 262-bit CED field (bits 22-283 of SB2 per sb2_bit_layout.csv)
    ced_bits = (
        _pack_bits(toe_q, 16)
        + _pack_bits(ecc_q, 32)
        + _pack_bits(sqrta_q, 32)
        + _pack_bits(inc0_q, 32)
        + _pack_bits(omg0_q, 32)
        + _pack_bits(aop_q, 32)
        + _pack_bits(m0_q, 32)
        + _pack_bits(toc_q, 16)
        + _pack_bits(af0_q, 22)
        + _pack_bits(af1_q, 16)
    )

    sb2 = np.zeros(n, dtype=np.uint8)
    # WN and ITOW at bits 0-21 are left as zero (filled by frame builder)
    # CED starts at bit 30 per sb2_bit_layout.csv (after 8-bit Health)
    for i, bit in enumerate(ced_bits):
        if 30 + i < n:
            sb2[30 + i] = bit
    return sb2


def _build_frame_bin(fid: int, toi: int, prn: int, sb2_pattern: str = "zeros") -> bytes:
    """Build a complete frame using frame_build and prepend 64-byte header."""
    patterns = {
        "zeros": lambda n: np.zeros(n, dtype=np.uint8),
        "ones": lambda n: np.ones(n, dtype=np.uint8),
        # bit_i = (i + 1) mod 2  →  [1, 0, 1, 0, ...], MSB-first first byte = 0xAA.
        # Matches the interop doc's "Alternating pattern (0xAA…)" annotation literally
        # and the public lsis-afs-test-vectors v0.2.0 convention.
        "alternating": lambda n: np.array(
            [(i + 1) % 2 for i in range(n)], dtype=np.uint8
        ),
        "random": lambda n: np.random.default_rng(0x12345678).integers(
            0, 2, n, dtype=np.uint8
        ),
    }

    if sb2_pattern == "ephemeris":
        sb2 = _make_ephemeris_sb2(1176)
    else:
        pat_fn = patterns.get(sb2_pattern, patterns["zeros"])
        sb2 = pat_fn(1176)

    pat_fn = patterns.get(sb2_pattern, patterns["zeros"])
    sb3 = pat_fn(846)
    sb4 = pat_fn(846)

    frame = frame_build(fid, toi, sb2, sb3, sb4)
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

    for msg_id, _desc, fid, toi, sb_pat in TEST_MESSAGES:
        data = _build_frame_bin(fid, toi, prn=1, sb2_pattern=sb_pat)
        path = out_dir / f"frame_{msg_id}.bin"
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
                    "decoded_fid": int(result.fid),
                    "decoded_toi": int(result.toi),
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
        # bit_i = (i + 1) mod 2 → first packed byte 0xAA, matches the interop-doc
        # convention used by frame_3.bin and the public lsis-afs-test-vectors set.
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

    for msg_id, desc, fid, toi, _sb_pat in TEST_MESSAGES:
        parsed = {
            "version": "1.0",
            "timestamp": NOW_ISO,
            "frame_id": f"test_{msg_id}",
            "description": desc,
            "subframe1": {
                "fid": fid,
                "toi": toi,
            },
            "subframe2": {"data_bits": 1176, "crc_bits": 24, "total_bits": 1200},
            "subframe3": {"data_bits": 846, "crc_bits": 24, "total_bits": 870},
            "subframe4": {"data_bits": 846, "crc_bits": 24, "total_bits": 870},
        }
        path = out_dir / f"parsed_{msg_id}.json"
        path.write_text(json.dumps(parsed, indent=2))

    print(f"[parsed] wrote {len(TEST_MESSAGES)} .json files → {out_dir}")


# ---------------------------------------------------------------------------
# Level 3 — I/Q signal vectors
# ---------------------------------------------------------------------------


def generate_iq_vectors(prn_ids: list[int]) -> None:
    """Write I/Q signal files for interoperability Level 3 validation.

    Generates one 2 ms epoch per PRN at native 5.115 MHz and at 10.23 MHz,
    using the 128-byte LSISIQ header format.
    """
    out_dir = VECTORS / "signals"
    out_dir.mkdir(parents=True, exist_ok=True)

    native_rate = 5_115_000.0
    upsampled_rate = 10_230_000.0

    for prn_id in prn_ids:
        # Native rate: 1 sample/chip (5.115 MHz)
        iq_native = generate_iq_epoch(prn_id, epoch_idx=0, data_symbol=1)
        path_native = out_dir / f"signal_prn{prn_id}_5115ksps.iq"
        write_iq_file(
            str(path_native), iq_native, native_rate, prn_id, duration_s=0.002
        )
        _gzip_replace(path_native)

        # Upsampled: 10.23 MHz (2x oversampling)
        iq_up = generate_iq_epoch(
            prn_id, epoch_idx=0, data_symbol=1, sample_rate=upsampled_rate
        )
        path_up = out_dir / f"signal_prn{prn_id}_10230ksps.iq"
        write_iq_file(str(path_up), iq_up, upsampled_rate, prn_id, duration_s=0.002)
        _gzip_replace(path_up)

    print(f"[signal] wrote {len(prn_ids) * 2} epoch .iq.gz files → {out_dir}")

    # Full 12-second frame signal at 10.23 MHz (spec baseline rate) for PRN 1
    # Uses Test Message 1 (all zeros) as navigation data.
    prn_id = 1
    sb2 = np.zeros(1176, dtype=np.uint8)
    sb3 = np.zeros(846, dtype=np.uint8)
    sb4 = np.zeros(846, dtype=np.uint8)
    frame_syms = frame_build(0, 0, sb2, sb3, sb4)
    iq_frame = generate_iq_frame(prn_id, frame_syms, sample_rate=upsampled_rate)
    path_frame = out_dir / f"signal_prn{prn_id}_12s.iq"
    write_iq_file(str(path_frame), iq_frame, upsampled_rate, prn_id, duration_s=12.0)
    _gzip_replace(path_frame)
    print(f"[signal] wrote 12 s frame signal → {path_frame}.gz")


# ---------------------------------------------------------------------------
# Decode-direction vectors — expected decoder outputs for frame .bin files
# ---------------------------------------------------------------------------


def generate_decode_vectors() -> None:
    """Generate expected decode outputs for each standard test message.

    For each of the 5 test messages, derive the expected decoder output
    independently from the spec (not by running the DUT):
    - FID, TOI: from the frame_build input parameters
    - SB2/SB3/SB4 data: from the input bit patterns
    - WN, ITOW: extracted from SB2 input bits 0..12 and 13..21 (MSB-first)
    - ToT: computed from LSIS-720 formula (WN×604800 + ITOW×1200 + TOI×12)

    A third party can validate their decoder against these vectors.
    """
    out_dir = VECTORS / "decode"
    out_dir.mkdir(parents=True, exist_ok=True)

    patterns = {
        "zeros": lambda n: np.zeros(n, dtype=np.uint8),
        "ones": lambda n: np.ones(n, dtype=np.uint8),
        # bit_i = (i + 1) mod 2 → first packed byte 0xAA — keeps the decode-direction
        # vectors aligned with frame_3.bin's input bits.
        "alternating": lambda n: np.array(
            [(i + 1) % 2 for i in range(n)], dtype=np.uint8
        ),
        "sparse": lambda n: np.array(
            [1 if i % 17 == 0 else 0 for i in range(n)], dtype=np.uint8
        ),
        "random": lambda n: np.random.default_rng(0x12345678).integers(
            0, 2, n, dtype=np.uint8
        ),
    }

    for msg_id, desc, fid, toi, sb_pat in TEST_MESSAGES:
        if sb_pat == "ephemeris":
            sb2_data = _make_ephemeris_sb2(1176)
        else:
            sb2_data = patterns.get(sb_pat, patterns["zeros"])(1176)
        pat_fn = patterns.get(sb_pat, patterns["zeros"])
        sb3_data = pat_fn(846)
        sb4_data = pat_fn(846)

        # FAQ Q21 / LSIS-300: SB2 spare bits at offsets 1150..1175 carry
        # the spec-mandated alternating-0/1 pattern starting with 0,
        # written by frame_build() unconditionally. Mirror that here so
        # the expected SB2 matches what the decoder will recover.
        for i in range(26):
            sb2_data[1150 + i] = i % 2

        # Derive WN and ITOW independently from the SB2 input bits, NOT from
        # the DUT's parse_sb2().  Per LSIS V1.0 §2.4.3.1.6 (LSIS-FID0-520):
        #   WN   = bits 0..12  (13 bits, MSB first)
        #   ITOW = bits 13..21 ( 9 bits, MSB first)
        wn = 0
        for i in range(13):
            wn = (wn << 1) | int(sb2_data[i])
        itow = 0
        for i in range(13, 22):
            itow = (itow << 1) | int(sb2_data[i])

        # Compute ToT from the spec formula (LSIS-720), not from compute_tot():
        #   t_F = WN * SECWEEK + ITOW * BI_d + TOI * F_d
        # where SECWEEK=604800, BI_d=1200, F_d=12.
        # ToT is valid when all fields are within range.
        wn_valid = 0 <= wn <= 8191
        itow_valid = 0 <= itow <= 503
        toi_valid = 0 <= toi <= 99
        tot_valid = wn_valid and itow_valid and toi_valid
        tot_seconds = (wn * 604800 + itow * 1200 + toi * 12) if tot_valid else None

        vector = {
            "description": f"Decode vector: {desc}",
            "spec_ref": "Gateway 7 — decode-direction validation",
            "input_file": f"../frames/frame_{msg_id}.bin",
            "expected": {
                "fid": fid,
                "toi": toi,
                "sb2_data": sb2_data.tolist(),
                "sb3_data": sb3_data.tolist(),
                "sb4_data": sb4_data.tolist(),
                "wn": wn,
                "itow": itow,
                "tot_valid": tot_valid,
                "tot_seconds": tot_seconds,
            },
        }

        path = out_dir / f"decode_{msg_id}.json.gz"
        path.write_bytes(gzip.compress(json.dumps(vector, indent=2).encode()))

    print(f"[decode] wrote {len(TEST_MESSAGES)} decode vectors → {out_dir}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    """Generate all test vector files under test-vectors/."""
    print(f"LunaLink test vector generator — {NOW_ISO}")
    print(f"Output root: {VECTORS}\n")

    # Ensure all output directories exist
    for subdir in ("codes", "frames", "fec", "signals", "parsed", "decode"):
        (VECTORS / subdir).mkdir(parents=True, exist_ok=True)

    # Level 1: first 12 interim test PRN IDs (Table 11)
    prn_ids = list(range(1, 13))
    generate_code_vectors(prn_ids)
    generate_frame_vectors()
    generate_bch_vectors()
    generate_crc24_vectors()
    generate_interleaver_vectors()
    generate_ldpc_vectors()
    generate_iq_vectors(prn_ids)
    generate_parsed_vectors()
    generate_decode_vectors()

    print("\nDone. To validate the vectors against this implementation:")
    print("  python tests/validate_test_vectors.py")


if __name__ == "__main__":
    main()
