"""Interoperability test vector generation in competition format.

Produces full-length, bit-exact vectors for cross-team validation
at all 5 interop levels defined in the competition spec.
"""

from __future__ import annotations

import json
import os
import struct
from datetime import datetime, timezone

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
    weil10230_code,
    weil1500_code,
    write_iq_file,
)


def _bits_to_hex(bits: NDArray[np.uint8]) -> str:
    """Pack a bit array to a hex string."""
    pad = (8 - len(bits) % 8) % 8
    if pad:
        bits = np.concatenate([bits, np.zeros(pad, dtype=np.uint8)])
    return np.packbits(bits).tobytes().hex()


def export_code_vectors(prn_id: int, output_dir: str) -> str:
    """Export spreading codes for one PRN in hex format (Level 1).

    Returns the path to the written file.
    """
    os.makedirs(output_dir, exist_ok=True)
    path = os.path.join(output_dir, f"codes_prn{prn_id:03d}.hex")

    gold = prn_code(prn_id)
    weil_p = weil10230_code(prn_id)
    weil_t = weil1500_code(prn_id)

    ts = datetime.now(timezone.utc).isoformat()

    lines = [
        "# LSIS-AFS Code Test Vector",
        f"# PRN: {prn_id}",
        f"# Generated: {ts}",
        "# Implementation: LunaLink",
        "",
        "[GOLD_CODE]",
        f"length: {len(gold)}",
        f"hex: {_bits_to_hex(gold)}",
        "",
        "[WEIL_PRIMARY]",
        f"length: {len(weil_p)}",
        f"hex: {_bits_to_hex(weil_p)}",
        "",
        "[WEIL_TERTIARY]",
        f"length: {len(weil_t)}",
        f"hex: {_bits_to_hex(weil_t)}",
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
    output_path: str,
) -> str:
    """Export an encoded frame as binary with 64-byte header (Level 2).

    Returns the path to the written file.
    """
    frame = frame_build(fid, toi, sb2_data, sb3_data, sb4_data)

    # 64-byte header per interoperability.pdf
    header = bytearray(64)
    header[0:8] = b"LSISAFS\x00"
    struct.pack_into("<I", header, 8, 1)  # version
    struct.pack_into("<I", header, 12, FRAME_LENGTH)  # frame length
    struct.pack_into("<I", header, 16, prn_id)
    struct.pack_into("<q", header, 20, int(datetime.now(timezone.utc).timestamp()))

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(bytes(header))
        f.write(frame.astype(np.uint8).tobytes())

    return output_path


def export_signal_vector(
    prn_id: int,
    iq_data: NDArray[np.float32],
    sample_rate: float,
    output_path: str,
) -> str:
    """Export baseband IQ signal with 128-byte LSISIQ header (Level 3).

    Delegates to :func:`~lunalink.afs.write_iq_file`.
    Returns the path to the written file.
    """
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    write_iq_file(output_path, iq_data, sample_rate, prn_id)
    return output_path


def export_parsed_json(
    result: DecodeResult,
    output_path: str,
    frame_id: str = "test_001",
) -> str:
    """Export parsed navigation data as JSON (Level 5).

    Returns the path to the written file.
    """
    doc: dict = {
        "version": "1.0",
        "timestamp": datetime.now(timezone.utc).isoformat(),
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

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(doc, f, indent=2)

    return output_path


# ── Standard test scenarios ─────────────────────────────────────────────────

_SCENARIOS: list[tuple[str, str]] = [
    ("msg1_zeros", "zeros"),
    ("msg2_ones", "ones"),
    ("msg3_alternating", "alternating"),
    ("msg4_random", "random"),
]


def generate_standard_vectors(
    output_dir: str,
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
            prn_id=prn_id, fid=fid, toi=toi,
            sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
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
