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
 * @brief 60x98 Block Interleaver (LSIS-FID0-470).
 * Write row-wise, read column-wise.
 * 
 * Uses Class A Hardening: CFI sentinels and mirrored calculation.
 */
[[nodiscard]] InterleaverStatus block_interleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept;

/**
 * @brief 60x98 Block De-interleaver.
 * Write column-wise, read row-wise.
 */
[[nodiscard]] InterleaverStatus block_deinterleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept;

} // namespace lunalink::signal
