// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/core/safety.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for 60x98 block interleave/deinterleave operations. [LSIS-AFS-470]
 * SEU-hardened with high Hamming distance bit patterns. */
enum class InterleaverStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kInputTooSmall = 0xCCU,  ///< Input buffer smaller than required.
  kOutputTooSmall = 0x33U, ///< Output buffer too small for result.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// Number of rows in the block interleaver matrix (LSIS Table 17).
inline constexpr uint16_t kInterleaverRows = 60;
/// Number of columns in the block interleaver matrix (LSIS Table 17).
inline constexpr uint16_t kInterleaverCols = 98;
/// Total interleaver block size: rows x cols = 5880 symbols.
inline constexpr uint16_t kInterleaverSize = kInterleaverRows * kInterleaverCols; // 5880

// Note: LSIS-FID0-xxx tags reference the LSIS V1.0 "FID 0" requirement numbering
// (data-link layer). LSIS-AFS-xxx tags reference AFS signal-layer requirements.
// Both formats are correct per the spec.

/**
 * @brief 60x98 Block Interleaver. [LSIS-FID0-470]
 * Write row-wise, read column-wise.
 *
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard("InterleaverStatus reports buffer-size mismatches and CFI/TMR faults; "
            "ignoring it allows malformed interleaved symbols into the transmit chain "
            "(LSIS-FID0-470, FIZ-1/2/3)")]] InterleaverStatus
block_interleave(std::span<const uint8_t> in, std::span<uint8_t> out) noexcept;

/**
 * @brief 60x98 Block De-interleaver. [LSIS-FID0-470]
 * Write column-wise, read row-wise.
 *
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard("InterleaverStatus reports buffer-size mismatches and CFI/TMR faults; "
            "ignoring it lets corrupted symbol order propagate to the FEC decoder "
            "(LSIS-FID0-470, FIZ-1/2/3)")]] InterleaverStatus
block_deinterleave(std::span<const uint8_t> in, std::span<uint8_t> out) noexcept;

/**
 * @brief Soft-decision 60x98 Block De-interleaver. [LSIS-AFS-471]
 *
 * Identical permutation to block_deinterleave() but operates on float
 * soft-decision values (LLRs) to preserve soft information for FEC decoding.
 * Hardened with CFI sentinels and mirrored index calculation.
 */
[[nodiscard("InterleaverStatus reports buffer-size mismatches and CFI/TMR faults; "
            "ignoring it lets corrupted LLR order propagate to the soft FEC decoder "
            "(LSIS-AFS-471, FIZ-1/2/3)")]] InterleaverStatus
block_deinterleave_soft(std::span<const float> in, std::span<float> out) noexcept;

} // namespace lunalink::signal
