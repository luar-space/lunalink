#pragma once
#include <cstdint>
#include <span>

namespace lunalink::signal {

/**
 * @brief CSR (Compressed Sparse Row) representation of LDPC sub-matrices.
 */
struct LdpcCsrMatrix {
    std::span<const uint16_t> col_indices;
    std::span<const uint32_t> row_ptr;
    uint32_t num_entries;
    uint16_t num_rows;
    uint16_t num_cols;
    uint32_t crc32;

    /**
     * @brief Perform a JIT integrity check of matrix structure using CRC32. [CBIT-AFS-LDPC]
     */
    [[nodiscard]] bool verify_integrity() const noexcept;

#ifdef LUNALINK_FAULT_INJECTION
    // Fault injection overlay — mutable so inject_bit_flip works on const matrices.
    mutable uint8_t   fi_active{0};       // 1 if a fault is injected
    mutable size_t    fi_byte_offset{0};  // byte index within col_indices raw bytes
    mutable uint8_t   fi_bit_mask{0};     // XOR mask applied to that byte

    /**
     * @brief Flip a bit in the col_indices data used by verify_integrity().
     * Causes the CRC check to fail, demonstrating Zone 3 detection.
     * Only available when LUNALINK_FAULT_INJECTION=1.
     */
    void inject_bit_flip(size_t byte_offset, uint8_t bit_mask) const noexcept {
        fi_active      = 1U;
        fi_byte_offset = byte_offset;
        fi_bit_mask    = bit_mask;
    }

    void clear_fault() const noexcept {
        fi_active = 0U;
    }
#endif
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
