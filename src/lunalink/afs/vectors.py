"""Interoperability test vector generation in competition format.

Produces full-length, bit-exact vectors for cross-team validation
at all 5 interop levels defined in the competition spec.
"""

from __future__ import annotations

import json
import os
import struct
from datetime import UTC, datetime

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._receiver import receive_frame
from lunalink.afs._results import DecodeResult
from lunalink.afs._transmitter import make_test_payload, transmit_frame
from lunalink.afs.signal import (
    AFS_Q_CHIP_RATE,
    FRAME_LENGTH,
    frame_build,
    prn_code,
    weil1500_code,
    weil10230_code,
    write_iq_file,
)

StrPath = str | os.PathLike[str]


# LSIS V1.0 publication date — pinned for byte-stable reference-vector
# output across rebuilds.  Used as the default in every producer that
# stamps a timestamp into its output (L1 hex header, L2 frame header,
# L5 parsed JSON).  ``Z`` suffix matches the interop-PDF Parsed Data
# Export Format example.
_LSIS_V1_TIMESTAMP_ISO = "2025-01-29T00:00:00Z"
_LSIS_VERSION_LABEL = "V1.0 (29 January 2025)"


def _bits_to_hex(bits: NDArray[np.uint8]) -> str:
    """Pack a bit array to a hex string (trailing-LSB zero pad to full bytes)."""
    pad = (8 - len(bits) % 8) % 8
    if pad:
        bits = np.concatenate([bits, np.zeros(pad, dtype=np.uint8)])
    return np.packbits(bits).tobytes().hex()


_HEX_CHARS = np.array(list("0123456789ABCDEF"))


def _bits_to_hex_annex3(bits: NDArray[np.uint8], prepend_zeros: int = 0) -> str:
    """Encode a chip sequence as upper-case hex per LNIS AD1 Volume A Annex 3.

    Annex 3 packs bits MSB-first in 4-bit nibbles (not whole bytes).  For the
    2046- and 10230-chip codes, two zero bits are prepended on the MSB side so
    the total bit count is divisible by 4.  The 1500-chip code has no padding
    (it yields an odd number of hex digits).
    """
    if prepend_zeros:
        bits = np.concatenate([np.zeros(prepend_zeros, dtype=np.uint8), bits])
    if len(bits) % 4 != 0:
        msg = (
            f"bit count {len(bits)} not divisible by 4 "
            f"after {prepend_zeros} prepend zeros"
        )
        raise ValueError(msg)
    nibbles = bits.reshape(-1, 4)
    values = (
        (nibbles[:, 0].astype(np.uint8) << 3)
        | (nibbles[:, 1].astype(np.uint8) << 2)
        | (nibbles[:, 2].astype(np.uint8) << 1)
        | nibbles[:, 3].astype(np.uint8)
    )
    return "".join(_HEX_CHARS[values])


# AFS-Q secondary codes (4 bits each), per LNIS AD1 Volume A Annex 3, Table 2.
_SECONDARY_CODES_HEX: tuple[str, ...] = ("E", "7", "B", "D")


def export_code_vectors(
    prn_id: int,
    output_dir: StrPath,
    timestamp_iso: str | None = None,
) -> str:
    """Export spreading codes for one PRN in hex format (Level 1).

    Pass an explicit ``timestamp_iso`` (ISO-8601 string) for byte-stable
    output suitable for shipping as a published test vector; defaults to
    the LSIS V1.0 publication date for reference-implementation
    determinism, matching ``export_frame_vector`` and
    ``export_parsed_json_interop_v1``.

    Returns the path to the written file.
    """
    os.makedirs(output_dir, exist_ok=True)
    path = os.path.join(output_dir, f"codes_prn{prn_id:03d}.hex")

    gold = prn_code(prn_id)
    weil_p = weil10230_code(prn_id)
    weil_t = weil1500_code(prn_id)

    ts = timestamp_iso if timestamp_iso is not None else _LSIS_V1_TIMESTAMP_ISO

    lines = [
        "# LSIS-AFS Code Test Vector",
        f"# PRN: {prn_id}",
        f"# Generated: {ts}",
        "# Implementation: LunaLink",
        "# Encoding: LNIS AD1 Volume A Annex 3 (MSB-first, 4-bit nibbles)",
        "",
        "[GOLD_CODE]",
        f"length: {len(gold)}",
        f"hex: {_bits_to_hex_annex3(gold, prepend_zeros=2)}",
        "",
        "[WEIL_PRIMARY]",
        f"length: {len(weil_p)}",
        f"hex: {_bits_to_hex_annex3(weil_p, prepend_zeros=2)}",
        "",
        "[WEIL_TERTIARY]",
        f"length: {len(weil_t)}",
        f"hex: {_bits_to_hex_annex3(weil_t, prepend_zeros=0)}",
        "",
    ]

    for idx, sec_hex in enumerate(_SECONDARY_CODES_HEX):
        lines += [
            f"[SECONDARY_S{idx}]",
            f"hex: {sec_hex}",
            "",
        ]

    with open(path, "w") as f:
        f.write("\n".join(lines))

    return path


