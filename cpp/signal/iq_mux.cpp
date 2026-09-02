// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/signal/iq_mux.hpp"

#include "lunalink/codes/prn_codes.hpp"
#include "lunalink/core/safety.hpp"

namespace lunalink::signal {

namespace {

// Validate IQ mux input sizes and aliasing.
[[nodiscard]] IqMuxStatus validate_iq_inputs(std::span<const int8_t> i_samples,
                                             std::span<const int8_t> q_samples,
                                             std::span<int16_t> out) noexcept {
  if (i_samples.size() < kGoldChipLength || q_samples.size() < kWeil10230ChipLength) [[unlikely]] {
    return IqMuxStatus::kInputTooSmall;
  }
  if (out.size() < static_cast<size_t>(2U * kIqSamplesPerEpoch)) [[unlikely]] {
    return IqMuxStatus::kOutputTooSmall;
  }
  if (spans_overlap(static_cast<const void*>(i_samples.data()),
                    i_samples.size_bytes(),
                    static_cast<const void*>(out.data()),
                    out.size_bytes()) ||
      spans_overlap(static_cast<const void*>(q_samples.data()),
                    q_samples.size_bytes(),
                    static_cast<const void*>(out.data()),
                    out.size_bytes())) [[unlikely]] {
    return IqMuxStatus::kFaultDetected; // FIZ-1
  }
  return IqMuxStatus::kOk;
}

// Validate that an IQ sample pair is in {-1, 1}.
// NOLINTBEGIN(bugprone-signed-char-misuse) -- int8_t samples are +/-1 BPSK symbols, signedness
// intentional
[[nodiscard]] IqMuxStatus validate_iq_sample(int8_t i_raw, int8_t q_raw) noexcept {
  if (static_cast<int>(i_raw) * static_cast<int>(i_raw) != 1) [[unlikely]] {
    return IqMuxStatus::kInvalidISample;
  }
  if (static_cast<int>(q_raw) * static_cast<int>(q_raw) != 1) [[unlikely]] {
    return IqMuxStatus::kInvalidQSample;
  }
  return IqMuxStatus::kOk;
}
// NOLINTEND(bugprone-signed-char-misuse)

} // namespace

/**
 * @brief Multiplex I and Q channels. [LSIS-AFS-301]
 */
IqMuxStatus multiplex_iq(std::span<const int8_t> i_samples,
                         std::span<const int8_t> q_samples,
                         std::span<int16_t> out) noexcept {
  IqMuxStatus status = validate_iq_inputs(i_samples, q_samples, out);

  if (status == IqMuxStatus::kOk) {
    volatile uint32_t loop_count = 0;
    for (uint32_t n = 0; n < kIqSamplesPerEpoch; ++n) {
      const auto i_raw = i_samples[n / kIqUpsampleFactor];
      const auto q_raw = q_samples[n];

      status = validate_iq_sample(i_raw, q_raw);
      if (status != IqMuxStatus::kOk) [[unlikely]] {
        break;
      }

      // NOLINTBEGIN(bugprone-signed-char-misuse) -- int8_t to int16_t widening of BPSK +/-1 symbols
      const auto idx = n << 1U;
      out[idx] = static_cast<int16_t>(i_raw);
      out[idx + 1U] = static_cast<int16_t>(q_raw);
      // NOLINTEND(bugprone-signed-char-misuse)
      LUNALINK_CRITICAL_ENTER;
      loop_count = loop_count + 1U;
      LUNALINK_CRITICAL_EXIT;
      wip_tick();
    }

    // CFI: Terminal count verification.
    if (status == IqMuxStatus::kOk && loop_count != kIqSamplesPerEpoch)
        [[unlikely]] {                      // NOCOVER_BEGIN(DEFENCE_IN_DEPTH)
      status = IqMuxStatus::kFaultDetected; // FIZ-1
    } // NOCOVER_END

    if (status != IqMuxStatus::kOk) [[unlikely]] {
      secure_scrub(out);
    }
  }
  return status;
}

} // namespace lunalink::signal
