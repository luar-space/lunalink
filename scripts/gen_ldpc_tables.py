#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate flight-hardened LDPC sub-matrix tables for LSIS-AFS.

Converts CSV indices into CSR (Compressed Sparse Row) C++ structures.
Includes Matrix-CRC32 for Mission Assurance (CBIT).
"""

import csv
import zlib
from pathlib import Path

# Paths
ROOT = Path(__file__).parent.parent
REF_DIR = ROOT / "docs" / "references"
OUT_CPP = ROOT / "cpp" / "signal" / "ldpc_tables.cpp"

# Matrix definitions based on LSIS Vol A Annex 2 Mother Code
# SF2: Z=120, K=1200, N_mother=6240
# SF3: Z=88, K=880, N_mother=4576
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


def calculate_matrix_crc(res, m_def):
    """Calculate CRC32 of the matrix data AND metadata for Mission Assurance.

    Covers: num_entries (4B LE), num_rows (2B LE), num_cols (2B LE),
    then col_indices (2B LE each), then row_ptr (4B LE each).
    This ensures that a SEU flipping num_rows/num_cols/num_entries
    is detected by the JIT integrity check (C3 compliance).
    """
    data = bytearray()
    # Metadata first — must match C++ verify_integrity() byte order
    data.extend(int(res["num_entries"]).to_bytes(4, "little"))
    data.extend(int(res["rows"]).to_bytes(2, "little"))
    data.extend(int(res["cols_count"]).to_bytes(2, "little"))
    # Then array data
    for val in res["cols"]:
        data.extend(int(val).to_bytes(2, "little"))
    for val in res["ptr"]:
        data.extend(int(val).to_bytes(4, "little"))
    return zlib.crc32(data) & 0xFFFFFFFF


def parse_matrix(m_def):  # noqa: D103
    path = REF_DIR / m_def["file"]
    if not path.exists():
        # Fallback for local dev names if needed
        alt_path = REF_DIR / m_def["file"].replace("lunanet", "lunalink")
        if alt_path.exists():
            path = alt_path

    edges_raw = []
    min_val = 9999
    num_rows = m_def["rows"]
    with open(path) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            r, c = map(int, row)  # CSV is (row, col) as per README
            # Filter rows within spec range
            if r < num_rows:
                edges_raw.append((r, c))
                min_val = min(min_val, r, c)

    # Normalize to 0-based
    offset = 1 if min_val == 1 else 0
    edges = [(r - offset, c - offset) for r, c in edges_raw]

    edges.sort()
    col_indices = [e[1] for e in edges]
    row_ptr = [0] * (num_rows + 1)
    curr_edge = 0
    for r in range(num_rows):
        row_ptr[r] = curr_edge
        while curr_edge < len(edges) and edges[curr_edge][0] == r:
            curr_edge += 1
    row_ptr[num_rows] = len(edges)

    return {
        "cols": col_indices,
        "ptr": row_ptr,
        "num_entries": len(edges),
        "rows": num_rows,
        "cols_count": m_def["cols"],
    }


def main():  # noqa: D103
    results = {}
    for m in MATRICES:
        results[m["id"]] = parse_matrix(m)
        results[m["id"]]["crc"] = calculate_matrix_crc(results[m["id"]], m)

    with open(OUT_CPP, "w") as f:
        f.write("// NOLINTBEGIN(hicpp-avoid-c-arrays,hicpp-signed-bitwise)\n")
        f.write('#include "lunalink/encoding/ldpc_tables.hpp"\n')
        f.write('#include "lunalink/core/safety.hpp"\n\n')
        f.write("namespace lunalink::signal {\n\n")

        for m_id, res in results.items():
            f.write(
                f"// --- {m_id} ---\n"
                f"alignas(64) LUNALINK_LUT_SECTION\n"
                f"static const uint16_t k{m_id}_cols[] = {{"
            )
            f.write(", ".join(map(str, res["cols"])))
            f.write("};\n\n")

            f.write(
                f"alignas(64) LUNALINK_LUT_SECTION\n"
                f"static const uint32_t k{m_id}_ptr[] = {{"
            )
            f.write(", ".join(map(str, res["ptr"])))
            f.write("};\n\n")

            nnz = res["num_entries"]
            nr = res["rows"]
            nc = res["cols_count"]
            crc = res["crc"]
            f.write(
                f"const LdpcCsrMatrix kLdpc_{m_id} = {{\n"
                f"    std::span(k{m_id}_cols),\n"
                f"    std::span(k{m_id}_ptr),\n"
                f"    {nnz}, {nr}, {nc}, 0x{crc:08X}U\n"
                f"}};\n\n"
            )

        f.write("} // namespace lunalink::signal\n\n")

        # Generate verify_integrity() with inline CRC32 (tidy-clean)
        f.write("namespace lunalink::signal {\n")
        f.write("namespace {\n")
        f.write("// NOLINTNEXTLINE(misc-no-recursion)\n")
        f.write("uint32_t crc32_byte(uint32_t crc, uint8_t byte) noexcept {\n")
        f.write("    static constexpr uint32_t kP = 0xEDB88320U;\n")
        f.write("    crc ^= byte;\n")
        f.write("    for (int b = 0; b < 8; ++b) {\n")
        f.write("        crc = (crc >> 1U) ^ (kP & (0U - (crc & 1U)));\n")
        f.write("    }\n")
        f.write("    return crc;\n")
        f.write("}\n")
        f.write("} // namespace\n\n")
        f.write("bool LdpcCsrMatrix::verify_integrity() const noexcept {\n")
        f.write("    if (col_indices.empty() || row_ptr.empty()) { return false; }\n")
        f.write(
            "    if (row_ptr.size() != static_cast<size_t>"
            "(num_rows) + 1U) { return false; }\n"
        )
        f.write("    if (col_indices.size() != num_entries) { return false; }\n")
        f.write("    uint32_t crc = 0xFFFFFFFFU;\n")
        # C3: CRC covers metadata (num_entries, num_rows, num_cols) first
        f.write("    // C3: CRC covers metadata to detect SEU on dimension fields.\n")
        f.write(
            "    crc = crc32_byte(crc, static_cast<uint8_t>(num_entries & 0xFFU));\n"
        )
        f.write(
            "    crc = crc32_byte(crc,"
            " static_cast<uint8_t>((num_entries >> 8U) & 0xFFU));\n"
        )
        f.write(
            "    crc = crc32_byte(crc,"
            " static_cast<uint8_t>((num_entries >> 16U) & 0xFFU));\n"
        )
        f.write(
            "    crc = crc32_byte(crc,"
            " static_cast<uint8_t>((num_entries >> 24U) & 0xFFU));\n"
        )
        f.write("    crc = crc32_byte(crc, static_cast<uint8_t>(num_rows & 0xFFU));\n")
        f.write(
            "    crc = crc32_byte(crc,"
            " static_cast<uint8_t>((num_rows >> 8U) & 0xFFU));\n"
        )
        f.write("    crc = crc32_byte(crc, static_cast<uint8_t>(num_cols & 0xFFU));\n")
        f.write(
            "    crc = crc32_byte(crc,"
            " static_cast<uint8_t>((num_cols >> 8U) & 0xFFU));\n"
        )
        # Then array data
        f.write("    for (const auto c : col_indices) {\n")
        f.write("        crc = crc32_byte(crc, static_cast<uint8_t>(c & 0xFFU));\n")
        f.write(
            "        crc = crc32_byte(crc, static_cast<uint8_t>((c >> 8U) & 0xFFU));\n"
        )
        f.write("    }\n")
        f.write("    for (const auto p : row_ptr) {\n")
        f.write("        crc = crc32_byte(crc, static_cast<uint8_t>(p & 0xFFU));\n")
        f.write(
            "        crc = crc32_byte(crc, static_cast<uint8_t>((p >> 8U) & 0xFFU));\n"
        )
        f.write(
            "        crc = crc32_byte(crc, static_cast<uint8_t>((p >> 16U) & 0xFFU));\n"
        )
        f.write(
            "        crc = crc32_byte(crc, static_cast<uint8_t>((p >> 24U) & 0xFFU));\n"
        )
        f.write("    }\n")
        f.write("    return (~crc) == crc32;\n")
        f.write("}\n")
        f.write("} // namespace lunalink::signal\n")
        f.write("// NOLINTEND(hicpp-avoid-c-arrays,hicpp-signed-bitwise)\n")

    print(f"Generated {OUT_CPP.name} with Mother Code support.")


if __name__ == "__main__":
    main()
