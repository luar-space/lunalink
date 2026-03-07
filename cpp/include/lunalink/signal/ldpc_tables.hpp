#pragma once
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

    /**
     * @brief Perform a JIT integrity check of matrix structure using CRC32. [CBIT-AFS-LDPC]
     */
    [[nodiscard]] bool verify_integrity() const noexcept;
};

extern const LdpcCsrMatrix kLdpc_sf2_a;
extern const LdpcCsrMatrix kLdpc_sf2_b;
extern const LdpcCsrMatrix kLdpc_sf2_b_inv;
extern const LdpcCsrMatrix kLdpc_sf2_c;
extern const LdpcCsrMatrix kLdpc_sf2_d;
extern const LdpcCsrMatrix kLdpc_sf3_a;
extern const LdpcCsrMatrix kLdpc_sf3_b;
extern const LdpcCsrMatrix kLdpc_sf3_b_inv;
extern const LdpcCsrMatrix kLdpc_sf3_c;
extern const LdpcCsrMatrix kLdpc_sf3_d;

} // namespace lunalink::signal
