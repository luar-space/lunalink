#include "lunalink/signal/iq_mux.hpp"

#include "lunalink/signal/prn.hpp"
#include <algorithm> // for std::fill
#include <cassert>

namespace lunalink::signal {

IqMuxStatus multiplex_iq(
    std::span<const int8_t> i_samples,
    std::span<const int8_t> q_samples,
    std::span<int16_t>      out) noexcept {
  
  // ESA/JAXA Safety Policy: Pre-initialise buffer.
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), static_cast<int16_t>(0));
  }

  if (i_samples.size() < kGoldChipLength || q_samples.size() < kWeil10230ChipLength) [[unlikely]] {
    return IqMuxStatus::kInputTooSmall;
  }
  // NOLINTNEXTLINE(bugprone-implicit-widening-of-multiplication-result)
  if (out.size() < static_cast<size_t>(2U * kIqSamplesPerEpoch)) [[unlikely]] {
    return IqMuxStatus::kOutputTooSmall;
  }

  // JAXA Safety: Aliasing Protection.
  // We assert that the output buffer does not overlap with either input.
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  assert(static_cast<const void*>(out.data()) >= static_cast<const void*>(i_samples.data() + i_samples.size()) ||
         static_cast<const void*>(out.data() + out.size()) <= static_cast<const void*>(i_samples.data()));
  assert(static_cast<const void*>(out.data()) >= static_cast<const void*>(q_samples.data() + q_samples.size()) ||
         static_cast<const void*>(out.data() + out.size()) <= static_cast<const void*>(q_samples.data()));
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  for (uint32_t n = 0; n < kIqSamplesPerEpoch; ++n) {
    // Upsample AFS-I: each I chip spans 5 Q-rate samples (sample-and-hold).
    const auto i_raw = i_samples[n / kIqUpsampleFactor];
    const auto q_raw = q_samples[n];

    // Validate samples are in {-1, 1} on the fly.
    // Branchless check for value in {-1, 1}: x^2 == 1.
    // NOLINTBEGIN(bugprone-signed-char-misuse)
    if (static_cast<int>(i_raw) * static_cast<int>(i_raw) != 1) [[unlikely]] {
        // Clear partially written output on failure.
        std::fill(out.begin(), out.end(), static_cast<int16_t>(0));
        return IqMuxStatus::kInvalidISample;
    }
    if (static_cast<int>(q_raw) * static_cast<int>(q_raw) != 1) [[unlikely]] {
        std::fill(out.begin(), out.end(), static_cast<int16_t>(0));
        return IqMuxStatus::kInvalidQSample;
    }

    const auto i_val = static_cast<int16_t>(i_raw);
    const auto q_val = static_cast<int16_t>(q_raw);
    // NOLINTEND(bugprone-signed-char-misuse)

    // Interleaved IQ output: [I0, Q0, I1, Q1, ...]
    const auto idx = n << 1U;
    out[idx] = i_val;
    out[idx + 1U] = q_val;
  }
  return IqMuxStatus::kOk;
}

} // namespace lunalink::signal
