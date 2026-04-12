#include "lunalink/messages/frame.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <random>

using namespace lunalink::signal;

namespace {

/// Generate a clean bipolar sync pattern at a given offset within a buffer.
void place_sync_at_offset(std::span<float> buf, size_t offset) {
  // Fill with zeros (no signal)
  for (auto& s : buf) {
    s = 0.0F;
  }
  // Place sync pattern: 0→+1.0, 1→-1.0
  for (size_t i = 0; i < kSyncLength; ++i) {
    buf[offset + i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;
  }
}

/// Add AWGN to a symbol buffer.
void add_awgn(std::span<float> symbols, float eb_n0_db, std::mt19937& rng) {
  const auto eb_n0_lin = std::pow(10.0F, eb_n0_db / 10.0F);
  const auto sigma = std::sqrt(1.0F / (2.0F * eb_n0_lin));
  auto dist = std::normal_distribution<float>(0.0F, sigma);
  for (auto& s : symbols) {
    s += dist(rng);
  }
}

} // namespace

TEST_CASE("Sync detection Pfa: < 1% at 0 dB noise", "[channel][pd][rx][slow]") {
  // False alarm rate test: 1000 trials of pure AWGN, no sync pattern injected.
  // Pairs with the Pd test: both at 0 dB Eb/N0 so the threshold can be verified
  // for Pd ≥ 99% AND Pfa < 1% simultaneously at the same noise level.
  // A detector with miscalibrated threshold could pass Pd while failing Pfa.
  constexpr uint32_t kTrials = 1000;
  constexpr size_t kBufLen = 6200;

  // NOLINTNEXTLINE(cert-msc51-cpp) — deterministic seed for reproducible Pfa measurement
  std::mt19937 rng(99999U);
  uint32_t false_alarms = 0;

  for (uint32_t trial = 0; trial < kTrials; ++trial) {
    std::array<float, kBufLen> buf{};
    // Pure AWGN — buf zero-initialised, then 0 dB noise added (no sync pattern).
    add_awgn(buf, 0.0F, rng);

    NullTelemetryHal hal;
    SyncDetector det(hal);
    SyncResult result{};
    if (det.detect(buf, result) == SyncDetectorStatus::kOk && result.detected) {
      ++false_alarms;
    }
  }

  const auto pfa = static_cast<float>(false_alarms) / static_cast<float>(kTrials);
  INFO("Pfa = " << pfa << " (" << false_alarms << "/" << kTrials << " false alarms)");
  CHECK(pfa < 0.01F); // < 1% false alarm rate — same SNR where Pd ≥ 99%
}

TEST_CASE("Sync detection Pd vs Eb/N0: > 99% at 0 dB", "[channel][pd][rx][slow]") {
  // Monte Carlo: 10,000 trials per SNR point
  // Place sync pattern at random offsets, add AWGN, measure detection rate.
  constexpr uint32_t kTrials = 10000;
  constexpr size_t kBufLen = 6200; // Room for offset + sync

  // SNR test points (dB)
  // NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  const float snr_points[] = {-3.0F, 0.0F, 3.0F, 6.0F, 10.0F};
  // NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  const float pd_required[] = {0.50F, 0.99F, 0.99F, 0.99F, 0.99F}; // -3 dB: relaxed but tested

  for (size_t snr_idx = 0; snr_idx < 5; ++snr_idx) {
    const float eb_n0_db = snr_points[snr_idx];
    const float pd_min = pd_required[snr_idx];

    std::mt19937 rng(static_cast<uint32_t>(snr_idx * 1000U + 42U));
    auto offset_dist = std::uniform_int_distribution<size_t>(0, kBufLen - kSyncLength);
    uint32_t detections = 0;

    for (uint32_t trial = 0; trial < kTrials; ++trial) {
      std::array<float, kBufLen> buf{};
      const size_t offset = offset_dist(rng);
      place_sync_at_offset(buf, offset);
      add_awgn(buf, eb_n0_db, rng);

      NullTelemetryHal hal;
      SyncDetector det(hal);
      SyncResult result{};
      const auto status = det.detect(buf, result);

      if (status == SyncDetectorStatus::kOk && result.detected &&
          result.frame_offset == static_cast<uint16_t>(offset)) {
        detections++;
      }
    }

    const auto pd = static_cast<float>(detections) / static_cast<float>(kTrials);

    INFO("Eb/N0 = " << eb_n0_db << " dB: Pd = " << pd << " (" << detections << "/" << kTrials
                    << ")");
    CHECK(pd >= pd_min);
  }
}
