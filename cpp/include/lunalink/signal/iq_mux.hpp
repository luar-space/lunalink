#pragma once
#include <cstdint>
#include <span>

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

enum class IqMuxStatus : uint8_t {
  kOk = 0,
  kInputTooSmall,
  kOutputTooSmall,
  kInvalidISample,
  kInvalidQSample,
  kNullInput,
};

/// Samples per 2 ms AFS-I chip: 1.
inline constexpr uint8_t kSamplesPerIChip = 1;

/// Samples per 2 ms AFS-Q chip: 1 (native).
inline constexpr uint8_t kSamplesPerQChip = 1;

/// Upsample factor for I to match Q sample rate: 1.023 / 5.115 = 1/5.
inline constexpr uint8_t kIqUpsampleFactor = 5;

/// Total IQ samples per 2 ms epoch: 10230 Q chips.
inline constexpr uint16_t kIqSamplesPerEpoch = kWeil10230ChipLength;

/**
 * @brief Multiplex AFS-I and AFS-Q into interleaved baseband IQ samples.
 *
 * Implements sample-and-hold upsampling for I channel by factor 5.
 * Output is interleaved [I0, Q0, I1, Q1, ...].
 *
 * @param i_samples Input AFS-I symbols in {-1, 1} (2046 samples)
 * @param q_samples Input AFS-Q chips in {-1, 1} (10230 samples)
 * @param out       Output interleaved samples (2 * 10230 = 20460 samples)
 * @return IqMuxStatus::kOk or error
 */
[[nodiscard]] IqMuxStatus multiplex_iq(
    std::span<const int8_t> i_samples,
    std::span<const int8_t> q_samples,
    std::span<int16_t>      out) noexcept;

/**
 * @brief Ergonomic overloads.
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
