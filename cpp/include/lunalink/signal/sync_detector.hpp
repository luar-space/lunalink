#pragma once
#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"
#include "lunalink/messages/frame.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace lunalink::signal {

// ─── Constants ───────────────────────────────────────────────────────────────

/// Detection threshold for normalized cross-correlation peak.
/// Sync pattern is 68 symbols; perfect correlation = 68.
/// Threshold of 50 gives ~11 dB margin for noise robustness.
inline constexpr float kDefaultSyncThreshold = 50.0F;

/** @brief Status codes for 68-symbol sync pattern detection via cross-correlation. [LSIS-AFS-710]
 * SEU-hardened with high Hamming distance bit patterns. */
enum class SyncDetectorStatus : uint8_t {
  kOk = 0x5AU,            ///< Operation completed successfully.
  kInvalidInput = 0xA5U,  ///< Input parameters out of range or invalid.
  kNotDetected = 0x33U,   ///< Peak correlation below detection threshold.
  kFaultDetected = 0xCCU, ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

// ─── Sync Detection Result ──────────────────────────────────────────────────

struct SyncResult {
  uint16_t frame_offset; ///< Symbol index where sync pattern starts.
  float peak_metric;     ///< Peak cross-correlation value (max = 68.0).
  float second_peak; ///< Second-highest peak (for discrimination); 0 if only one offset searched.
  bool detected;     ///< True if peak_metric >= threshold.
};

// ─── Sync Pattern Detector ──────────────────────────────────────────────────

/// Cross-correlation-based sync pattern detector for AFS navigation frames.
/// Searches a buffer of received soft symbols for the 68-symbol sync pattern.
///
/// Input symbols are bipolar soft-decision values: positive = bit 0,
/// negative = bit 1, magnitude = confidence. The detector computes
/// a sliding dot-product with the bipolar-mapped sync pattern.
///
/// @note Not thread-safe. Caller must ensure exclusive access.
/// @note All storage is inline (no heap allocation).
class SyncDetector {
public:
  /// Construct with a telemetry HAL for fault reporting.
  explicit SyncDetector(ITelemetryHal& hal) noexcept;

  /// Set the detection threshold. Default: kDefaultSyncThreshold.
  /// Returns false if threshold is non-finite or non-positive.
  [[nodiscard]] bool set_threshold(float threshold) noexcept;

  /// Detect the sync pattern in a buffer of soft symbols. [LSIS-AFS-710]
  ///
  /// Computes cross-correlation at every valid offset in the input buffer
  /// and returns the peak location and metric.
  ///
  /// @param rx_symbols  Received soft symbols (bipolar: +1.0 = bit 0, -1.0 = bit 1).
  ///                    Must contain [kSyncLength, UINT16_MAX + kSyncLength] symbols.
  /// @param result      Output: detection result with frame offset and metrics.
  /// @return SyncDetectorStatus::kOk if sync detected, kNotDetected if below
  ///         threshold, or kInvalidInput/kFaultDetected on error.
  [[nodiscard]] SyncDetectorStatus detect(std::span<const float> rx_symbols,
                                          SyncResult& result) noexcept;

  /// Inject an SEU on the CFI counter register after milestones complete.
  /// The terminal check sees a short count equivalent to a branch skip,
  /// causing detect() to fire kFaultDetected. [SWIFI]
#ifdef LUNALINK_FAULT_INJECTION
  void inject_cfi_skip(uint32_t n) noexcept {
    cfi_skip_inject_ = n;
  }

  /// Inject a single-copy fault into the peak offset TMR.
  /// Call after detect() has populated the TMR, then call
  /// voted_peak_offset() to verify majority vote repairs. [SWIFI]
  void inject_tmr_fault(uint8_t copy, uint16_t val) noexcept {
    peak_offset_tmr_.inject_fault(copy, val);
  }

  /// Read the TMR-voted peak offset without running detect().
  /// Used by SWIFI tests to verify majority vote after injection.
  [[nodiscard]] uint16_t voted_peak_offset() const noexcept {
    return peak_offset_tmr_.vote();
  }
#endif

private:
  ITelemetryHal& hal_;
  float threshold_;

  /// Bipolar sync pattern reference (+1/-1), precomputed from kSyncPattern.
  std::array<float, kSyncLength> sync_ref_{};

  /// TMR-protected peak offset — persists across calls to enable SWIFI
  /// injection between detect() invocations.  Reset at start of each detect().
  TmrValue<uint16_t> peak_offset_tmr_{};

#ifdef LUNALINK_FAULT_INJECTION
  uint32_t cfi_skip_inject_ = 0U;
#endif
};

} // namespace lunalink::signal
