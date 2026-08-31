/**
 * @file test_swifi_signal.cpp
 * @brief SWIFI tests for signal subsystem: modulator aliasing, IQ mux aliasing,
 *        sync detector CFI/TMR/recovery.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include "lunalink/codes/gold.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <complex>
#include <cstdint>
#include <random>
#include <vector>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-06 — Aliasing: overlapping input/output spans to modulate_bpsk_generic
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-06 — aliasing_overlap_modulator — kFaultDetected before write",
          "[swifi][signal]") {
  // Chip buffer doubles as output buffer (aliasing)
  std::array<uint8_t, 64> buf{};
  buf.fill(0U);

  // Cast the same buffer to both chip span and output span
  std::span<const uint8_t> chips(buf.data(), buf.size());
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  std::span<int8_t> out(reinterpret_cast<int8_t*>(buf.data()), buf.size());

  const auto status = modulate_bpsk_i(chips, 1, out);
  REQUIRE(status == ModulationStatus::kFaultDetected);

  // Output must be scrubbed (all zero) after aliasing detection
  for (const auto byte : buf) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-07 — Aliasing: overlapping input/output spans to multiplex_iq
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-07 — aliasing_overlap_iq_mux — kFaultDetected before write",
          "[swifi][signal]") {
  // Create a single backing array large enough for out, reused as i_samples
  constexpr size_t kOutSize = 2U * kIqSamplesPerEpoch;
  std::vector<int16_t> out_buf(kOutSize, 0);

  // Q samples: separate valid buffer
  std::vector<int8_t> q_buf(kWeil10230ChipLength, 1);

  // I samples: alias the first bytes of out_buf
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  std::span<const int8_t> i_alias(reinterpret_cast<const int8_t*>(out_buf.data()), kGoldChipLength);
  std::span<int16_t> out(out_buf.data(), kOutSize);

  const auto status = multiplex_iq(i_alias, std::span<const int8_t>(q_buf), out);
  REQUIRE(status == IqMuxStatus::kFaultDetected);

  // Output must be scrubbed after detection
  for (const auto sample : out_buf) {
    CHECK(sample == 0);
  }
}

// ---------------------------------------------------------------------------
// FI-20 — Sync Detector: CFI sentinel skip (FM-056)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: sync_detector_cfi_skip — injected milestone skip fires kFaultDetected",
          "[swifi][signal]") {
  CaptureTelemetryHal<4> capture;
  SyncDetector det(capture);
  SyncResult result{};

  // Build a clean frame with sync pattern at offset 0.
  std::array<uint8_t, kFrameLength> frame{};
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    frame[i] = kSyncPattern[i];
  }
  std::array<float, kFrameLength> symbols{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    symbols[i] = (frame[i] == 0U) ? 1.0F : -1.0F;
  }

  // Phase 1: Inject — simulate radiation-induced branch skip.
  det.inject_cfi_skip(1U);

  // Phase 2: Execute — detection with a signal that would normally succeed.
  const auto status = det.detect(std::span<const float>(symbols), result);

  // Phase 3: Assert Detection — CFI terminal check must fire.
  REQUIRE(status == SyncDetectorStatus::kFaultDetected);
  REQUIRE(capture.count() == 1U);      // FaultPacket emitted
  REQUIRE(result.detected == false);   // Output scrubbed on fault path
  REQUIRE(result.frame_offset == 0U);  // Zeroed by secure scrub
  REQUIRE(result.peak_metric == 0.0F); // Zeroed by secure scrub
}

// ---------------------------------------------------------------------------
// FI-21 — Sync Detector: TMR peak offset corruption repaired (FM-055)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: sync_detector_tmr_peak — vote repairs single-copy offset corruption",
          "[swifi][signal]") {
  NullTelemetryHal null_hal;
  SyncDetector det(null_hal);
  SyncResult result{};

  // Sync pattern at offset 500 in a 7000-symbol stream.
  constexpr size_t kStreamLen = 7000U;
  std::array<uint8_t, kStreamLen> bits{};
  std::mt19937 rng(321U);
  std::uniform_int_distribution<uint32_t> bit(0U, 1U);
  for (auto& b : bits) {
    b = static_cast<uint8_t>(bit(rng));
  }
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    bits[500U + i] = kSyncPattern[i];
  }
  std::array<float, kStreamLen> symbols{};
  for (size_t i = 0; i < kStreamLen; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    symbols[i] = (bits[i] == 0U) ? 1.0F : -1.0F;
  }

  // Phase 1: Clean detection — populates TMR with [500, 500, 500].
  auto status = det.detect(std::span<const float>(symbols), result);
  REQUIRE(status == SyncDetectorStatus::kOk);
  REQUIRE(result.frame_offset == 500U);

  // Phase 2: Inject — corrupt one TMR copy: [9999, 500, 500].
  det.inject_tmr_fault(0, 9999U);

  // Phase 3: Vote — majority vote on corrupted TMR returns 500.
  const uint16_t voted = det.voted_peak_offset();
  REQUIRE(voted == 500U);
}

// ---------------------------------------------------------------------------
// FI-22 — Sync Detector: CFI recovery after injected fault (FM-056)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: sync_detector_cfi_recovery — clean detection after injected fault",
          "[swifi][signal]") {
  CaptureTelemetryHal<4> capture;
  SyncDetector det(capture);
  SyncResult result{};

  std::array<uint8_t, kFrameLength> frame{};
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    frame[i] = kSyncPattern[i];
  }
  std::array<float, kFrameLength> symbols{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    symbols[i] = (frame[i] == 0U) ? 1.0F : -1.0F;
  }

  // First call: inject fault → kFaultDetected.
  det.inject_cfi_skip(1U);
  const auto status1 = det.detect(std::span<const float>(symbols), result);
  REQUIRE(status1 == SyncDetectorStatus::kFaultDetected);
  REQUIRE(capture.count() == 1U);

  // Second call: no injection → clean recovery.
  const auto status2 = det.detect(std::span<const float>(symbols), result);
  REQUIRE(status2 == SyncDetectorStatus::kOk);
  REQUIRE(result.detected == true);
  REQUIRE(result.frame_offset == 0U);
  REQUIRE(capture.count() == 1U); // No additional FaultPackets
}
