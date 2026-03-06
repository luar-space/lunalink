#pragma once
#include <cstdint>

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

enum class IqMuxStatus : uint8_t {
  kOk = 0,
  kNullInput,
  kInvalidISample,
  kInvalidQSample,
};

/// Upsample factor: AFS-I chip rate x 5 = AFS-Q chip rate (LSIS-140, Table 7).
inline constexpr uint8_t kIqUpsampleFactor = 5;

/// Number of IQ samples per 2 ms epoch at the Q chip rate (5.115 MSPS).
inline constexpr uint16_t kIqSamplesPerEpoch = kWeil10230ChipLength; // 10230

/// Multiplex AFS-I and AFS-Q into baseband IQ at the Q chip rate (5.115 MSPS).
///
/// AFS-I samples (kGoldChipLength = 2046) are upsampled 5x by sample-and-hold
/// to match the AFS-Q rate (kWeil10230ChipLength = 10230), per LSIS-140 Table 7:
///   I chip rate = 1.023 Mcps, Q chip rate = 5.115 Mcps, ratio = 5.
///
/// Both channels are emitted at equal normalized digital amplitude.
/// Absolute power scaling per LSIS-103/LSIS-130 is expected downstream.
/// Output is interleaved [I0, Q0, I1, Q1, ...] as int16_t.
///
/// Parameters:
///   i_samples – AFS-I modulated samples {-1, +1}, length = kGoldChipLength
///   q_samples – AFS-Q modulated samples {-1, +1}, length = kWeil10230ChipLength
///   out       – caller-allocated interleaved IQ, length >= 2 * kIqSamplesPerEpoch
///
/// Returns explicit status on input validation or processing errors.
[[nodiscard]] IqMuxStatus multiplex_iq(
    const int8_t* i_samples,
    const int8_t* q_samples,
    int16_t*      out) noexcept;

} // namespace lunalink::signal
