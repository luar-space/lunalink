#include "lunalink/signal/sync_detector.hpp"

#include "lunalink/core/safety.hpp" // LUNALINK_CRITICAL_ENTER/EXIT, TmrValue, wip_tick, secure_scrub

#include <algorithm>
#include <cmath>
#include <limits>

namespace lunalink::signal {

namespace {

// Cross-correlation search result.
struct SyncSearchResult {
  uint16_t best_offset = 0;
  float best_corr = -std::numeric_limits<float>::max();
  float second_best_corr = -std::numeric_limits<float>::max();
};

// Search all valid offsets for the best sync pattern cross-correlation.
[[nodiscard]] SyncSearchResult sync_correlate_search(std::span<const float> rx_symbols,
                                       std::span<const float> sync_ref,
                                       uint32_t search_len) noexcept {
  SyncSearchResult res{};
  for (uint32_t offset = 0; offset < search_len; ++offset) {
    float corr = 0.0F;
    for (uint16_t i = 0; i < kSyncLength; ++i) {
      corr += rx_symbols[static_cast<size_t>(offset) + i] * sync_ref[i];
    }
    if (corr > res.best_corr) {
      res.second_best_corr = res.best_corr;
      res.best_corr = corr;
      res.best_offset = static_cast<uint16_t>(offset);
    } else if (corr > res.second_best_corr) {
      res.second_best_corr = corr;
    }
    wip_tick();
  }
  return res;
}

} // namespace

// ─── Construction ────────────────────────────────────────────────────────────

SyncDetector::SyncDetector(ITelemetryHal& hal) noexcept
    : hal_(hal), threshold_(kDefaultSyncThreshold) {
  // Precompute bipolar reference: bit 0 → +1.0, bit 1 → −1.0.
  std::ranges::transform(
      kSyncPattern, sync_ref_.begin(), [](uint8_t b) noexcept { return (b == 0U) ? 1.0F : -1.0F; });
}

bool SyncDetector::set_threshold(float threshold) noexcept {
  if (!std::isfinite(threshold) || threshold <= 0.0F) {
    return false;
  }
  threshold_ = threshold;
  return true;
}

// ─── Sync Detection [LSIS-AFS-710] ──────────────────────────────────────────

[[nodiscard]] SyncDetectorStatus SyncDetector::detect(std::span<const float> rx_symbols,
                                                      SyncResult& result) noexcept {
  // [LSIS-AFS-710] CFI sentinel for sync detection (§5).
  constexpr uint32_t kCfiMagicSync = 0x7E7E7E7EU;

  SyncDetectorStatus status = SyncDetectorStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicSync;

  // Zero-initialise output (§7).
  result = SyncResult{};

  // ── Input validation ──
  // frame_offset is uint16_t: reject buffers that would overflow the offset.
  constexpr size_t kMaxSearchableSize = static_cast<size_t>(UINT16_MAX) +
                                        static_cast<size_t>(kSyncLength);
  if (rx_symbols.size() < static_cast<size_t>(kSyncLength) ||
      rx_symbols.size() > kMaxSearchableSize) {
    status = SyncDetectorStatus::kInvalidInput;
    return status;
  }

  LUNALINK_CRITICAL_ENTER;
  cfi_count = cfi_count + 1U; // milestone 1: input validated
  LUNALINK_CRITICAL_EXIT;

  // ── Cross-correlation search ──
  const auto search_len =
      static_cast<uint32_t>(rx_symbols.size() - static_cast<size_t>(kSyncLength) + 1U);

  const auto search = sync_correlate_search(rx_symbols, std::span(sync_ref_), search_len);

  // Update TMR with search result (member — enables SWIFI injection between calls).
  peak_offset_tmr_.refresh(search.best_offset);

  LUNALINK_CRITICAL_ENTER;
  cfi_count = cfi_count + 1U; // milestone 2: search complete
  LUNALINK_CRITICAL_EXIT;

  // ── Populate result ──
  const uint16_t best_offset = peak_offset_tmr_.vote();
  result.frame_offset = best_offset;
  result.peak_metric = search.best_corr;
  // When only one offset was searched, no second peak exists; report 0.
  result.second_peak = (search_len > 1U) ? search.second_best_corr : 0.0F;

  LUNALINK_CRITICAL_ENTER;
  cfi_count = cfi_count + 1U; // milestone 3: result populated
  LUNALINK_CRITICAL_EXIT;

#ifdef LUNALINK_FAULT_INJECTION
  // SWIFI hook: simulate SEU on the CFI counter register by decrementing
  // it after all milestones are complete.  The effect on the terminal
  // check is equivalent to a radiation-induced branch skip. [FI-20]
  if (cfi_skip_inject_ > 0U) {
    cfi_count = cfi_count - cfi_skip_inject_;
    cfi_skip_inject_ = 0U;
  }
#endif

  // ── Threshold decision ──
  if (search.best_corr >= threshold_) {
    result.detected = true;
  } else {
    result.detected = false;
  }

  // ── CFI terminal check: all 3 milestones must have been reached (§5) ──
  if (cfi_count == kCfiMagicSync + 3U) {
    // cppcheck-suppress redundantInitialization -- pessimistic default overwritten only on success
    // path
    status = result.detected ? SyncDetectorStatus::kOk : SyncDetectorStatus::kNotDetected;
  } else {
    // CFI violation: radiation-induced skip detected. FIZ-1
    status = SyncDetectorStatus::kFaultDetected; // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): loop counter corruption
    const FaultPacket pkt{
        .timestamp_ms = 0U, // HAL-provided in flight
        .fault_code = static_cast<uint8_t>(SyncDetectorStatus::kFaultDetected),
        .fiz_level = 1U,
        .originating_step = 0U,
        .reserved = 0U,
    };
    hal_.emit(pkt);
  } // NOCOVER_END

  // Cleanup: scrub result on fault (§7).
  if (status == SyncDetectorStatus::kFaultDetected) { // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): SEU on peak offset
    result = SyncResult{};
  } // NOCOVER_END

  return status;
}

} // namespace lunalink::signal
