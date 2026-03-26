/// @file sise.cpp
/// @brief Signal-In-Space Error (SISE) per LSIS V1.0 §2.2.2. [LSIS-SISE]
///
/// Implements the 4-component SISE metric (3 spatial + clock):
///   Eq. 1: SISE_pos = sqrt((x-x̃)² + (y-ỹ)² + (z-z̃)² + (ct-ct̃)²)
///   Eq. 2: SISE_vel = sqrt((ẋ-ẋ̃)² + (ẏ-ỹ̇)² + (ż-z̃̇)² + (cṫ-ct̃̇)²)
///
/// Limits (LSIS V1.0 Tables 1-2, values TBC):
///   Position: ≤ 40 m (95th percentile over 24 h)
///   Velocity: ≤ 1 cm/s (95th percentile over 24 h)

#include "lunalink/core/sise.hpp"
#include "lunalink/core/safety.hpp"

#include <algorithm>
#include <cmath>

namespace lunalink::core {

using lunalink::signal::secure_scrub_scalar;

namespace {
/// CFI magic sentinel for SISE calculation.
constexpr uint32_t kCfiMagicSise = 0x53534953U; // "SSIS"

/// CFI magic sentinel for SISE percentile calculation.
constexpr uint32_t kCfiMagicPercentile = 0x50435449U; // "PCTI"

/// Validate that a SiseVec contains finite values (all 4 components).
[[nodiscard]] bool is_finite(SiseVec v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) &&
           std::isfinite(v.z) && std::isfinite(v.ct);
}

/// Compute 4-D RSS of the difference between two SiseVec.
/// LSIS V1.0 §2.2.2 Equations 1 and 2.
[[nodiscard]] double rss_diff(SiseVec a, SiseVec b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    const double dct = a.ct - b.ct;
    return std::sqrt(dx * dx + dy * dy + dz * dz + dct * dct);
}

/// Validate percentile inputs. Returns kOk if valid; first failing status otherwise.
[[nodiscard]] SiseStatus validate_percentile_inputs(
    std::span<const double> pos_errors_m, std::span<const double> vel_errors_mps,
    double percentile) noexcept {
    if (pos_errors_m.size() != vel_errors_mps.size()) {
        return SiseStatus::kInvalidInput;
    }
    if (pos_errors_m.empty()) {
        return SiseStatus::kEmptyData;
    }
    if (!std::isfinite(percentile) || percentile < 0.0 || percentile > 1.0) {
        return SiseStatus::kInvalidInput;
    }
    for (size_t i = 0; i < pos_errors_m.size(); ++i) {
        if (!std::isfinite(pos_errors_m[i]) || !std::isfinite(vel_errors_mps[i])) {
            return SiseStatus::kInvalidInput;
        }
    }
    return SiseStatus::kOk;
}

/// Compute the value at a given percentile index using nth_element in-place.
/// The span is mutated (reordered) — caller must provide scratch data.
[[nodiscard]] double nth_percentile_inplace(std::span<double> data, size_t idx) noexcept {
    std::nth_element(data.begin(),
                     data.begin() + static_cast<ptrdiff_t>(idx),
                     data.end());
    return data[idx];
}

/// Compute percentile index: ceil(p * n) - 1, clamped to [0, n-1].
[[nodiscard]] size_t percentile_index(double p, size_t n) noexcept {
    const auto raw = static_cast<size_t>(std::ceil(p * static_cast<double>(n)));
    const size_t idx = (raw > 0U) ? (raw - 1U) : 0U;
    return (idx < n) ? idx : (n - 1U);
}
} // namespace

[[nodiscard]] SiseResult compute_sise(const SiseVec pos_true, const SiseVec pos_broadcast,
                                      const SiseVec vel_true, const SiseVec vel_broadcast) noexcept {
    // [LSIS-SISE] Single-epoch 4-D SISE computation per LSIS V1.0 §2.2.2.
    SiseResult result{};
    SiseStatus status = SiseStatus::kFaultDetected; // Pessimistic default
    volatile uint32_t cfi_count = kCfiMagicSise;

    // Input validation: all 4 components of all 4 vectors must be finite.
    const bool inputs_ok = is_finite(pos_true) && is_finite(pos_broadcast) &&
                           is_finite(vel_true) && is_finite(vel_broadcast);
    bool ok = inputs_ok;
    if (!ok) {
        status = SiseStatus::kInvalidInput;
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U; // Milestone 1: inputs validated
        LUNALINK_CRITICAL_EXIT;
    }

    // Compute 4-D RSS errors (Equations 1 and 2).
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        result.position_error_m = rss_diff(pos_true, pos_broadcast);
        result.velocity_error_mps = rss_diff(vel_true, vel_broadcast);

        // Post-condition: results must be finite.
        if (!std::isfinite(result.position_error_m) ||
            !std::isfinite(result.velocity_error_mps)) {
            ok = false;                             // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): requires component differences near DBL_MAX (~1e308); cislunar coordinates bounded to ~5e8 m
            status = SiseStatus::kFaultDetected;    // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH)
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U; // Milestone 2: RSS computed
        LUNALINK_CRITICAL_EXIT;
    }

    // CFI terminal check: both milestones reached.
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        if (cfi_count == kCfiMagicSise + 2U) {
            status = SiseStatus::kOk;
        } else {
            status = SiseStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI sentinel failure path
        }
    }

    // Scrub output on failure.
    if (status != SiseStatus::kOk) {
        secure_scrub_scalar(result.position_error_m);
        secure_scrub_scalar(result.velocity_error_mps);
    }

    result.status = status;
    return result;
}

[[nodiscard]] SisePercentileResult compute_sise_percentile(
    std::span<double> pos_errors_m, std::span<double> vel_errors_mps,
    double percentile) noexcept {
    // [LSIS-SISE] Percentile computation over N epochs.
    // Input spans are mutable: nth_element operates in-place, avoiding
    // std::vector (forbidden in flight code). Caller provides scratch copies.
    SisePercentileResult result{};
    SiseStatus status = SiseStatus::kFaultDetected; // Pessimistic default
    volatile uint32_t cfi_count = kCfiMagicPercentile;

    // Input validation.
    const auto input_status = validate_percentile_inputs(
        pos_errors_m, vel_errors_mps, percentile);
    bool ok = (input_status == SiseStatus::kOk);
    if (!ok) {
        status = input_status;
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U; // Milestone 1: inputs validated
        LUNALINK_CRITICAL_EXIT;
    }

    // Compute percentile via in-place partial sort (no heap allocation).
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        const size_t n = pos_errors_m.size();
        const size_t idx = percentile_index(percentile, n);
        result.position_percentile_m = nth_percentile_inplace(pos_errors_m, idx);
        result.velocity_percentile_mps = nth_percentile_inplace(vel_errors_mps, idx);
        result.position_pass = (result.position_percentile_m <= kSisePositionLimitM);
        result.velocity_pass = (result.velocity_percentile_mps <= kSiseVelocityLimitMps);

        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U; // Milestone 2: percentile computed
        LUNALINK_CRITICAL_EXIT;
    }

    // CFI terminal check.
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (ok) {
        if (cfi_count == kCfiMagicPercentile + 2U) {
            status = SiseStatus::kOk;
        } else {
            status = SiseStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI sentinel failure path
        }
    }

    // Scrub output on failure.
    if (status != SiseStatus::kOk) {
        secure_scrub_scalar(result.position_percentile_m);
        secure_scrub_scalar(result.velocity_percentile_mps);
        result.position_pass = false;
        result.velocity_pass = false;
    }

    result.status = status;
    return result;
}

} // namespace lunalink::core
