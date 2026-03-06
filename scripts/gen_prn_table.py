#!/usr/bin/env python3
"""Generate C++ PRN tables from hex reference files.

Reads three PRN code families from docs/references/:
  - 006_GoldCode2046hex210prns.txt   (AFS-I primary, 2046 chips)
  - 007_l1cp_hex210prns.txt          (AFS-Q primary, 10230 chips)
  - 008_Weil1500hex210prns.txt       (AFS-Q tertiary, 1500 chips)

Chips are stored in packed binary format (1 bit per chip, MSB-first) to
minimize binary size. Accessor functions unpack individual chips on demand.

Generates C++ header/source pairs under cpp/include/lunalink/signal/ and
cpp/signal/. Run once; commit the generated files.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPT_NAME = "scripts/gen_prn_table.py"

PRN_COUNT = 210

CODE_FAMILIES = [
    {
        "input": REPO_ROOT / "docs" / "references" / "006_GoldCode2046hex210prns.txt",
        "chip_length": 2046,
        "padding_bits": 2,
        "array_name": "kGoldPrnsPacked",
        "hpp": REPO_ROOT / "cpp" / "include" / "lunalink" / "signal" / "prn_table.hpp",
        "cpp": REPO_ROOT / "cpp" / "signal" / "prn_table.cpp",
        "format": "quoted",
    },
    {
        "input": REPO_ROOT / "docs" / "references" / "007_l1cp_hex210prns.txt",
        "chip_length": 10230,
        "padding_bits": 2,
        "array_name": "kWeil10230PrnsPacked",
        "hpp": REPO_ROOT
        / "cpp"
        / "include"
        / "lunalink"
        / "signal"
        / "prn_table_weil10230.hpp",
        "cpp": REPO_ROOT / "cpp" / "signal" / "prn_table_weil10230.cpp",
        "format": "raw",
    },
    {
        "input": REPO_ROOT / "docs" / "references" / "008_Weil1500hex210prns.txt",
        "chip_length": 1500,
        "padding_bits": 0,
        "array_name": "kWeil1500PrnsPacked",
        "hpp": REPO_ROOT
        / "cpp"
        / "include"
        / "lunalink"
        / "signal"
        / "prn_table_weil1500.hpp",
        "cpp": REPO_ROOT / "cpp" / "signal" / "prn_table_weil1500.cpp",
        "format": "quoted",
    },
]


def parse_hex_quoted(path: Path) -> list[str]:
    """Extract quoted hex strings from a Python-style list file."""
    text = path.read_text()
    strings = re.findall(r'"([0-9A-Fa-f]+)"', text)
    if len(strings) != PRN_COUNT:
        msg = f"{path.name}: expected {PRN_COUNT} hex strings, found {len(strings)}"
        raise ValueError(msg)
    return strings


def parse_hex_raw(path: Path) -> list[str]:
    """Extract raw hex strings, one per line."""
    lines = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    if len(lines) != PRN_COUNT:
        msg = f"{path.name}: expected {PRN_COUNT} lines, found {len(lines)}"
        raise ValueError(msg)
    for i, line in enumerate(lines):
        if not re.fullmatch(r"[0-9A-Fa-f]+", line):
            msg = f"{path.name}: line {i + 1} is not a valid hex string"
            raise ValueError(msg)
    return lines


def hex_to_packed_bytes(
    hex_str: str,
    chip_length: int,
    padding_bits: int,
) -> list[int]:
    """Convert hex string to packed bytes (MSB-first), stripping padding.

    Returns ceil(chip_length / 8) bytes containing the chip bits packed
    MSB-first. Any trailing bits in the last byte are zero-padded.
    """
    total_bits = len(hex_str) * 4
    expected_bits = chip_length + padding_bits
    if total_bits != expected_bits:
        msg = (
            f"Expected {expected_bits} bits ({chip_length} + {padding_bits} "
            f"padding), got {total_bits}"
        )
        raise ValueError(msg)

    # Unpack all bits
    bits: list[int] = []
    for hex_char in hex_str:
        nibble = int(hex_char, 16)
        for bit_pos in range(3, -1, -1):
            bits.append((nibble >> bit_pos) & 1)

    # Strip leading padding bits
    chip_bits = bits[padding_bits : padding_bits + chip_length]

    # Re-pack into bytes, MSB-first
    packed_len = math.ceil(chip_length / 8)
    packed: list[int] = []
    for byte_idx in range(packed_len):
        byte_val = 0
        for bit_pos in range(8):
            flat_idx = byte_idx * 8 + bit_pos
            if flat_idx < chip_length:
                byte_val |= chip_bits[flat_idx] << (7 - bit_pos)
        packed.append(byte_val)

    return packed


def write_hpp(
    path: Path,
    chip_length: int,
    packed_len: int,
    array_name: str,
) -> None:
    """Write the header file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        f"// GENERATED — do not edit. Re-run {SCRIPT_NAME} to regenerate.\n"
        "#pragma once\n"
        "#include <cstdint>\n"
        "\n"
        "namespace lunalink::signal {\n"
        f"// {chip_length} chips packed MSB-first into {packed_len} bytes per PRN.\n"
        "// NOLINTNEXTLINE(hicpp-avoid-c-arrays)\n"
        f"extern const uint8_t {array_name}[{PRN_COUNT}][{packed_len}];\n"
        "} // namespace lunalink::signal\n"
    )


