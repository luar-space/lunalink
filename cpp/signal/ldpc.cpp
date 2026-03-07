#include "lunalink/signal/ldpc.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>

namespace lunalink::signal {

namespace {

/**
 * @brief CFI Magic Patterns (High-entropy to detect multi-bit radiation jumps).
 */
constexpr uint32_t kCfiMagicStart = 0xA5A5A5A5U;
constexpr uint32_t kCfiMagicEnd = 0x5A5A5A5AU;

/**
 * @brief Sparse Matrix-Vector Multiplication (Hardened CSR). [LSIS-AFS-320]
 * Includes Mirror Variables and CFI to detect radiation-induced logic skips.
 */
template <typename T_out>
[[nodiscard]] bool multiply_sparse_hardened(const LdpcCsrMatrix& m,
                                           std::span<const uint8_t> vec,
                                           std::span<T_out> out) noexcept {
  uint32_t ops_count = kCfiMagicStart;
  uint32_t ops_count_mir = ~kCfiMagicStart;
  bool success = true;

  for (uint16_t r = 0; r < m.num_rows; ++r) {
    uint8_t acc = 0;
    uint8_t acc_mir = 0xFFU;

    const uint32_t start = m.row_ptr[r];
    const uint32_t end = m.row_ptr[r + 1];

    for (uint32_t i = start; i < end; ++i) {
      const uint16_t col = m.col_indices[i];
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      const uint8_t val = vec[col];

      acc ^= val;
      acc_mir ^= val;
      ops_count++;
      ops_count_mir--;
      wip_tick();
    }

    if (acc != static_cast<uint8_t>(~acc_mir)) {
      success = false;
    }
    out[r] = static_cast<T_out>(acc & 1U);
    wip_tick();
  }

  const uint32_t total_ops = ops_count - kCfiMagicStart;
  return success && (total_ops == m.num_entries) &&
         (ops_count == static_cast<uint32_t>(~ops_count_mir)) &&
         (kCfiMagicEnd != 0);
}

/**
 * @brief Input Bit-Validation Gate. [NASA-FSW-SAFETY]
 */
[[nodiscard]] bool validate_bits(std::span<const uint8_t> bits) noexcept {
  uint8_t fault_mask = 0;
  for (const uint8_t b : bits) {
    fault_mask |= (b & 0xFEU);
  }
  return (fault_mask == 0);
}

/**
 * @brief Assemble the final punctured codeword. [LSIS-AFS-320]
 * Hardened with CFI to ensure correct symbol count.
 */
[[nodiscard]] bool assemble_codeword_hardened(const uint16_t Z, const uint16_t K,
                                              const uint16_t N_output,
                                              std::span<const uint8_t> s,
                                              std::span<const uint8_t> p1,
                                              std::span<const uint8_t> p2,
                                              std::span<uint8_t> out) noexcept {
  const auto puncture_start = static_cast<std::size_t>(2U * Z);
  std::size_t out_idx = 0;
  uint32_t cfi_count = kCfiMagicStart;

  for (std::size_t i = puncture_start; i < K; ++i) {
    out[out_idx++] = s[i];
    cfi_count++;
    wip_tick();
  }
  for (const uint8_t p : p1) {
    out[out_idx++] = p;
    cfi_count++;
    wip_tick();
  }
  for (const uint8_t p : p2) {
    if (out_idx < N_output) {
      out[out_idx++] = p;
      cfi_count++;
      wip_tick();
    }
  }

  return (out_idx == N_output) && (cfi_count == (kCfiMagicStart + N_output));
}

}  // namespace

LdpcStatus ldpc_encode(LdpcSubframe type, std::span<const uint8_t> msg,
                        std::span<uint8_t> out) noexcept {
  LdpcStatus status = LdpcStatus::kOk;

  if (msg.size() != 200 || !validate_bits(msg)) {
    status = LdpcStatus::kInvalidInput;
  } else {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const auto Z =
        is_sf2 ? static_cast<uint16_t>(120U) : static_cast<uint16_t>(88U);
    const auto K =
        is_sf2 ? static_cast<uint16_t>(1200U) : static_cast<uint16_t>(880U);
    const auto N_output =
        is_sf2 ? static_cast<uint16_t>(2400U) : static_cast<uint16_t>(1740U);
    const auto filler_count =
        is_sf2 ? static_cast<uint8_t>(0U) : static_cast<uint8_t>(10U);

    if (out.size() < N_output) {
      status = LdpcStatus::kOutputTooSmall;
    } else {
      const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
      const LdpcCsrMatrix& mBinv = is_sf2 ? kLdpc_sf2_b_inv : kLdpc_sf3_b_inv;
      const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
      const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

      if (!mA.verify_integrity() || !mBinv.verify_integrity() ||
          !mC.verify_integrity() || !mD.verify_integrity()) {
        status = LdpcStatus::kFaultDetected;
      } else {
        std::array<uint8_t, 1200> s{};
        std::array<uint8_t, 480> p1{};
        std::array<uint8_t, 4560> p2{};

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        for (std::size_t i = 0; i < 200; ++i) {
          s[i] = msg[i];
        }
        if (filler_count > 0) {
          for (std::size_t i = 200; i < 200U + filler_count; ++i) {
            s[i] = 0U;
          }
        }
        for (std::size_t i = 200U + filler_count; i < K; ++i) {
          s[i] = 0U;
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

        std::array<uint8_t, 480> temp_as{};
        if (!multiply_sparse_hardened(mA, std::span(s).subspan(0, K),
                                      std::span(temp_as).subspan(0,
                                                                 mA.num_rows)) ||
            !multiply_sparse_hardened(
                mBinv, std::span(temp_as).subspan(0, mA.num_rows),
                std::span(p1).subspan(0, mBinv.num_rows))) {
          status = LdpcStatus::kFaultDetected;
        } else {
          std::array<uint8_t, 4560> temp_cs{};
          std::array<uint8_t, 4560> temp_dp1{};

          if (!multiply_sparse_hardened(mC, std::span(s).subspan(0, K),
                                        std::span(temp_cs).subspan(0,
                                                                   mC.num_rows)) ||
              !multiply_sparse_hardened(
                  mD, std::span(p1).subspan(0, mD.num_cols),
                  std::span(temp_dp1).subspan(0, mD.num_rows))) {
            status = LdpcStatus::kFaultDetected;
          } else {
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            for (uint16_t i = 0; i < mC.num_rows; ++i) {
              p2[i] = static_cast<uint8_t>(temp_cs[i] ^ temp_dp1[i]);
            }
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
            if (!assemble_codeword_hardened(
                    Z, K, N_output, s, std::span(p1).subspan(0, mBinv.num_rows),
                    std::span(p2).subspan(0, mC.num_rows), out)) {
              status = LdpcStatus::kFaultDetected;
            }
          }
          secure_scrub(temp_cs);
          secure_scrub(temp_dp1);
        }

        secure_scrub(s);
        secure_scrub(p1);
        secure_scrub(p2);
        secure_scrub(temp_as);
      }
    }
  }

  return (status == LdpcStatus::kOk && kCfiMagicEnd != 0) ? LdpcStatus::kOk
                                                         : status;
}

}  // namespace lunalink::signal
