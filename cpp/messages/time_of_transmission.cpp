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

[[nodiscard]] TotResult compute_tot(const uint16_t wn,
                                    const uint16_t itow,
                                    const uint8_t toi,
                                    const double delta_t_lrt) noexcept {
    // [LSIS-AFS-810] Time of Transmission computation.
    TotResult result{};
    TotStatus status = TotStatus::kFaultDetected; // Pessimistic default
    volatile uint32_t cfi_count = kCfiMagicTot;

    // Input validation
    bool ok = (wn <= kWnMax);
    if (!ok) {
        status = TotStatus::kInvalidWn;
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        ok = (itow <= kItowMax);
        if (!ok) {
            status = TotStatus::kInvalidItow;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        ok = (toi <= kTotToiMax);
        if (!ok) {
            status = TotStatus::kInvalidToi;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        ok = std::isfinite(delta_t_lrt);
        if (!ok) {
            status = TotStatus::kInvalidDelta;
        }
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
            status = TotStatus::kFaultDetected; // FIZ-1
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
