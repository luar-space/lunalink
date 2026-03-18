#pragma once
#include <cstdint>
#include <span>
#include <array>
#include "lunalink/core/safety.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for LDPC operations.
 */
enum class LdpcStatus : uint8_t {
    kOk            = 0x5AU,  // 01011010
    kInvalidInput  = 0xA5U,  // 10100101
    kOutputTooSmall = 0x33U,  // 00110011
    kFaultDetected = 0x99U,  // 10011001
    kNotConverged  = 0x66U,  // 01100110
};

/**
 * @brief LDPC Subframe types supported by LSIS-AFS.
 * Hardened with non-contiguous bit patterns.
 */
enum class LdpcSubframe : uint8_t {
    kSF2 = 0x5AU,
    kSF3 = 0xA5U,
    kSF4 = 0x33U  // SF4 reuses SF3 matrices
};

/**
 * @brief Scratchpad memory for LDPC encoding to keep large temporaries off stack.
 * SF2 mother code requires the largest buffers (~13.8 KB total).
 * Caller must allocate (heap or dedicated memory section on flight hardware).
 */
struct LdpcEncodeWorkspace {
    std::array<uint8_t, 1200> s{};
    std::array<uint8_t, 480>  p1{};
    std::array<uint8_t, 4560> p2{};
    std::array<uint8_t, 480>  t_as{};
    std::array<uint8_t, 4560> t_cs{};
    std::array<uint8_t, 4560> t_dp1{};
};

/**
 * @brief Encode a navigation subframe using rate 1/2 LDPC. [LSIS-AFS-320]
 *
 * Two-stage sparse encoding: p1 = B^-1·(A·s), p2 = C·s + D·p1 (mod 2).
 * Includes post-encode H·c=0 self-check for fault detection.
 *
 * @param type Subframe type (SF2: 1200->2400, SF3/4: 870->1740).
 * @param msg  Input message bits (including 24-bit CRC).
 * @param out  Output encoded symbols.
 * @param workspace Scratchpad memory for encoder temporaries.
 *
 * @return LdpcStatus::kOk or error code.
 */
[[nodiscard]] LdpcStatus ldpc_encode(
    LdpcSubframe               type,
    std::span<const uint8_t>   msg,
    std::span<uint8_t>         out,
    LdpcEncodeWorkspace&       workspace) noexcept;

/**
 * @brief Scratchpad memory for LDPC decoding to ensure re-entrancy.
 * Large buffers are moved out of the function stack and static memory.
 * Uses 16-bit fixed-point (Q8.7) to ensure determinism and efficiency.
 *
 * WARNING: ~85 KB total. Must be heap-allocated or placed in a dedicated
 * memory section on flight hardware. Do NOT allocate on a task stack.
 */
struct LdpcWorkspace {
    std::array<int16_t, 6240>  L_v{};   // Variable-node LLRs (Mother length)
    std::array<int16_t, 32000> L_cv{};  // Check-to-variable messages (Sparse parts)
    std::array<int16_t, 4560>  L_cv_identity{}; // Messages for identity part

    // NMS scaling factor α = alpha_numerator / alpha_denominator.
    // Default 2/3 ≈ 0.667 — optimized empirically for the LSIS-AFS LDPC
    // codes (n=2400 rate-1/2 and n=1740 rate-1/2). Outperforms the standard
    // α=3/4 by ~0.3 dB in the waterfall region.
    // Valid range: numerator in [1,7], denominator in [1,8], numerator <= denominator.
    int16_t alpha_numerator   = 2;
    int16_t alpha_denominator = 3;
};

/**
 * @brief Decode a navigation sub-block using Layered Normalized Min-Sum BP. [LSIS-AFS-321]
 *
 * @param type       Subframe type.
 * @param symbols    Input channel LLRs (log-likelihood ratios).
 *                   For BPSK AWGN: L_ch = 2*y/sigma^2, where y is the
 *                   received sample and sigma^2 is the noise variance.
 *                   Positive = likely bit 0, negative = likely bit 1.
 * @param iterations Number of BP iterations.
 * @param out        Buffer for decoded bits (1200 or 870 bits).
 * @param workspace  Scratchpad memory for this decoding instance.
 *
 * @return LdpcStatus::kOk on convergence, kNotConverged if max iterations reached.
 */
[[nodiscard]] LdpcStatus ldpc_decode(
    LdpcSubframe               type,
    std::span<const float>     symbols,
    uint8_t                    iterations,
    std::span<uint8_t>         out,
    LdpcWorkspace&             workspace) noexcept;

} // namespace lunalink::signal
