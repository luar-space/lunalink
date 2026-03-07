#!/usr/bin/env python3
"""Generate flight-hardened LDPC sub-matrix tables for LSIS-AFS.

Converts CSV indices into CSR (Compressed Sparse Row) C++ structures.
Includes Matrix-CRC32 for Mission Assurance (CBIT).
"""

import binascii
import csv
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


def calculate_matrix_crc(res, m_def):
    """Calculate a stable CRC32 for the CSR structure including metadata."""
    data = bytearray()
    # 1. num_entries (u32 LE)
    data.extend(res["num_entries"].to_bytes(4, "little"))
    # 2. num_rows (u16 LE)
    data.extend(m_def["rows"].to_bytes(2, "little"))
    # 3. num_cols (u16 LE)
    data.extend(m_def["cols"].to_bytes(2, "little"))
    # 4. col_indices (u16 LE per element)
    for val in res["col_indices"]:
        data.extend(val.to_bytes(2, "little"))
    # 5. row_ptr (u16 LE per element)
    for val in res["row_ptr"]:
        data.extend(val.to_bytes(2, "little"))
    return binascii.crc32(data) & 0xFFFFFFFF


def parse_matrix(m_def):
    """Parse matrix and convert to CSR."""
    path = REF_DIR / m_def["file"]
    if not path.exists():
        alt_path = REF_DIR / m_def["file"].replace("lunalink", "lunanet")
        if alt_path.exists():
            path = alt_path
        else:
            alt_path = REF_DIR / m_def["file"].replace("lunanet", "lunalink")
            if alt_path.exists():
                path = alt_path

    edges = []
    with open(path) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            r, c = map(int, row)
            edges.append((r, c))

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

    res = {
        "col_indices": col_indices,
        "row_ptr": row_ptr,
        "num_entries": len(col_indices),
    }
    res["crc"] = calculate_matrix_crc(res, m_def)
    return res


def main():
    """Generate all CSR tables with Mission Assurance hardening."""
    results = {}
    for m in MATRICES:
        results[m["id"]] = parse_matrix(m)

    # Generate HPP
    with open(OUT_HPP, "w") as f:
        f.write(
            "#pragma once\n"
            "#include <cstdint>\n"
            "#include <span>\n\n"
            "namespace lunalink::signal {\n\n"
            "/**\n"
            " * @brief CSR (Compressed Sparse Row) representation of LDPC "
            "sub-matrices.\n"
            " */\n"
            "struct LdpcCsrMatrix {\n"
            "    std::span<const uint16_t> col_indices;\n"
            "    std::span<const uint16_t> row_ptr;\n"
            "    uint32_t                  num_entries;\n"
            "    uint16_t                  num_rows;\n"
            "    uint16_t                  num_cols;\n"
            "    uint32_t                  crc32;\n\n"
            "    /**\n"
            "     * @brief Perform a JIT integrity check of matrix structure "
            "using CRC32. [CBIT-AFS-LDPC]\n"
            "     */\n"
            "    [[nodiscard]] bool verify_integrity() const noexcept;\n"
            "};\n\n"
        )
        for m in MATRICES:
            f.write(f"extern const LdpcCsrMatrix kLdpc_{m['id']};\n")
        f.write("\n} // namespace lunalink::signal\n")

    # Generate CPP
    with open(OUT_CPP, "w") as f:
        f.write('#include "lunalink/signal/ldpc_tables.hpp"\n')
        f.write('#include "lunalink/signal/safety.hpp"\n\n')
        f.write("namespace lunalink::signal {\n\n")

        # Flight-Hardened JIT integrity check implementation
        f.write("bool LdpcCsrMatrix::verify_integrity() const noexcept {\n")
        f.write("    uint32_t crc = 0xFFFFFFFFU;\n")
        f.write("    auto update_u32 = [&](uint32_t val) {\n")
        f.write("        for (uint32_t i = 0; i < 4U; i++) {\n")
        f.write(
            "            crc ^= static_cast<uint8_t>("
            "(static_cast<uint32_t>(val) >> (i * 8U)) & 0xFFU);\n"
        )
        f.write("            for (uint32_t j = 0; j < 8U; j++) {\n")
        f.write("                crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));\n")
        f.write("            }\n")
        f.write("        }\n")
        f.write("    };\n")
        f.write("    auto update_u16 = [&](uint16_t val) {\n")
        f.write("        for (uint32_t i = 0; i < 2U; i++) {\n")
        f.write(
            "            crc ^= static_cast<uint8_t>("
            "(static_cast<uint32_t>(val) >> (i * 8U)) & 0xFFU);\n"
        )
        f.write("            for (uint32_t j = 0; j < 8U; j++) {\n")
        f.write("                crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));\n")
        f.write("            }\n")
        f.write("        }\n")
        f.write("    };\n")
        f.write("    update_u32(num_entries);\n")
        f.write("    update_u16(num_rows);\n")
        f.write("    update_u16(num_cols);\n")
        f.write("    for (uint16_t val : col_indices) { update_u16(val); }\n")
        f.write("    for (uint16_t val : row_ptr) { update_u16(val); }\n")
        f.write("    return (crc ^ 0xFFFFFFFFU) == crc32;\n")
        f.write("}\n\n")

        for m in MATRICES:
            res = results[m["id"]]
            f.write(f"// --- {m['id']} ---\n")
            f.write(
                "alignas(64) LUNALINK_LUT_SECTION "
                "// NOLINTNEXTLINE(hicpp-avoid-c-arrays, "
                "cppcoreguidelines-avoid-c-arrays)\n"
            )
            f.write(f"static const uint16_t k{m['id']}_cols[] = {{")
            f.write(", ".join(map(str, res["col_indices"])))
            f.write("};\n")

            f.write(
                "alignas(64) LUNALINK_LUT_SECTION "
                "// NOLINTNEXTLINE(hicpp-avoid-c-arrays, "
                "cppcoreguidelines-avoid-c-arrays)\n"
            )
            f.write(f"static const uint16_t k{m['id']}_ptr[] = {{")
            f.write(", ".join(map(str, res["row_ptr"])))
            f.write("};\n")

            f.write(f"const LdpcCsrMatrix kLdpc_{m['id']} = {{\n")
            f.write(
                "    // NOLINTNEXTLINE("
                "cppcoreguidelines-pro-bounds-array-to-pointer-decay, "
                "hicpp-no-array-decay)\n"
            )
            f.write(
                f"    std::span(k{m['id']}_cols), "
                f"std::span(k{m['id']}_ptr), {res['num_entries']},\n"
            )
            f.write(f"    {m['rows']}, {m['cols']}, 0x{res['crc']:08X}U\n")
            f.write("};\n\n")

        f.write("} // namespace lunalink::signal\n")

    print(f"Generated {OUT_CPP.name} with Mission Assurance hardening.")


if __name__ == "__main__":
    main()
