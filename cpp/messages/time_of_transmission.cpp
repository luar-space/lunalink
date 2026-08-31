/// @file time_of_transmission.cpp
/// @brief Time of Transmission calculator per LSIS-720. [LSIS-AFS-810]
///
/// Implements the ToT formula:
///   t_F = WN × SECWEEK + ITOW × BI_d + TOI × F_d + Δt_LRT

#include "lunalink/messages/time_of_transmission.hpp"

#include "lunalink/core/safety.hpp"

#include <cmath>

namespace lunalink::signal {

namespace {
/// CFI magic sentinel for ToT calculation.
constexpr uint32_t kCfiMagicTot = 0x4D4D4D4DU;
} // namespace

// Validate ToT inputs. Returns kOk if all pass; otherwise the first failing status.
[[nodiscard]] TotStatus
validate_tot_inputs(uint16_t wn, uint16_t itow, uint8_t toi, double delta_t_lrt) noexcept {
  if (wn > kWnMax) {
    return TotStatus::kInvalidWn;
  }
  if (itow > kItowMax) {
    return TotStatus::kInvalidItow;
  }
  if (toi > kTotToiMax) {
    return TotStatus::kInvalidToi;
  }
  if (!std::isfinite(delta_t_lrt)) {
    return TotStatus::kInvalidDelta;
  }
  return TotStatus::kOk;
}

[[nodiscard]] TotResult
compute_tot(uint16_t wn, uint16_t itow, uint8_t toi, double delta_t_lrt) noexcept {
  // [LSIS-AFS-810] Time of Transmission computation.
  TotResult result{};
  TotStatus status = TotStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicTot;

  // Input validation
  const auto input_status = validate_tot_inputs(wn, itow, toi, delta_t_lrt);
  bool ok = (input_status == TotStatus::kOk);
  if (!ok) {
    status = input_status;
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: inputs validated
    LUNALINK_CRITICAL_EXIT;
  }

  // t_F = WN × SECWEEK + ITOW × BI_d + TOI × F_d + Δt_LRT
  // Use double to avoid overflow: WN=8191 × 604800 = ~4.95e9 (exceeds uint32_t)
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    const auto t_f = static_cast<double>(wn) * static_cast<double>(kSecWeek) +
                     static_cast<double>(itow) * static_cast<double>(kBlockIntervalDuration) +
                     static_cast<double>(toi) * static_cast<double>(kFrameDurationSec) +
                     delta_t_lrt;
    result.t_f = t_f;

    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: formula computed
    LUNALINK_CRITICAL_EXIT;
  }

  // CFI terminal check: both milestones must have been reached.
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    if (cfi_count == kCfiMagicTot + 2U) {
      status = TotStatus::kOk;
    } else {
      status = TotStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI sentinel
                                          // failure path — volatile counter always matches under
                                          // nominal execution
    }
  }

  // Scrub output on failure
  if (status != TotStatus::kOk) {
    secure_scrub_scalar(result.t_f);
  }

  result.status = status;
  return result;
}

} // namespace lunalink::signal