def export_frame_vector(
    prn_id: int,
    fid: int,
    toi: int,
    sb2_data: NDArray[np.uint8],
    sb3_data: NDArray[np.uint8],
    sb4_data: NDArray[np.uint8],
    output_path: StrPath,
    timestamp: int | None = None,
) -> str:
    """Export an encoded frame as binary with 64-byte header (Level 2).

    Pass an explicit ``timestamp`` (Unix epoch seconds) for byte-stable
    output suitable for shipping as a published test vector; defaults to
    the current wall-clock time otherwise.

    Returns the path to the written file.
    """
    frame = frame_build(fid, toi, sb2_data, sb3_data, sb4_data)

    if timestamp is None:
        timestamp = int(datetime.now(UTC).timestamp())

    # 64-byte header per interoperability.pdf
    header = bytearray(64)
    header[0:8] = b"LSISAFS\x00"
    struct.pack_into("<I", header, 8, 1)  # version
    struct.pack_into("<I", header, 12, FRAME_LENGTH)  # frame length
    struct.pack_into("<I", header, 16, prn_id)
    struct.pack_into("<q", header, 20, timestamp)

    path_str = os.fspath(output_path)
    os.makedirs(os.path.dirname(path_str) or ".", exist_ok=True)
    with open(path_str, "wb") as f:
        f.write(bytes(header))
        f.write(frame.astype(np.uint8).tobytes())

    return path_str


def export_signal_vector(
    prn_id: int,
    iq_data: NDArray[np.float32],
    sample_rate: float,
    output_path: StrPath,
) -> str:
    """Export baseband IQ signal with 128-byte LSISIQ header (Level 3).

    Delegates to :func:`~lunalink.afs.write_iq_file`.
    Returns the path to the written file.
    """
    path_str = os.fspath(output_path)
    os.makedirs(os.path.dirname(path_str) or ".", exist_ok=True)
    write_iq_file(path_str, iq_data, sample_rate, prn_id)
    return path_str


def export_parsed_json(
    result: DecodeResult,
    output_path: StrPath,
    frame_id: str = "test_001",
) -> str:
    """Export parsed navigation data as JSON (Level 5).

    Returns the path to the written file.
    """
    doc: dict = {
        "version": "1.0",
        "timestamp": datetime.now(UTC).isoformat(),
        "implementation": "LunaLink",
        "frame_id": frame_id,
        "subframe1": {
            "fid": result.fid,
            "toi": result.toi,
        },
        "subframe2": {
            "ldpc_converged": result.sb2.ldpc_converged,
            "crc_ok": result.sb2.crc_ok,
            "data_hex": _bits_to_hex(result.sb2.data) if result.sb2.crc_ok else None,
            "data_bits": len(result.sb2.data),
        },
        "subframe3": {
            "ldpc_converged": result.sb3.ldpc_converged,
            "crc_ok": result.sb3.crc_ok,
            "data_hex": _bits_to_hex(result.sb3.data) if result.sb3.crc_ok else None,
            "data_bits": len(result.sb3.data),
        },
        "subframe4": {
            "ldpc_converged": result.sb4.ldpc_converged,
            "crc_ok": result.sb4.crc_ok,
            "data_hex": _bits_to_hex(result.sb4.data) if result.sb4.crc_ok else None,
            "data_bits": len(result.sb4.data),
        },
        "sync": {
            "offset": result.sync_offset,
            "metric": result.sync_metric,
        },
    }

    if result.time is not None:
        doc["time_of_transmission"] = {
            "wn": result.time.wn,
            "itow": result.time.itow,
            "toi": result.time.toi,
            "t_f": result.time.t_f,
            "seconds_of_week": result.time.seconds_of_week,
        }

    path_str = os.fspath(output_path)
    os.makedirs(os.path.dirname(path_str) or ".", exist_ok=True)
    with open(path_str, "w") as f:
        json.dump(doc, f, indent=2)

    return path_str


# ── Level 5 interop-PDF v1.0 spec-shaped emitter ────────────────────────────


