#include "lunalink/signal/ldpc.hpp"
#include <algorithm>
#include <array>
#include <numeric>
#include <cassert>

namespace lunalink::signal {

namespace {

/**
 * @brief Hardened Sparse Matrix-Vector Multiplier (GF2). [LSIS-AFS-320]
 * 
 * Hardening Patterns:
 * 1. Accumulator Mirroring (Soft-TMR): Detects ALU/Register bit-flips.
 * 2. Control-Flow Integrity (CFI): Sentinel terminal verification.
 * 3. WCET Determinism: No early exits on zero bits.
 */
template <typename T_out>
void multiply_sparse_hardened(
    const LdpcCsrMatrix& m,
    std::span<const uint8_t> vec,
    std::span<T_out> out,
    uint32_t& sentinel_out) noexcept {
    
    uint32_t ops_count = 0;
    for (uint16_t r = 0; r < m.num_rows; ++r) {
        uint8_t acc = 0;
        uint8_t acc_mir = 0xFFU; // Mirrored accumulator (bitwise inverse)
        
        const uint32_t start = m.row_ptr[r];
        const uint32_t end = m.row_ptr[r + 1];
        
        for (uint32_t i = start; i < end; ++i) {
            const uint16_t col = m.col_indices[i];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            const uint8_t val = vec[col];
            
            acc ^= val;
            acc_mir ^= val; // Parity is preserved: (acc ^ val) == ~(acc_mir ^ val)
            ops_count++;
        }
        
        // ALU/Register Integrity Check
        if (acc != static_cast<uint8_t>(~acc_mir)) [[unlikely]] {
            sentinel_out = 0U; // Force CFI failure
            return;
        }
        
        out[r] = static_cast<T_out>(acc & 1U);
    }
    sentinel_out = ops_count;
}

} // namespace

/**
 * @brief Encode a navigation sub-block using LSIS-AFS LDPC. [LSIS-AFS-320]
 */
LdpcStatus ldpc_encode(
    LdpcSubframe               type,
    std::span<const uint8_t>   msg,
    std::span<uint8_t>         out) noexcept {
    
    LdpcStatus status = LdpcStatus::kOk;

    if (msg.size() != 200) [[unlikely]] {
        status = LdpcStatus::kInvalidInput;
    } else {
        const bool is_sf2 = (type == LdpcSubframe::kSF2);
        const auto Z = is_sf2 ? static_cast<uint16_t>(120U) : static_cast<uint16_t>(88U);
        const auto K = is_sf2 ? static_cast<uint16_t>(1200U) : static_cast<uint16_t>(880U);
        const auto N_output = is_sf2 ? static_cast<uint16_t>(6000U) : static_cast<uint16_t>(4400U);
        const auto filler_count = is_sf2 ? static_cast<uint8_t>(0U) : static_cast<uint8_t>(10U);

        if (out.size() < N_output) [[unlikely]] {
            status = LdpcStatus::kOutputTooSmall;
        } else {
            // ESA Safety: Scrub local stack buffers.
            std::array<uint8_t, 1200> s{}; // Systematic (padded)
            std::array<uint8_t, 480>  p1{}; // Intermediate parity
            std::array<uint8_t, 4560> p2{}; // Final parity
            
            // 1. Prepare systematic vector 's' [LSIS-320]
            // Copy message (200 bits)
            for (std::size_t i = 0; i < 200; ++i) {
                s.at(i) = msg[i];
            }
            
            // Add Fillers (LSIS §2.4.2)
            if (filler_count > 0) {
                for (std::size_t i = 200; i < 200U + filler_count; ++i) {
                    s.at(i) = 0U;
                }
            }
            // Zero-padding to reach K (Information bits block size)
            for (std::size_t i = 200U + filler_count; i < K; ++i) {
                s.at(i) = 0U;
            }

            // 2. Select Matrices
            const LdpcCsrMatrix &mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
            const LdpcCsrMatrix &mBinv = is_sf2 ? kLdpc_sf2_b_inv : kLdpc_sf3_b_inv;
            const LdpcCsrMatrix &mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
            const LdpcCsrMatrix &mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

            // 3. Stage 1: p1 = B^-1 * (A * s) [LSIS-320]
            uint32_t sentinel_a = 0;
            uint32_t sentinel_binv = 0;
            std::array<uint8_t, 480> temp_as{};
            
            multiply_sparse_hardened(mA, std::span(s).subspan(0, K), std::span(temp_as).subspan(0, mA.num_rows), sentinel_a);
            multiply_sparse_hardened(mBinv, std::span(temp_as).subspan(0, mA.num_rows), std::span(p1).subspan(0, mBinv.num_rows), sentinel_binv);

            if (sentinel_a != mA.num_entries || sentinel_binv != mBinv.num_entries) [[unlikely]] {
                status = LdpcStatus::kFaultDetected;
            } else {
                // 4. Stage 2: p2 = C*s + D*p1 [LSIS-320]
                uint32_t sentinel_c = 0;
                uint32_t sentinel_d = 0;
                std::array<uint8_t, 4560> temp_cs{};
                std::array<uint8_t, 4560> temp_dp1{};

                multiply_sparse_hardened(mC, std::span(s).subspan(0, K), std::span(temp_cs).subspan(0, mC.num_rows), sentinel_c);
                multiply_sparse_hardened(mD, std::span(p1).subspan(0, mD.num_cols), std::span(temp_dp1).subspan(0, mD.num_rows), sentinel_d);

                if (sentinel_c != mC.num_entries || sentinel_d != mD.num_entries) [[unlikely]] {
                    status = LdpcStatus::kFaultDetected;
                } else {
                    for (uint16_t i = 0; i < mC.num_rows; ++i) {
                        p2.at(i) = static_cast<uint8_t>(temp_cs.at(i) ^ temp_dp1.at(i));
                    }

                    // 5. Assembly and Puncturing [LSIS-320]
                    // Systematic bits (remaining after puncture of 2*Z bits)
                    const auto puncture_start = static_cast<std::size_t>(2U * Z);
                    std::size_t out_idx = 0;

                    for (std::size_t i = puncture_start; i < K; ++i) {
                        out[out_idx++] = s.at(i);
                    }
                    // Parity p1
                    for (std::size_t i = 0; i < mA.num_rows; ++i) {
                        out[out_idx++] = p1.at(i);
                    }
                    // Parity p2
                    for (std::size_t i = 0; i < mC.num_rows; ++i) {
                        if (out_idx < N_output) {
                            out[out_idx++] = p2.at(i);
                        }
                    }
                }
                
                // JAXA Safety: Stack Scrubbing.
                std::fill(temp_cs.begin(), temp_cs.end(), 0U);
                std::fill(temp_dp1.begin(), temp_dp1.end(), 0U);
            }
            
            // JAXA Safety: Stack Scrubbing.
            std::fill(s.begin(), s.end(), 0U);
            std::fill(p1.begin(), p1.end(), 0U);
            std::fill(p2.begin(), p2.end(), 0U);
            std::fill(temp_as.begin(), temp_as.end(), 0U);
        }
    }

    return status;
}

} // namespace lunalink::signal
