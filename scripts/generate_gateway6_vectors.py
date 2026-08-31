#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate Gateway 6 interoperability test vectors (JSON).

Produces a set of message parsing and ToT test vectors that other teams
can consume to validate their implementations against LunaLink's.

Output: build/test_vectors/gateway6_vectors.json

Usage:
    python scripts/generate_gateway6_vectors.py
    # or via Taskfile:
    task generate-g6-vectors
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

# Add src to path for direct invocation
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))


def generate_tot_vectors() -> list[dict]:
    """Generate Time of Transmission test vectors per LSIS-720."""
    vectors = []

    # Constants from the spec
    sec_week = 604800
    bi_d = 1200
    f_d = 12

    cases = [
        # (description, wn, itow, toi, delta_t_lrt)
        ("all_zeros", 0, 0, 0, 0.0),
        ("basic", 1, 2, 5, 0.0),
        ("with_delta", 0, 0, 0, 1.5),
        ("negative_delta", 1, 0, 0, -0.5),
        ("boundary_max", 8191, 503, 99, 0.0),
        ("mid_week", 52, 252, 50, 0.0),
        ("last_frame_of_week", 0, 503, 99, 0.0),
        ("first_frame_of_next_week", 1, 0, 0, 0.0),
        ("week_rollover_pair_a", 8191, 503, 99, 0.0),
        ("week_rollover_pair_b", 0, 0, 0, 0.0),  # WN wraps mod 8192
    ]

    for desc, wn, itow, toi, delta in cases:
        t_f = wn * sec_week + itow * bi_d + toi * f_d + delta
        vectors.append(
            {
                "description": desc,
                "inputs": {
                    "wn": wn,
                    "itow": itow,
                    "toi": toi,
                    "delta_t_lrt": delta,
                },
                "expected": {
                    "status": "OK",
                    "t_f": t_f,
                },
                "spec_reference": "LSIS-720",
            }
        )

    # Invalid input cases
    invalid_cases = [
        ("wn_out_of_range", 8192, 0, 0, 0.0, "INVALID_WN"),
        ("itow_out_of_range", 0, 504, 0, 0.0, "INVALID_ITOW"),
        ("toi_out_of_range", 0, 0, 100, 0.0, "INVALID_TOI"),
    ]
    for desc, wn, itow, toi, delta, status in invalid_cases:
        vectors.append(
            {
                "description": desc,
                "inputs": {"wn": wn, "itow": itow, "toi": toi, "delta_t_lrt": delta},
                "expected": {"status": status, "t_f": 0.0},
                "spec_reference": "LSIS-720",
            }
        )

    return vectors


def generate_sb2_parse_vectors() -> list[dict]:
    """Generate SB2 parsing test vectors (WN + ITOW extraction)."""
    vectors = []

    wn_bits = 13
    itow_bits = 9
    sb2_data_bits = 1176

    cases = [
        ("zeros", 0, 0),
        ("basic", 1234, 456),
        ("boundary_max", 8191, 503),
        ("wn_only", 4096, 0),
        ("itow_only", 0, 256),
        ("mid_range", 2048, 252),
    ]

    for desc, wn, itow in cases:
        # Pack bits MSB-first
        bits = [0] * sb2_data_bits
        for i in range(wn_bits):
            bits[i] = (wn >> (wn_bits - 1 - i)) & 1
        for i in range(itow_bits):
            bits[wn_bits + i] = (itow >> (itow_bits - 1 - i)) & 1

        vectors.append(
            {
                "description": desc,
                "inputs": {"sb2_data_first_22_bits": bits[:22]},
                "expected": {
                    "status": "OK",
                    "wn": wn,
                    "itow": itow,
                    "data_offset": 22,
                },
                "spec_reference": "LSIS-720 Table 22",
                "note": (
                    "WN (13 bits) + ITOW (9 bits) at offset 0, "
                    "MSB-first per LSIS-320. "
                    "Bit positions within SB2 assumed per SF-005."
                ),
            }
        )

    return vectors


