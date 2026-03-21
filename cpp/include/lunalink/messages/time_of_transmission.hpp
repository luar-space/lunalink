#pragma once
#include <cstdint>

namespace lunalink::signal {

// ─── Constants (LSIS-720) ────────────────────────────────────────────────────

/// Seconds in one week.
inline constexpr uint32_t kSecWeek = 604800U;

/// Block Interval duration in seconds (BI_d = F_d × 100 = 1200 s = 20 min).
inline constexpr uint16_t kBlockIntervalDuration = 1200U;

/// Frame duration in seconds.
inline constexpr uint8_t kFrameDurationSec = 12U;

/// Maximum TOI value (inclusive). 7 bits, 0..99.
inline constexpr uint8_t kTotToiMax = 99U;

/// Maximum ITOW value (inclusive). 9 bits, 0..503.
inline constexpr uint16_t kItowMax = 503U;

/// Maximum WN value (inclusive). 13 bits, 0..8191.
inline constexpr uint16_t kWnMax = 8191U;

// ─── Status ──────────────────────────────────────────────────────────────────

/** @brief Status codes for Time of Transmission calculation.
 * SEU-hardened with high Hamming distance bit patterns. */
enum class TotStatus : uint8_t {
    kOk = 0x5AU,            ///< Calculation completed successfully.
    kInvalidToi = 0xA5U,    ///< TOI value exceeds kTotToiMax (99).
    kInvalidItow = 0x33U,   ///< ITOW value exceeds kItowMax (503).
    kInvalidWn = 0xCCU,     ///< WN value exceeds kWnMax (8191).
    kInvalidDelta = 0x3CU,  ///< delta_t_lrt is NaN or Inf.
    kFaultDetected = 0x99U, ///< Integrity check failed.
};

// ─── Result ──────────────────────────────────────────────────────────────────

/// Time of Transmission computation result.
struct TotResult {
    TotStatus status = TotStatus::kFaultDetected;
    double t_f = 0.0; ///< Time since LRT epoch in seconds.
};

// ─── Function ────────────────────────────────────────────────────────────────

/// Compute Time of Transmission per LSIS-720. [LSIS-AFS-810]
///
/// Formula: t_F = WN × SECWEEK + ITOW × BI_d + TOI × F_d + Δt_LRT
///
/// Where:
///   - SECWEEK = 604800 s (seconds per week)
///   - BI_d = 1200 s (block interval duration = 20 min)
///   - F_d = 12 s (frame duration)
///   - Δt_LRT = Node Time offset with respect to LRT (from MSG-G4)
///
/// @param wn          Week Number (0..8191, 13 bits).
/// @param itow        Interval Time of Week (0..503, 9 bits).
/// @param toi         Time of Interval (0..99, 7 bits).
/// @param delta_t_lrt Node Time offset w.r.t. LRT in seconds (default: 0.0).
/// @return TotResult with status and computed time.
[[nodiscard]] TotResult compute_tot(uint16_t wn,
                                    uint16_t itow,
                                    uint8_t toi,
                                    double delta_t_lrt = 0.0) noexcept;

} // namespace lunalink::signal
