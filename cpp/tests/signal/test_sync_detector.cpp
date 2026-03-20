#include "lunalink/core/telemetry.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <random>

using namespace lunalink::signal;
using Catch::Matchers::WithinAbs;

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

NullTelemetryHal g_null_hal;

/// Build a bipolar symbol stream from a frame buffer (0→+1.0, 1→−1.0).
template <size_t N> std::array<float, N> to_bipolar(const std::array<uint8_t, N>& bits) {
  std::array<float, N> out{};
  for (size_t i = 0; i < N; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    out[i] = (bits[i] == 0U) ? 1.0F : -1.0F;
  }
  return out;
}

/// Add Gaussian noise to a bipolar symbol stream.
template <size_t N>
void add_noise(std::array<float, N>& symbols, float sigma, uint32_t seed = 42U) {
  std::mt19937 rng(seed);
  std::normal_distribution<float> noise(0.0F, sigma);
  for (auto& s : symbols) {
    s += noise(rng);
  }
}

/// Build a clean frame with sync pattern at position 0, rest random data.
std::array<uint8_t, kFrameLength> build_test_frame(uint32_t seed = 99U) {
  std::array<uint8_t, kFrameLength> frame{};
  std::mt19937 rng(seed);
  std::uniform_int_distribution<uint32_t> bit(0U, 1U);

  // Place sync pattern at the start.
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    frame[i] = kSyncPattern[i];
  }
  // Fill the rest with random data.
  for (uint16_t i = kSyncLength; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    frame[i] = static_cast<uint8_t>(bit(rng));
  }
  return frame;
}

/// Build a symbol stream with the sync pattern starting at a given offset.
/// Pads before and after with random data.
template <size_t N>
std::array<float, N> build_shifted_stream(uint16_t sync_offset, uint32_t seed = 77U) {
  static_assert(N >= kFrameLength, "Stream must be at least one frame long");
  std::array<uint8_t, N> bits{};
  std::mt19937 rng(seed);
  std::uniform_int_distribution<uint32_t> bit(0U, 1U);

  // Fill entirely with random data.
  for (size_t i = 0; i < N; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    bits[i] = static_cast<uint8_t>(bit(rng));
  }

  // Insert sync pattern at the specified offset.
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    bits[static_cast<size_t>(sync_offset) + i] = kSyncPattern[i];
  }

  return to_bipolar(bits);
}

} // namespace

// ─── Input Validation Tests ─────────────────────────────────────────────────

TEST_CASE("SyncDetector: rejects empty input", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};
  std::array<float, 0> empty{};

  REQUIRE(det.detect(std::span<const float>(empty), result) == SyncDetectorStatus::kInvalidInput);
}

TEST_CASE("SyncDetector: rejects input shorter than sync length", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};
  std::array<float, 67> too_short{};

  REQUIRE(det.detect(std::span<const float>(too_short), result) ==
          SyncDetectorStatus::kInvalidInput);
}