def export_parsed_json_interop_v1(
    result: DecodeResult,
    output_path: StrPath,
    frame_id: str,
    *,
    timestamp_iso: str | None = None,
    input_frame_sha256: str | None = None,
    description: str | None = None,
) -> str:
    """Export L5 parsed JSON in the interop-PDF Parsed Data Export shape.

    Top-level keys (``version``, ``timestamp``, ``frame_id``, ``subframe1``,
    ``subframe2``, ``subframe3``, ``subframe4``, ``time_of_transmission``)
    follow the interop-PDF p.3-4 template literally.  Within each
    subframe block, fields that LSIS V1.0 *pins* (FID, TOI per Tables
    13/14; WN, ITOW per Table 22; ToT formula per §2.5.5; CRC-24Q per
    §2.4.3.1.3 + Appendix H) are emitted as concrete values.  Fields
    that V1.0 declares as *To Be Written* (CED §2.5.3, Health §2.5.2,
    Time Conversions §2.5.20) or *To Be Confirmed* (SF3/SF4 type-field
    width: "4 or 6 bits" per Tables 19/20) are emitted as raw bit-slices
    with explicit ``_lsis_ref`` markers, so consumers can byte-equal-
    compare without an implementation having to invent positions V1.0
    doesn't pin.

    Parameters
    ----------
    result : DecodeResult
        From :func:`~lunalink.afs.receive_frame`.  All subframe CRCs
        must have passed (``result.all_ok``); otherwise the JSON will
        still be written but with ``_crc24q_ok: false`` markers.
    output_path : StrPath
        Output file path (typically ``parsed/parsed_<frame_id>.json``
        in the public lsis-afs-test-vectors repo).
    frame_id : str
        Opaque identifier echoed into the JSON (e.g.,
        ``"frame_message_1"``); also used by consumers' ``diff-parsed``.
    timestamp_iso : str, optional
        ISO-8601 timestamp written into the JSON's ``timestamp`` field.
        Defaults to LSIS V1.0 publication date (2025-01-29T00:00:00Z)
        for byte-stable output suitable for shipping as a published
        test vector.
    input_frame_sha256 : str, optional
        Hex SHA-256 of the input ``frame_*.bin``; written under
        ``_input_frame_sha256`` for provenance.
    description : str, optional
        Human-readable description (e.g., ``"All zeros (baseline)"``);
        echoed into ``_description`` for human readers.

    Returns
    -------
    str
        Path to the written JSON file.
    """
    # Local import — parse_sb*/SUBFRAME_TYPE_BITS are only needed by this
    # Level-5 emitter.
    from lunalink.afs.signal import (  # noqa: PLC0415
        SUBFRAME_TYPE_BITS,
        parse_sb2,
        parse_sb3,
        parse_sb4,
    )

    # parse_sb2/3/4 require writable C-contiguous arrays, but
    # DecodeResult.sb*.data are read-only views (set by _readonly()).
    # np.array(..., dtype=...) forces a fresh writable copy.
    sb2_parsed = parse_sb2(np.array(result.sb2.data, dtype=np.uint8))
    sb3_parsed = parse_sb3(np.array(result.sb3.data, dtype=np.uint8))
    sb4_parsed = parse_sb4(np.array(result.sb4.data, dtype=np.uint8))

    # WN/ITOW come directly from parse_sb2 (bit-level unpacking of SB2[0..21]
    # per V1.0 Table 22).  We do NOT use result.time, because receive_frame's
    # high-level API only populates result.time when WN/ITOW/TOI are all
    # within spec range (V1.0 §2.4.3.1.6: ITOW max 503).  Test corpora may
    # intentionally include out-of-spec field values (e.g. all-ones SB2
    # yielding ITOW=511) where reporting raw bits is the load-bearing
    # behaviour at L5.
    wn_raw = int(sb2_parsed.wn)
    itow_raw = int(sb2_parsed.itow)
    toi_raw = int(result.toi)

    # Per LSIS V1.0 §2.5.5: tF = WN*604800 + ITOW*1200 + TOI*12 + delta_t_lrt.
    # delta_t_lrt comes from MSG-G4 (CED), which V1.0 §2.5.3 declares TBW;
    # we therefore set it to 0 and document the assumption in the JSON.
    # Computed via the formula directly so out-of-spec field values still
    # produce a deterministic ToT (consumers can decide what to do with it).
    tot_seconds = float(wn_raw * 604800 + itow_raw * 1200 + toi_raw * 12)

    # SB2 raw data slice (1176 bits = 147 bytes when bit-packed).
    sb2_data_hex = _bits_to_hex(result.sb2.data)
    sb3_data_hex = _bits_to_hex(result.sb3.data)
    sb4_data_hex = _bits_to_hex(result.sb4.data)

    # SB2 spare bits at offsets 1150..1175 (LSIS-FID0-550, 26 bits).
    sb2_spare_bits = result.sb2.data[1150:1176]
    sb2_spare_hex = _bits_to_hex(sb2_spare_bits)

    # SB2 V1.0-TBW region: bits 22..1149 (after WN[0..12] + ITOW[13..21],
    # before spare bits[1150..1175]).  Layout of CED, Health, Time-Conv
    # within this slice is TBW per LSIS-TBW-2005/2006/2012.
    sb2_tbw_bits = result.sb2.data[22:1150]
    sb2_tbw_hex = _bits_to_hex(sb2_tbw_bits)

    # SB3/SB4 data after the type field.  LunaLink uses a 6-bit type
    # width per V1.0 Tables 19/20 (which declare "4 or 6 bits" TBC).
    sb3_data_after_type = result.sb3.data[SUBFRAME_TYPE_BITS:]
    sb4_data_after_type = result.sb4.data[SUBFRAME_TYPE_BITS:]
    sb3_data_after_type_hex = _bits_to_hex(sb3_data_after_type)
    sb4_data_after_type_hex = _bits_to_hex(sb4_data_after_type)

    doc: dict = {
        "version": "1.0",
        "timestamp": timestamp_iso or _LSIS_V1_TIMESTAMP_ISO,
        "frame_id": frame_id,
        "_implementation": "LunaLink",
        "_lsis_version": _LSIS_VERSION_LABEL,
        "subframe1": {
            "fid": result.fid,
            "toi": result.toi,
            "_lsis_ref": "Tables 13, 14 (§2.4.2.2)",
        },
        "subframe2": {
            "wn": wn_raw,
            "itow": itow_raw,
            "ced": {},
            "_ced_status": "tbw",
            "_ced_lsis_ref": "LSIS-TBW-2006 (§2.5.3)",
            "_ced_note": (
                "CED bit-level layout TBW per LSIS V1.0; empty object "
                "emitted to match interop-PDF Parsed Data Export Format "
                "example shape (interop-PDF page 4 shows concrete fields "
                "as aspirational; V1.0 has not yet pinned the layout). "
                "See _tbw_region_raw_hex for byte-equal cross-impl "
                "comparison of the full SB2 TBW region (bits 22..1149)."
            ),
            "health": 0,
            "_health_status": "tbw",
            "_health_lsis_ref": "LSIS-TBW-2005 (§2.5.2)",
            "_health_note": (
                "Health bit-level layout TBW per LSIS V1.0; "
                "scalar 0 emitted to match interop-PDF example shape."
            ),
            "time_conversions": {},
            "_time_conversions_status": "tbw",
            "_time_conversions_lsis_ref": "LSIS-TBW-2012 (§2.5.20)",
            "_time_conversions_note": (
                "Time conversion fields TBW per LSIS V1.0; empty object "
                "emitted to match interop-PDF example shape."
            ),
            "_spare_bits_hex": sb2_spare_hex,
            "_spare_bits_lsis_ref": "LSIS-FID0-550 (LSIS-TBC-2022)",
            "_tbw_region_raw_hex": sb2_tbw_hex,
            "_tbw_region_bit_range": [22, 1149],
            "_data_raw_hex": sb2_data_hex,
            "_data_lsis_ref": "Table 18 (1176 data bits + 24-bit CRC)",
            "_crc24q_ok": bool(result.sb2.crc_ok),
            "_crc24q_lsis_ref": "§2.4.3.1.3 + Appendix H",
        },
        "subframe3": {
            "type": int(sb3_parsed.type_raw),
            "_type_status": "lunalink-resolved",
            "_type_lsis_ref": (
                "LSIS-TBC-2023 (§2.4.3.1.7 Table 19) — V1.0 declares "
                f"'4 or 6 bits'; LunaLink uses {SUBFRAME_TYPE_BITS}"
            ),
            "_type_width_bits": SUBFRAME_TYPE_BITS,
            "data": {},
            "_data_status": "tbw",
            "_data_after_type_lsis_ref": (
                "Table 19 — per-type content layouts TBW per LSIS V1.0 §2.5"
            ),
            "_data_after_type_bit_range": [SUBFRAME_TYPE_BITS, 845],
            "_data_after_type_raw_hex": sb3_data_after_type_hex,
            "_data_raw_hex": sb3_data_hex,
            "_data_lsis_ref": "Table 19 (846 data bits + 24-bit CRC)",
            "_crc24q_ok": bool(result.sb3.crc_ok),
            "_crc24q_lsis_ref": "§2.4.3.1.3 + Appendix H",
        },
        "subframe4": {
            "type": int(sb4_parsed.type_raw),
            "_type_status": "lunalink-resolved",
            "_type_lsis_ref": (
                "LSIS-TBC-2024 (§2.4.3.1.8 Table 20) — V1.0 declares "
                f"'4 or 6 bits'; LunaLink uses {SUBFRAME_TYPE_BITS}"
            ),
            "_type_width_bits": SUBFRAME_TYPE_BITS,
            "data": {},
            "_data_status": "tbw",
            "_data_after_type_lsis_ref": (
                "Table 20 — per-type content layouts TBW per LSIS V1.0 §2.5"
            ),
            "_data_after_type_bit_range": [SUBFRAME_TYPE_BITS, 845],
            "_data_after_type_raw_hex": sb4_data_after_type_hex,
            "_data_raw_hex": sb4_data_hex,
            "_data_lsis_ref": "Table 20 (846 data bits + 24-bit CRC)",
            "_crc24q_ok": bool(result.sb4.crc_ok),
            "_crc24q_lsis_ref": "§2.4.3.1.3 + Appendix H",
        },
        "time_of_transmission": tot_seconds,
        "_time_of_transmission_lsis_ref": "§2.5.5 + Table 22",
        "_time_of_transmission_formula": (
            "tF = WN*604800 + ITOW*1200 + TOI*12 + dt_lrt"
        ),
        "_time_of_transmission_dt_lrt_status": "tbd",
        "_time_of_transmission_dt_lrt_lsis_ref": (
            "LSIS-TBD-2003 (LRT epoch); dt_lrt comes from MSG-G4 (TBW); "
            "value 0 used as placeholder."
        ),
    }

    if input_frame_sha256:
        doc["_input_frame_sha256"] = input_frame_sha256
    if description:
        doc["_description"] = description

    path_str = os.fspath(output_path)
    os.makedirs(os.path.dirname(path_str) or ".", exist_ok=True)
    with open(path_str, "w") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")  # trailing newline for POSIX-friendly diffs

    return path_str


