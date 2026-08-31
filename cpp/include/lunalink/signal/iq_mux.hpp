// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/codes/prn.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for AFS-I/Q IQ multiplexing at 5.115 MSPS. [LSIS-AFS-301] SEU-hardened with
 * high Hamming distance bit patterns. */
enum class IqMuxStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kInputTooSmall = 0xA5U,  ///< Input buffer smaller than required.
  kOutputTooSmall = 0x33U, ///< Output buffer too small for result.
  kInvalidISample = 0xCCU, ///< I-channel sample not in {-1, +1}.
  kInvalidQSample = 0x0FU, ///< Q-channel sample not in {-1, +1}.
  kNullInput = 0xF0U,      ///< Required buffer is null or empty.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// Samples per 2 ms AFS-I chip: 1.
inline constexpr uint8_t kSamplesPerIChip = 1;

/// Samples per 2 ms AFS-Q chip: 5.
inline constexpr uint8_t kSamplesPerQChip = 5;

/// Upsample factor from I to Q (pilot rate).
inline constexpr uint8_t kIqUpsampleFactor = 5;

/// Total Q-rate samples per 2ms epoch.
inline constexpr uint16_t kIqSamplesPerEpoch = 10230;

/**
 * @brief Multiplex I and Q channels into a single interleaved sequence. [LSIS-AFS-301]
 * Interleaving pattern: [I0, Q0, I1, Q1, ...].
 * The I channel is upsampled 5x to match the Q channel rate.
 *
 * @param i_samples  In-phase chips (min 2046).
 * @param q_samples  Quadrature chips (min 10230).
 * @param out        Interleaved output (min 20460 samples).
 *
 * @pre i_samples.size() >= 2046
 * @pre q_samples.size() >= 10230
 * @pre out.size() >= 20460
 * @return IqMuxStatus::kOk or an error code.
 */
[[nodiscard("IqMuxStatus reports kInvalidISample/kInvalidQSample (out-of-domain {-1,+1}), "
            "kInputTooSmall/kOutputTooSmall, and CFI/TMR faults; ignoring it allows "
            "malformed interleaved I/Q to reach the RF stage (LSIS-AFS-301, FIZ-1)")]] IqMuxStatus
multiplex_iq(std::span<const int8_t> i_samples,
             std::span<const int8_t> q_samples,
             std::span<int16_t> out) noexcept;

/**
 * @brief Ergonomic template overload for multiplex_iq.
 */
template <typename T, typename U, typename V>
[[nodiscard("IqMuxStatus reports invalid sample domains, buffer-size violations, and "
            "CFI/TMR faults; ignoring it allows malformed interleaved I/Q to reach the "
            "RF stage (LSIS-AFS-301, FIZ-1)")]] inline IqMuxStatus
multiplex_iq(T&& i_samples, U&& q_samples, V&& out) noexcept {
  return multiplex_iq(std::span<const int8_t>(i_samples),
                      std::span<const int8_t>(q_samples),
                      std::span<int16_t>(out));
}

} // namespace lunalink::signal