def generate_type_field_vectors() -> list[dict]:
    """Generate SB3/SB4 type field routing test vectors."""
    # All known MSG IDs from Table 21
    known_types = {
        1: "G1",
        2: "G2",
        3: "G3",
        4: "G4",
        5: "G5",
        8: "G8",
        10: "G10",
        11: "G11",
        14: "G14",
        15: "G15",
        17: "G17",
        19: "S19",
        20: "S20",
        22: "G22",
        23: "G23",
        24: "G24",
        25: "G25",
        27: "G27",
        28: "G28",
        29: "G29",
        30: "G30",
        31: "G31",
        32: "G32",
    }

    type_bits = 6
    vectors = []

    for raw_val, msg_name in known_types.items():
        bits = [(raw_val >> (type_bits - 1 - i)) & 1 for i in range(type_bits)]
        vectors.append(
            {
                "description": f"MSG-{msg_name}",
                "inputs": {"type_field_bits": bits, "raw_value": raw_val},
                "expected": {"status": "OK", "message_type": msg_name},
                "spec_reference": "LSIS V1.0 Table 21",
            }
        )

    # Unknown type
    for raw_val in [0, 6, 7, 9, 63]:
        bits = [(raw_val >> (type_bits - 1 - i)) & 1 for i in range(type_bits)]
        vectors.append(
            {
                "description": f"unknown_type_{raw_val}",
                "inputs": {"type_field_bits": bits, "raw_value": raw_val},
                "expected": {"status": "OK", "message_type": "UNKNOWN"},
                "spec_reference": "LSIS V1.0 Table 21",
            }
        )

    return vectors


def generate_spare_bits_vectors() -> list[dict]:
    """Generate LSIS-300 spare bit pattern test vectors."""
    return [
        {
            "description": "correct_8bit_pattern",
            "inputs": {"bits": [0, 1, 0, 1, 0, 1, 0, 1]},
            "expected": {"valid": True},
            "spec_reference": "LSIS-300",
        },
        {
            "description": "incorrect_starts_with_1",
            "inputs": {"bits": [1, 0, 1, 0, 1, 0, 1, 0]},
            "expected": {"valid": False},
            "spec_reference": "LSIS-300",
        },
        {
            "description": "single_zero",
            "inputs": {"bits": [0]},
            "expected": {"valid": True},
            "spec_reference": "LSIS-300",
        },
    ]


def main() -> None:
    """Generate and write Gateway 6 test vectors to JSON."""
    out_dir = Path("build/test_vectors")
    out_dir.mkdir(parents=True, exist_ok=True)

    test_vectors = {
        "format_version": 1,
        "generator": "LunaLink Gateway 6 test vector generator",
        "spec_version": "LSIS V1.0 (29 January 2025)",
        "interop_notes": {
            "type_field_width": (
                "LunaLink uses 6 bits for SB3/SB4 type fields to accommodate all 22 "
                "Table 21 message types. The spec marks this as TBC (4 or 6 bits, "
                "{LSIS-TBC-2023}/{LSIS-TBC-2024}). If your implementation uses 4 bits, "
                "type values > 15 will not be representable and interoperability will "
                "break at the type field boundary."
            ),
            "sb2_field_layout": (
                "WN (13 bits) and ITOW (9 bits) are placed at bit offset 0 in SB2, "
                "MSB-first per LSIS-320. The exact bit positions within SB2 are not "
                "specified in LSIS V1.0 (spec finding SF-005). This is an interop "
                "decision point that must be agreed between implementations."
            ),
        },
        "sections": {
            "time_of_transmission": generate_tot_vectors(),
            "sb2_parse": generate_sb2_parse_vectors(),
            "type_field_routing": generate_type_field_vectors(),
            "spare_bits": generate_spare_bits_vectors(),
        },
    }

    out_path = out_dir / "gateway6_vectors.json"
    with open(out_path, "w") as f:
        json.dump(test_vectors, f, indent=2)

    # Summary
    total = sum(len(v) for v in test_vectors["sections"].values())
    print(f"Generated {total} test vectors → {out_path}")
    for name, vecs in test_vectors["sections"].items():
        print(f"  {name}: {len(vecs)} vectors")


if __name__ == "__main__":
    main()
