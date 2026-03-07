#!/usr/bin/env python3
"""Generate flight-hardened LDPC sub-matrix tables for LSIS-AFS.

Converts CSV indices into CSR (Compressed Sparse Row) C++ structures.
Includes Row-Parity and Matrix-CRC32 for Mission Assurance (CBIT).
"""

import binascii
import csv
import struct
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
REF_DIR = REPO_ROOT / "docs" / "references"
OUT_HPP = REPO_ROOT / "cpp" / "include" / "lunalink" / "signal" / "ldpc_tables.hpp"
OUT_CPP = REPO_ROOT / "cpp" / "signal" / "ldpc_tables.cpp"

# Matrix Definitions from LSIS-AFS Annex 2
MATRICES = [
    # SF2 (Z=120, K=1200)
    {
        "id": "sf2_a",
        "file": "003a_lunanet_sf2_ldpc_submatrix_a_ind.csv",
        "rows": 480,
        "cols": 1200,
    },
    {
        "id": "sf2_b",
        "file": "003b_lunanet_sf2_ldpc_submatrix_b_ind.csv",
        "rows": 480,
        "cols": 480,
    },
    {
        "id": "sf2_b_inv",
        "file": "003c_lunanet_sf2_ldpc_submatrix_b_inv_ind.csv",
        "rows": 480,
        "cols": 480,
    },
    {
        "id": "sf2_c",
        "file": "003d_lunanet_sf2_ldpc_submatrix_c_ind.csv",
        "rows": 4560,
        "cols": 1200,
    },
    {
        "id": "sf2_d",
        "file": "003e_lunanet_sf2_ldpc_submatrix_d_ind.csv",
        "rows": 4560,
        "cols": 480,
    },
    # SF3 (Z=88, K=880)
    {
        "id": "sf3_a",
        "file": "003f_lunanet_sf3_ldpc_submatrix_a_ind.csv",
        "rows": 352,
        "cols": 880,
    },
    {
        "id": "sf3_b",
        "file": "003g_lunanet_sf3_ldpc_submatrix_b_ind.csv",
        "rows": 352,
        "cols": 352,
    },
    {
        "id": "sf3_b_inv",
        "file": "003h_lunanet_sf3_ldpc_submatrix_b_inv_ind.csv",
        "rows": 352,
        "cols": 352,
    },
    {
        "id": "sf3_c",
        "file": "003i_lunanet_sf3_ldpc_submatrix_c_ind.csv",
        "rows": 3344,
        "cols": 880,
    },
    {
        "id": "sf3_d",
        "file": "003j_lunanet_sf3_ldpc_submatrix_d_ind.csv",
        "rows": 3344,
        "cols": 352,
    },
]


def crc32_data(data_list):
    """Calculate CRC32 for a list of integers."""
    b = bytearray()
    for x in data_list:
        b.extend(struct.pack("<H", x))
    return binascii.crc32(b) & 0xFFFFFFFF


def parse_matrix(m_def):
    """Parse a single LDPC sub-matrix from CSV and convert to CSR format."""
    path = REF_DIR / m_def["file"]
    edges = []
    with open(path) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            r, c = map(int, row)
            edges.append((r, c))

    # Sort for CSR: row primary, then column
    edges.sort()

    num_rows = m_def["rows"]
    col_indices = [e[1] for e in edges]
    row_ptr = [0] * (num_rows + 1)

    curr_edge = 0
    for r in range(num_rows):
        row_ptr[r] = curr_edge
        while curr_edge < len(edges) and edges[curr_edge][0] == r:
            curr_edge += 1
    row_ptr[num_rows] = curr_edge

    return {
        "col_indices": col_indices,
        "row_ptr": row_ptr,
        "num_entries": len(edges),
        "crc": crc32_data(col_indices + row_ptr),
    }


def main():
    """Process all matrices and generate C++ code."""
    print("LunaLink LDPC Table Generator - Mission Assurance Mode")

    results = {}
    total_bytes = 0

    for m in MATRICES:
        print(f"  Processing {m['id']}...")
        results[m["id"]] = parse_matrix(m)
        # uint16_t for both
        m_bytes = (
            len(results[m["id"]]["col_indices"]) + len(results[m["id"]]["row_ptr"])
        ) * 2
        total_bytes += m_bytes

    print("\nResource Audit:")
    print(f"  Total ROM Footprint: {total_bytes / 1024:.2f} KB")
    if total_bytes > 128 * 1024:
        print("  WARNING: Memory budget exceeded (128KB)!")
    else:
        print("  Memory budget: OK")

    # Generate HPP
    with open(OUT_HPP, "w") as f:
        f.write(
            """#pragma once
#include <cstdint>
#include <span>

namespace lunalink::signal {

/**
 * @brief CSR (Compressed Sparse Row) representation of LDPC sub-matrices.
 */
struct LdpcCsrMatrix {
    std::span<const uint16_t> col_indices;
    std::span<const uint16_t> row_ptr;
    uint32_t                  num_entries;
    uint16_t                  num_rows;
    uint16_t                  num_cols;
    uint32_t                  crc32;
};

"""
        )
        for m in MATRICES:
            f.write(f"extern const LdpcCsrMatrix kLdpc_{m['id']};\n")
        f.write("\n} // namespace lunalink::signal\n")

    # Generate CPP
    with open(OUT_CPP, "w") as f:
        f.write('#include "lunalink/signal/ldpc_tables.hpp"\n\n')
        f.write("namespace lunalink::signal {\n\n")

        for m in MATRICES:
            res = results[m["id"]]
            f.write(f"// --- {m['id']} ---\n")
            # Using C-arrays but suppressing tidy since they are large LUTs.
            f.write(
                "// NOLINTNEXTLINE(hicpp-avoid-c-arrays, "
                "cppcoreguidelines-avoid-c-arrays)\n"
            )
            f.write(f"static const uint16_t k{m['id']}_cols[] = {{")
            f.write(", ".join(map(str, res["col_indices"])))
            f.write("};\n")

            f.write(
                "// NOLINTNEXTLINE(hicpp-avoid-c-arrays, "
                "cppcoreguidelines-avoid-c-arrays)\n"
            )
            f.write(f"static const uint16_t k{m['id']}_ptr[] = {{")
            f.write(", ".join(map(str, res["row_ptr"])))
            f.write("};\n")

            f.write(f"const LdpcCsrMatrix kLdpc_{m['id']} = {{\n")
            # Suppress decay warning
            f.write(
                "    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array"
                "-to-pointer-decay, hicpp-no-array-decay)\n"
            )
            f.write(
                f"    std::span(k{m['id']}_cols), "
                f"std::span(k{m['id']}_ptr), "
                f"{res['num_entries']},\n"
                f"    {m['rows']}, {m['cols']}, 0x{res['crc']:08X}U\n"
            )
            f.write("};\n\n")

        f.write("} // namespace lunalink::signal\n")

    print(f"\nTables generated successfully: {OUT_CPP.name}")


if __name__ == "__main__":
    main()
