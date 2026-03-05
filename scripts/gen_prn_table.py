#!/usr/bin/env python3
"""Generate C++ PRN table from Gold-2046 hex reference file.

Reads docs/references/006_GoldCode2046hex210prns.txt, unpacks 210 hex
strings into uint8 chip arrays (2046 chips each, MSB-first), and writes:
  - cpp/include/lunalink/signal/prn_table.hpp
  - cpp/signal/prn_table.cpp

Run once; commit both generated files.
"""

from __future__ import annotations

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INPUT = REPO_ROOT / "docs" / "references" / "006_GoldCode2046hex210prns.txt"
HPP_OUT = REPO_ROOT / "cpp" / "include" / "lunalink" / "signal" / "prn_table.hpp"
CPP_OUT = REPO_ROOT / "cpp" / "signal" / "prn_table.cpp"

PRN_COUNT = 210
CHIP_LENGTH = 2046


def parse_hex_strings(path: Path) -> list[str]:
    """Extract quoted hex strings from the reference file."""
    text = path.read_text()
    strings = re.findall(r'"([0-9A-Fa-f]+)"', text)
    if len(strings) != PRN_COUNT:
        msg = f"Expected {PRN_COUNT} hex strings, found {len(strings)}"
        raise ValueError(msg)
    return strings


def hex_to_chips(hex_str: str) -> list[int]:
    """Unpack a hex string into 2046 chip values (MSB-first)."""
    raw_bytes = bytes.fromhex(hex_str)
    if len(raw_bytes) != 256:
        msg = f"Expected 256 bytes, got {len(raw_bytes)}"
        raise ValueError(msg)
    bits: list[int] = []
    for byte_val in raw_bytes:
        for bit_pos in range(7, -1, -1):
            bits.append((byte_val >> bit_pos) & 1)
    return bits[:CHIP_LENGTH]


def write_hpp(path: Path) -> None:
    """Write the header file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "// GENERATED — do not edit. Re-run scripts/gen_prn_table.py to regenerate.\n"
        "#pragma once\n"
        "#include <cstdint>\n"
        "\n"
        "namespace lunalink::signal {\n"
        f"extern const uint8_t kGoldPrns[{PRN_COUNT}][{CHIP_LENGTH}];\n"
        "} // namespace lunalink::signal\n"
    )


def write_cpp(path: Path, all_chips: list[list[int]]) -> None:
    """Write the source file with full initialiser list."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = []
    lines.append(
        "// GENERATED — do not edit. Re-run scripts/gen_prn_table.py to regenerate."
    )
    lines.append('#include "lunalink/signal/prn_table.hpp"')
    lines.append("")
    lines.append("namespace lunalink::signal {")
    lines.append("")
    lines.append(f"const uint8_t kGoldPrns[{PRN_COUNT}][{CHIP_LENGTH}] = {{")
    for prn_idx, chips in enumerate(all_chips):
        # Format as rows of 78 chips for readability
        row_parts: list[str] = []
        for i in range(0, CHIP_LENGTH, 78):
            chunk = chips[i : i + 78]
            row_parts.append("".join(str(c) for c in chunk))
        chip_str = ",".join(str(c) for c in chips)
        lines.append(f"    // PRN {prn_idx + 1}")
        lines.append(f"    {{{chip_str}}},")
    lines.append("};")
    lines.append("")
    lines.append("} // namespace lunalink::signal")
    lines.append("")
    path.write_text("\n".join(lines))


def main() -> None:
    """Generate PRN table files."""
    hex_strings = parse_hex_strings(INPUT)
    all_chips = [hex_to_chips(h) for h in hex_strings]

    # Verify all chips are binary
    for prn_idx, chips in enumerate(all_chips):
        if len(chips) != CHIP_LENGTH:
            msg = f"PRN {prn_idx + 1}: expected {CHIP_LENGTH} chips, got {len(chips)}"
            raise ValueError(msg)
        if not all(c in (0, 1) for c in chips):
            msg = f"PRN {prn_idx + 1}: non-binary chip value"
            raise ValueError(msg)

    write_hpp(HPP_OUT)
    write_cpp(CPP_OUT, all_chips)
    print(f"Generated {HPP_OUT.relative_to(REPO_ROOT)}")
    print(f"Generated {CPP_OUT.relative_to(REPO_ROOT)}")
    print(f"  {PRN_COUNT} PRNs × {CHIP_LENGTH} chips")


if __name__ == "__main__":
    main()
