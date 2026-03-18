#pragma once
#include <cstdint>
#include <span>
#include "lunalink/core/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status for CRC operations.
 */
enum class CrcStatus : uint8_t {
    kOk             = 0x5AU,
    kNullInput      = 0xCCU,
    kOutputTooSmall = 0x33U,
    kFaultDetected  = 0x99U
};

inline constexpr uint8_t kCrc24Bits = 24;

/**
 * @brief Calculate 24-bit CRC (CRC-24Q). [LSIS-FID0-467]
 * Polynomial: G(X) = (1+X)P(X) = 0x864CFB.
 * Seed: 0.
 * Bit ordering: MSB first.
 *
 * @param in Input bit sequence (one bit per byte, 0 or 1).
 * @param out_crc Pointer to 24-bit output (3 bytes or 24 bit-bytes).
 * @return CrcStatus
 */
[[nodiscard]] CrcStatus crc24_calculate(
    std::span<const uint8_t> in,
    std::span<uint8_t, 24ULL> out_crc) noexcept;

/**
 * @brief Hardened CRC verification. [LSIS-FID0-467]
 * @return true if CRC matches.
 */
[[nodiscard]] bool crc24_verify(
    std::span<const uint8_t> data_with_crc) noexcept;

} // namespace lunalink::signal