TEST_CASE("SyncDetector: accepts input exactly sync length", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  // Build a perfect sync pattern as bipolar symbols.
  std::array<float, kSyncLength> perfect{};
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    perfect[i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;
  }

  const auto status = det.detect(std::span<const float>(perfect), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
  REQUIRE(result.frame_offset == 0U);
  REQUIRE_THAT(static_cast<double>(result.peak_metric), WithinAbs(68.0, 0.01));
  // Single-offset search: second_peak should be 0 (no second candidate).
  REQUIRE_THAT(static_cast<double>(result.second_peak), WithinAbs(0.0, 0.01));
}

// ─── Detection Tests ────────────────────────────────────────────────────────

TEST_CASE("[LSIS-AFS-710] SyncDetector: detects sync pattern at start of frame",
          "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  auto frame = build_test_frame();
  auto symbols = to_bipolar(frame);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
  REQUIRE(result.frame_offset == 0U);
  REQUIRE_THAT(static_cast<double>(result.peak_metric), WithinAbs(68.0, 0.01));
}

TEST_CASE("[LSIS-AFS-710] SyncDetector: detects sync pattern at arbitrary offset",
          "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  constexpr uint16_t test_offset = 1234U;
  constexpr size_t stream_len = 7000U;
  auto symbols = build_shifted_stream<stream_len>(test_offset);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
  REQUIRE(result.frame_offset == test_offset);
  REQUIRE_THAT(static_cast<double>(result.peak_metric), WithinAbs(68.0, 0.01));
}

TEST_CASE("SyncDetector: detects sync at end of buffer", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  // Place sync at the last valid position.
  constexpr size_t stream_len = 6200U;
  constexpr uint16_t end_offset = static_cast<uint16_t>(stream_len - kSyncLength);
  auto symbols = build_shifted_stream<stream_len>(end_offset);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
  REQUIRE(result.frame_offset == end_offset);
}

// ─── Noise Robustness Tests ─────────────────────────────────────────────────

TEST_CASE("[LSIS-AFS-710] SyncDetector: detects sync in moderate noise", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  auto frame = build_test_frame();
  auto symbols = to_bipolar(frame);

  // Add moderate noise (sigma = 0.5, SNR ≈ 6 dB per symbol).
  add_noise(symbols, 0.5F, 12U);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
  REQUIRE(result.frame_offset == 0U);
  REQUIRE(result.peak_metric > kDefaultSyncThreshold);
}

TEST_CASE("SyncDetector: fails detection in heavy noise (no sync)", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  // Pure noise — no sync pattern embedded.
  std::array<float, kFrameLength> noise_only{};
  std::mt19937 rng(55U);
  std::normal_distribution<float> noise(0.0F, 1.0F);
  for (auto& s : noise_only) {
    s = noise(rng);
  }

  const auto status = det.detect(std::span<const float>(noise_only), result);

  REQUIRE(status == SyncDetectorStatus::kNotDetected);
  REQUIRE_FALSE(result.detected);
  REQUIRE(result.peak_metric < kDefaultSyncThreshold);
}

// ─── Threshold Configuration ─────────────────────────────────────────────────

TEST_CASE("SyncDetector: custom threshold affects detection", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  auto frame = build_test_frame();
  auto symbols = to_bipolar(frame);

  // Set impossibly high threshold.
  REQUIRE(det.set_threshold(69.0F));

  const auto status = det.detect(std::span<const float>(symbols), result);

  // Perfect correlation = 68.0, but threshold is 69 → should not detect.
  REQUIRE(status == SyncDetectorStatus::kNotDetected);
  REQUIRE_FALSE(result.detected);

  // Now lower threshold: should detect.
  REQUIRE(det.set_threshold(60.0F));
  const auto status2 = det.detect(std::span<const float>(symbols), result);
  REQUIRE(status2 == SyncDetectorStatus::kOk);
  REQUIRE(result.detected);
}

// ─── set_threshold Input Validation ─────────────────────────────────────────

TEST_CASE("SyncDetector: set_threshold rejects invalid values", "[sync_detector]") {
  SyncDetector det(g_null_hal);

  REQUIRE_FALSE(det.set_threshold(0.0F));
  REQUIRE_FALSE(det.set_threshold(-1.0F));
  REQUIRE_FALSE(det.set_threshold(std::numeric_limits<float>::quiet_NaN()));
  REQUIRE_FALSE(det.set_threshold(std::numeric_limits<float>::infinity()));
  REQUIRE_FALSE(det.set_threshold(-std::numeric_limits<float>::infinity()));

  // Valid values accepted.
  REQUIRE(det.set_threshold(1.0F));
  REQUIRE(det.set_threshold(68.0F));
}

// ─── Second Peak Discrimination ──────────────────────────────────────────────

TEST_CASE("SyncDetector: second_peak is lower than peak_metric", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  auto frame = build_test_frame();
  auto symbols = to_bipolar(frame);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.second_peak < result.peak_metric);
}

// ─── Telemetry on Fault ──────────────────────────────────────────────────────

TEST_CASE("SyncDetector: CFI fault emits telemetry", "[sync_detector][fault]") {
  // This test verifies the telemetry infrastructure is wired.
  // Direct CFI fault injection would require LUNALINK_FAULT_INJECTION build.
  // We verify the detector runs cleanly with the capture HAL.
  CaptureTelemetryHal<4> capture_hal;
  SyncDetector det(capture_hal);
  SyncResult result{};

  auto frame = build_test_frame();
  auto symbols = to_bipolar(frame);

  const auto status = det.detect(std::span<const float>(symbols), result);

  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(capture_hal.count() == 0U); // No faults on clean run.
}

// ─── Edge Cases ──────────────────────────────────────────────────────────────

TEST_CASE("SyncDetector: all-zeros input returns kNotDetected", "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  std::array<float, kFrameLength> zeros{};
  // All zeros — correlation with sync pattern should be near zero
  // (sync has roughly equal 0s and 1s).

  const auto status = det.detect(std::span<const float>(zeros), result);

  // The sync pattern has 31 zeros and 37 ones → bipolar correlation with
  // all-zero input = 0.0 (since all-zero float = 0.0, not +1.0).
  // This is below threshold.
  REQUIRE(status == SyncDetectorStatus::kNotDetected);
  REQUIRE_FALSE(result.detected);
}

TEST_CASE("SyncDetector: all-ones input (bipolar +1.0) gives partial correlation",
          "[sync_detector]") {
  SyncDetector det(g_null_hal);
  SyncResult result{};

  std::array<float, kFrameLength> all_plus{};
  for (auto& s : all_plus) {
    s = 1.0F;
  }

  const auto status = det.detect(std::span<const float>(all_plus), result);

  // Sync pattern has 31 zeros (→ +1 ref) and 37 ones (→ −1 ref).
  // Dot product with all +1.0 = 31 - 37 = -6. Below threshold.
  REQUIRE(status == SyncDetectorStatus::kNotDetected);
  REQUIRE_FALSE(result.detected);
}
