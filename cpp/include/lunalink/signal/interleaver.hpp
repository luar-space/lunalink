#pragma once
#include <cstdint>
#include <span>
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status for Interleaver operations.
 */
enum class InterleaverStatus : uint8_t {
    kOk             = 0x5AU,
    kInputTooSmall  = 0xCCU,
    kOutputTooSmall = 0x33U,
    kFaultDetected  = 0x99U
};

inline constexpr uint16_t kInterleaverRows = 60;
inline constexpr uint16_t kInterleaverCols = 98;
inline constexpr uint16_t kInterleaverSize = kInterleaverRows * kInterleaverCols; // 5880

/**
 * @brief 60x98 Block Interleaver. [LSIS-FID0-470]
 * Write row-wise, read column-wise.
 *
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard]] InterleaverStatus block_interleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept;

/**
 * @brief 60x98 Block De-interleaver. [LSIS-FID0-470]
 * Write column-wise, read row-wise.
 *
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard]] InterleaverStatus block_deinterleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept;

/**
 * @brief Soft-decision 60x98 Block De-interleaver. [LSIS-AFS-471]
 *
 * Identical permutation to block_deinterleave() but operates on float
 * soft-decision values (LLRs) to preserve soft information for FEC decoding.
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard]] InterleaverStatus block_deinterleave_soft(
    std::span<const float> in,
    std::span<float>       out) noexcept;

} // namespace lunalink::signal
