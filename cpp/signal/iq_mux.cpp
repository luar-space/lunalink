#include "lunalink/signal/iq_mux.hpp"

#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include <algorithm> // for std::fill

namespace lunalink::signal {

/**
 * @brief Multiplex I and Q channels. [LSIS-AFS-301]
 */
IqMuxStatus multiplex_iq(
    std::span<const int8_t> i_samples,
    std::span<const int8_t> q_samples,
    std::span<int16_t>      out) noexcept {
  
  IqMuxStatus status = IqMuxStatus::kOk;

  // ESA/JAXA Safety Policy: Pre-initialise buffer.
  if (!out.empty()) {
    secure_scrub(out);
  }

  if (i_samples.size() < kGoldChipLength || q_samples.size() < kWeil10230ChipLength) [[unlikely]] {
    status = IqMuxStatus::kInputTooSmall;
  } else if (out.size() < static_cast<size_t>(2U * kIqSamplesPerEpoch)) [[unlikely]] {
    status = IqMuxStatus::kOutputTooSmall;
  }

  if (status == IqMuxStatus::kOk) {
    // JAXA Safety: Aliasing Protection — runtime guards (assert() disabled in release builds).
    if (spans_overlap(static_cast<const void*>(i_samples.data()), i_samples.size_bytes(),
                      static_cast<const void*>(out.data()), out.size_bytes()) ||
        spans_overlap(static_cast<const void*>(q_samples.data()), q_samples.size_bytes(),
                      static_cast<const void*>(out.data()), out.size_bytes())) [[unlikely]] {
      secure_scrub(out);
      return IqMuxStatus::kFaultDetected;  // FIZ-1
    }

    uint32_t loop_count = 0;
    for (uint32_t n = 0; n < kIqSamplesPerEpoch; ++n) {
      // Upsample AFS-I: each I chip spans 5 Q-rate samples (sample-and-hold).
      const auto i_raw = i_samples[n / kIqUpsampleFactor];
      const auto q_raw = q_samples[n];

      // Validate samples are in {-1, 1} on the fly.
      // Branchless check for value in {-1, 1}: x^2 == 1.
      // NOLINTBEGIN(bugprone-signed-char-misuse)
      if (static_cast<int>(i_raw) * static_cast<int>(i_raw) != 1) [[unlikely]] {
          status = IqMuxStatus::kInvalidISample;
          break;
      }
      if (static_cast<int>(q_raw) * static_cast<int>(q_raw) != 1) [[unlikely]] {
          status = IqMuxStatus::kInvalidQSample;
          break;
      }

      const auto i_val = static_cast<int16_t>(i_raw);
      const auto q_val = static_cast<int16_t>(q_raw);
      // NOLINTEND(bugprone-signed-char-misuse)

      // Interleaved IQ output: [I0, Q0, I1, Q1, ...]
      const auto idx = n << 1U;
      out[idx] = i_val;
      out[idx + 1U] = q_val;
      loop_count++;
      wip_tick();
    }

    // CFI: Terminal count verification.
    if (status == IqMuxStatus::kOk && loop_count != kIqSamplesPerEpoch) [[unlikely]] {
        status = IqMuxStatus::kFaultDetected;  // FIZ-1
    }

    if (status != IqMuxStatus::kOk) [[unlikely]] {
        // Clear partially written output on failure.
        secure_scrub(out);
    }
  }
  return status;
}

} // namespace lunalink::signal