def write_cpp(
    path: Path,
    packed_len: int,
    array_name: str,
    hpp_include: str,
    all_packed: list[list[int]],
) -> None:
    """Write the source file with packed byte initialiser lists."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = [
        f"// GENERATED — do not edit. Re-run {SCRIPT_NAME} to regenerate.",
        f'#include "{hpp_include}"',
        "",
        "namespace lunalink::signal {",
        "",
        "// NOLINTNEXTLINE(hicpp-avoid-c-arrays)",
        f"const uint8_t {array_name}[{PRN_COUNT}][{packed_len}] = {{",
    ]
    for packed in all_packed:
        hex_vals = ",".join(f"0x{b:02X}" for b in packed)
        lines.append("    {" + hex_vals + "},")
    lines.append("};")
    lines.append("")
    lines.append("} // namespace lunalink::signal")
    lines.append("")
    path.write_text("\n".join(lines))


def generate_family(family: dict) -> None:
    """Generate C++ files for one PRN code family."""
    input_path = family["input"]
    chip_length = family["chip_length"]
    padding_bits = family["padding_bits"]
    array_name = family["array_name"]
    hpp_path = family["hpp"]
    cpp_path = family["cpp"]
    fmt = family["format"]

    if fmt == "quoted":
        hex_strings = parse_hex_quoted(input_path)
    else:
        hex_strings = parse_hex_raw(input_path)

    all_packed = [
        hex_to_packed_bytes(h, chip_length, padding_bits) for h in hex_strings
    ]
    packed_len = math.ceil(chip_length / 8)

    # Verify lengths
    for prn_idx, packed in enumerate(all_packed):
        if len(packed) != packed_len:
            msg = f"PRN {prn_idx + 1}: expected {packed_len} bytes, got {len(packed)}"
            raise ValueError(msg)

    hpp_include = f"lunalink/signal/{hpp_path.name}"

    write_hpp(hpp_path, chip_length, packed_len, array_name)
    write_cpp(cpp_path, packed_len, array_name, hpp_include, all_packed)

    size_kb = PRN_COUNT * packed_len / 1024
    print(f"Generated {hpp_path.relative_to(REPO_ROOT)}")
    print(f"Generated {cpp_path.relative_to(REPO_ROOT)}")
    print(
        f"  {PRN_COUNT} PRNs x {chip_length} chips"
        f" = {packed_len} packed bytes/PRN ({size_kb:.1f} KB)"
    )


def main() -> None:
    """Generate PRN table files for all code families."""
    for family in CODE_FAMILIES:
        generate_family(family)


if __name__ == "__main__":
    main()
