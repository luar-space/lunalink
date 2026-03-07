#pragma once
#include <cstdint>
#include <span>

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for IQ Multiplexer operations.
 * Non-contiguous bit patterns with high Hamming distance resist SEU bit-flips.
 */
enum class IqMuxStatus : uint8_t {
  kOk             = 0x5AU,  // 01011010
  kInputTooSmall  = 0xA5U,  // 10100101
  kOutputTooSmall = 0x33U,  // 00110011
  kInvalidISample = 0xCCU,  // 11001100
  kInvalidQSample = 0x0FU,  // 00001111
  kNullInput      = 0xF0U,  // 11110000
  kFaultDetected  = 0x99U,  // 10011001
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
[[nodiscard]] IqMuxStatus multiplex_iq(
    std::span<const int8_t> i_samples,
    std::span<const int8_t> q_samples,
    std::span<int16_t>      out) noexcept;

/**
 * @brief Ergonomic template overload for multiplex_iq.
 */
template <typename T, typename U, typename V>
[[nodiscard]] inline IqMuxStatus multiplex_iq(
    T&& i_samples,
    U&& q_samples,
    V&& out) noexcept {
  return multiplex_iq(
      std::span<const int8_t>(i_samples),
      std::span<const int8_t>(q_samples),
      std::span<int16_t>(out));
}

} // namespace lunalink::signal
