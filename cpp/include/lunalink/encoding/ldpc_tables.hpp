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
  /// Total number of non-zero entries in the sparse matrix.
  /// Invariant: must equal col_indices.size(). Checked at runtime by
  /// verify_integrity() (CRC-32 over col_indices with length validation).
  uint32_t num_entries;
  /// Number of check equations (rows).
  uint16_t num_rows;
  /// Number of variable nodes (columns).
  uint16_t num_cols;
  /// Build-time CRC-32; verified by verify_integrity() at runtime.
  uint32_t crc32;

  /**
   * @brief Perform a JIT integrity check of matrix structure using CRC32. [CBIT-AFS-LDPC]
   */
  [[nodiscard]] bool verify_integrity() const noexcept;

#ifdef LUNALINK_FAULT_INJECTION
  // Fault injection overlay — mutable so inject_bit_flip works on const matrices.
  mutable uint8_t fi_active{0};     // 1 if a fault is injected
  mutable size_t fi_byte_offset{0}; // byte index within col_indices raw bytes
  mutable uint8_t fi_bit_mask{0};   // XOR mask applied to that byte

  /**
   * @brief Flip a bit in the col_indices data used by verify_integrity().
   * Causes the CRC check to fail, demonstrating Zone 3 detection.
   * Only available when LUNALINK_FAULT_INJECTION=1.
   */
  void inject_bit_flip(size_t byte_offset, uint8_t bit_mask) const noexcept {
    fi_active = 1U;
    fi_byte_offset = byte_offset;
    fi_bit_mask = bit_mask;
  }

  void clear_fault() const noexcept {
    fi_active = 0U;
  }
#endif
};

// ─── LDPC Base Graph Metadata (encoding_parameters.csv) ─────────────────────

/// 3GPP TS 38.212 Base Graph 2 dimensions.
inline constexpr uint8_t kLdpcBg2Rows = 42;
inline constexpr uint8_t kLdpcBg2Cols = 52;
inline constexpr uint8_t kLdpcBg2Kb = 10;  ///< Information columns.

/// SF2 lifting parameters (LSIS V1.0 §2.4.3).
inline constexpr uint8_t kLdpcSf2Zc = 120;   ///< Lifting size.
inline constexpr uint8_t kLdpcSf2Ils = 7;    ///< Lifting set index.
inline constexpr uint16_t kLdpcSf2K = 1200;  ///< Systematic bits.
inline constexpr uint16_t kLdpcSf2N = 2400;  ///< Encoded symbols.

/// SF3/SF4 lifting parameters (LSIS V1.0 §2.4.3).
inline constexpr uint8_t kLdpcSf34Zc = 88;   ///< Lifting size.
inline constexpr uint8_t kLdpcSf34Ils = 5;   ///< Lifting set index.
inline constexpr uint16_t kLdpcSf34K = 880;  ///< Systematic bits (870 data + 10 filler).
inline constexpr uint16_t kLdpcSf34N = 1740; ///< Encoded symbols.

/**
 * LDPC parity-check matrix H = [A B; C D I] in CSR format.
 * SF2: (n=2400, k=1200, rate 1/2).
 * SF3/SF4: (n=1740, k=870, rate 1/2).
 * [LSIS-AFS-320]
 */
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