# ── Standard test scenarios ─────────────────────────────────────────────────

_SCENARIOS: list[tuple[str, str]] = [
    ("msg1_zeros", "zeros"),
    ("msg2_ones", "ones"),
    ("msg3_alternating", "alternating"),
    ("msg4_random", "random"),
]


def generate_standard_vectors(
    output_dir: StrPath,
    prn_id: int = 1,
    fid: int = 0,
    toi: int = 42,
) -> dict[str, list[str]]:
    """Generate all standard interop test scenarios.

    Produces Level 1 (codes), Level 2 (frames), Level 3 (signals),
    and Level 5 (parsed JSON) vectors for PRN *prn_id*.

    Parameters
    ----------
    output_dir : str
        Root directory for generated vectors.
    prn_id : int
        PRN ID to use (default: 1).
    fid : int
        Frame ID (default: 0).
    toi : int
        Time of Interval (default: 42).

    Returns
    -------
    dict[str, list[str]]
        Mapping of vector type to list of generated file paths.
    """
    paths: dict[str, list[str]] = {
        "codes": [],
        "frames": [],
        "signals": [],
        "parsed": [],
    }

    # Level 1: Code vectors
    codes_dir = os.path.join(output_dir, "codes")
    paths["codes"].append(export_code_vectors(prn_id, codes_dir))

    for scenario_name, pattern in _SCENARIOS:
        sb2, sb3, sb4 = make_test_payload(pattern)

        # Level 2: Frame vectors
        frame_path = os.path.join(output_dir, "frames", f"{scenario_name}.bin")
        paths["frames"].append(
            export_frame_vector(prn_id, fid, toi, sb2, sb3, sb4, frame_path)
        )

        # Level 3: Signal vectors (native rate, one frame)
        frame_obj, iq = transmit_frame(
            prn_id=prn_id,
            fid=fid,
            toi=toi,
            sb2_data=sb2,
            sb3_data=sb3,
            sb4_data=sb4,
        )
        signal_path = os.path.join(output_dir, "signals", f"{scenario_name}.iq")
        paths["signals"].append(
            export_signal_vector(prn_id, iq, AFS_Q_CHIP_RATE, signal_path)
        )

        # Level 5: Parsed JSON (decode our own frame for reference)
        bipolar = np.where(frame_obj.symbols == 0, 1.0, -1.0).astype(np.float32)
        result = receive_frame(bipolar)
        parsed_path = os.path.join(output_dir, "parsed", f"{scenario_name}.json")
        paths["parsed"].append(
            export_parsed_json(result, parsed_path, frame_id=scenario_name)
        )

    return paths
