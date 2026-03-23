#pragma once
#include <cstddef>
#include <cstdint>

namespace lunalink::core {

// ─── Constants ──────────────────────────────────────────────────────────────

/// Speed of light in m/s (IAU 2012 exact value).
inline constexpr double kSpeedOfLightMps = 299792458.0;

/// SISE position error limit in metres (95th percentile over 24 h).
/// LSIS V1.0 Table 1: ≤ 40 {LSIS-TBC-2001} m over 24 {LSIS-TBC-2002} h.
inline constexpr double kSisePositionLimitM = 40.0;

/// SISE velocity error limit in m/s (95th percentile over 24 h).
/// LSIS V1.0 Table 2: ≤ 1 {LSIS-TBC-2003} cm/s over 24 {LSIS-TBC-2004} h.
inline constexpr double kSiseVelocityLimitMps = 0.01;

// ─── Types ──────────────────────────────────────────────────────────────────

/// SISE state vector: 3-D spatial + clock bias/drift (4 components).
///
/// For position: x, y, z in metres; ct = clock bias × c in metres.
/// For velocity: x, y, z in m/s;    ct = clock drift × c in m/s.
///
/// The clock term is pre-scaled by the speed of light so all components
/// have the same units, matching LSIS V1.0 §2.2.2 Equations 1 and 2.
struct SiseVec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double ct = 0.0; ///< Clock bias (pos) or drift (vel), scaled by c.
};

// ─── Status ─────────────────────────────────────────────────────────────────

/** @brief Status codes for SISE calculations.
 * SEU-hardened with high Hamming distance bit patterns. */
enum class SiseStatus : uint8_t {
    kOk           = 0x5AU, ///< Calculation completed successfully.
    kInvalidInput = 0xA5U, ///< Input contains NaN/Inf or null pointer.
    kEmptyData    = 0x33U, ///< Percentile called with n == 0.
    kFaultDetected = 0x99U, ///< Integrity check failed.
};

// ─── Single-Epoch Result ────────────────────────────────────────────────────

/// Result of a single-epoch SISE computation.
struct SiseResult {
    SiseStatus status = SiseStatus::kFaultDetected;
    double position_error_m = 0.0;   ///< 4-D RSS position+clock error (m).
    double velocity_error_mps = 0.0; ///< 4-D RSS velocity+clock-drift error (m/s).
};

/// Compute single-epoch SISE per LSIS V1.0 §2.2.2. [LSIS-SISE]
///
/// Equation 1 (position):
///   SISE_pos = sqrt((x-x̃)² + (y-ỹ)² + (z-z̃)² + (ct-ct̃)²)
///
/// Equation 2 (velocity):
///   SISE_vel = sqrt((ẋ-ẋ̃)² + (ẏ-ỹ̇)² + (ż-z̃̇)² + (cṫ-ct̃̇)²)
///
/// Where x,y,z,t are true state and x̃,ỹ,z̃,t̃ are broadcast state.
/// Clock terms are pre-scaled by c (speed of light) so all components
/// are in metres (position) or m/s (velocity).
///
/// @param pos_true       True position+clock state (metres).
/// @param pos_broadcast  Broadcast position+clock state (metres).
/// @param vel_true       True velocity+clock-drift state (m/s).
/// @param vel_broadcast  Broadcast velocity+clock-drift state (m/s).
/// @return SiseResult with status and error magnitudes.
[[nodiscard]] SiseResult compute_sise(SiseVec pos_true, SiseVec pos_broadcast,
                                      SiseVec vel_true, SiseVec vel_broadcast) noexcept;

// ─── Percentile Result ──────────────────────────────────────────────────────

/// Result of a percentile SISE computation over N epochs.
struct SisePercentileResult {
    SiseStatus status = SiseStatus::kFaultDetected;
    double position_percentile_m = 0.0;   ///< Position error at requested percentile.
    double velocity_percentile_mps = 0.0; ///< Velocity error at requested percentile.
    bool position_pass = false; ///< True if below kSisePositionLimitM.
    bool velocity_pass = false; ///< True if below kSiseVelocityLimitMps.
};

/// Compute percentile SISE over N epochs. [LSIS-SISE]
///
/// Uses partial sort (nth_element) to find the requested percentile.
/// Index = ceil(p * n) - 1, clamped to [0, n-1].
///
/// @param pos_errors_m   Array of N position errors in metres.
/// @param vel_errors_mps Array of N velocity errors in m/s.
/// @param n              Number of samples (must be > 0).
/// @param percentile     Percentile to compute (0.0..1.0, default 0.95).
/// @return SisePercentileResult with status, values, and pass/fail flags.
[[nodiscard]] SisePercentileResult compute_sise_percentile(
    const double* pos_errors_m, const double* vel_errors_mps,
    size_t n, double percentile = 0.95) noexcept;

} // namespace lunalink::core
