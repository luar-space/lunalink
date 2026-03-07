#!/usr/bin/env python3
"""Generate C++ PRN tables from hex reference files.

Applies Class A Hardening: alignas(64) and constinit.
Packing: MSB-first bit packing (8 chips per byte).
"""

import math
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
REF_DIR = REPO_ROOT / "docs" / "references"
OUT_DIR = REPO_ROOT / "cpp" / "signal"
HPP_DIR = REPO_ROOT / "cpp" / "include" / "lunalink" / "signal"

PRN_COUNT = 210

CODE_FAMILIES = [
    {
        "id": "gold",
        "file": "006_GoldCode2046hex210prns.txt",
        "chip_length": 2046,
        "padding_bits": 2,
        "var": "kGoldPrnsPacked",
        "header": "prn_table.hpp",
        "cpp": "prn_table.cpp",
    },
    {
        "id": "weil10230",
        "file": "007_l1cp_hex210prns.txt",
        "chip_length": 10230,
        "padding_bits": 2,
        "var": "kWeil10230PrnsPacked",
        "header": "prn_table_weil10230.hpp",
        "cpp": "prn_table_weil10230.cpp",
    },
    {
        "id": "weil1500",
        "file": "008_Weil1500hex210prns.txt",
        "chip_length": 1500,
        "padding_bits": 0,
        "var": "kWeil1500PrnsPacked",
        "header": "prn_table_weil1500.hpp",
        "cpp": "prn_table_weil1500.cpp",
    },
]


def hex_to_packed_bytes(hex_str, chip_length, padding_bits):
    """Convert hex string to packed bytes (MSB-first), stripping padding."""
    # Each hex char is 4 bits
    bits = []
    for char in hex_str:
        val = int(char, 16)
        for i in range(3, -1, -1):
            bits.append((val >> i) & 1)

    # Strip padding and take required chips
    actual_chips = bits[padding_bits : padding_bits + chip_length]

    # Pack into bytes MSB-first
    packed = []
    for i in range(0, len(actual_chips), 8):
        byte_bits = actual_chips[i : i + 8]
        byte_val = 0
        for bit_idx, bit in enumerate(byte_bits):
            if bit:
                byte_val |= 1 << (7 - bit_idx)
        packed.append(byte_val)
    return packed


def generate_family(family):
    """Process a PRN family and generate hardened files."""
    hex_path = REF_DIR / family["file"]
    cpp_path = OUT_DIR / family["cpp"]
    hpp_path = HPP_DIR / family["header"]

    if not hex_path.exists():
        print(f"Error: {hex_path} not found.")
        return

    content = hex_path.read_text()
    # Find all quoted hex strings or blocks of hex
    hex_strings = re.findall(r'"([0-9A-Fa-f]+)"', content)
    if not hex_strings:
        hex_strings = [
            line.strip()
            for line in content.splitlines()
            if re.fullmatch(r"[0-9A-Fa-f]+", line.strip())
        ]

    if len(hex_strings) != PRN_COUNT:
        print(
            f"Warning: Expected {PRN_COUNT} PRNs in {family['file']}, "
            f"found {len(hex_strings)}"
        )

    packed_len = math.ceil(family["chip_length"] / 8)
    all_packed = [
        hex_to_packed_bytes(h, family["chip_length"], family["padding_bits"])
        for h in hex_strings[:PRN_COUNT]
    ]

    # Generate HPP
    with hpp_path.open("w") as f:
        f.write(
            "// GENERATED - do not edit.\n#pragma once\n"
            "#include <cstdint>\n#include <array>\n\n"
        )
        f.write("namespace lunalink::signal {\n\n")
        f.write(
            f"// {family['chip_length']} chips packed MSB-first into "
            f"{packed_len} bytes per PRN.\n"
        )
        f.write(
            f"extern const std::array<std::array<uint8_t, {packed_len}>, "
            f"{PRN_COUNT}> {family['var']};\n\n"
        )
        f.write("} // namespace lunalink::signal\n")

    # Generate CPP
    with cpp_path.open("w") as f:
        f.write(f'#include "lunalink/signal/{family["header"]}"\n')
        f.write('#include "lunalink/signal/safety.hpp"\n')
        f.write("#include <cstdint>\n")
        f.write("#include <array>\n\n")
        f.write("namespace lunalink::signal {\n\n")
        f.write(
            "// Flight-Hardened LUT: 64-byte alignment, section pinning, "
            "and constinit.\n"
        )
        f.write(
            "alignas(64) LUNALINK_LUT_SECTION constinit const "
            "std::array<std::array<uint8_t, "
            + str(packed_len)
            + ">, "
            + str(PRN_COUNT)
            + "> "
            + family["var"]
            + " = {{\n"
        )

        for i, packed in enumerate(all_packed):
            f.write("    {{")  # Inner std::array start
            f.write(", ".join(f"0x{b:02X}U" for b in packed))
            f.write("}}")  # Inner std::array end
            if i < PRN_COUNT - 1:
                f.write(",")
            f.write(f" // PRN {i + 1}\n")

        f.write("}};\n\n")
        f.write("} // namespace lunalink::signal\n")

    size_kb = len(all_packed) * packed_len / 1024
    print(f"Generated {family['header']} and {family['cpp']} ({size_kb:.1f} KB)")


def main():
    """Generate all hardened PRN table files."""
    for family in CODE_FAMILIES:
        generate_family(family)


if __name__ == "__main__":
    main()
