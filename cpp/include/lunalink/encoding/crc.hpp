// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/core/safety.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for CRC-24Q generation and verification. [LSIS-AFS-467] SEU-hardened with
 * high Hamming distance bit patterns. */
enum class CrcStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kNullInput = 0xCCU,      ///< Required buffer is null or empty.
  kOutputTooSmall = 0x33U, ///< Output buffer too small for result.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// CRC-24Q output length in bits.
inline constexpr uint8_t kCrc24Bits = 24;

// Note: LSIS-FID0-xxx tags reference the LSIS V1.0 "FID 0" requirement numbering
// (data-link layer). LSIS-AFS-xxx tags reference AFS signal-layer requirements.
// Both formats are correct per the spec.

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
[[nodiscard("CrcStatus reports null inputs or output-buffer sizing failures; "
            "ignoring it leaves out_crc indeterminate and propagates an invalid "
            "checksum into the frame (LSIS-FID0-467)")]] CrcStatus
crc24_calculate(std::span<const uint8_t> in, std::span<uint8_t, 24ULL> out_crc) noexcept;

/**
 * @brief Hardened CRC verification. [LSIS-FID0-467]
 * @return true if CRC matches.
 */
[[nodiscard("crc24_verify returns true only when the trailing CRC-24Q matches the "
            "preceding data; discarding the result accepts unverified frames as valid "
            "— safety-critical CRC gate, do not discard")]] bool
crc24_verify(std::span<const uint8_t> data_with_crc) noexcept;

} // namespace lunalink::signal
