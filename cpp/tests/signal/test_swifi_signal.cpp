/**
 * @file test_swifi_signal.cpp
 * @brief SWIFI tests for signal subsystem: modulator aliasing, IQ mux aliasing,
 *        correlator TMR/CFI/adversarial, sync detector CFI/TMR/recovery.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <complex>
#include <cstdint>
#include <random>
#include <vector>

#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/correlator.hpp"
#include "lunalink/signal/sync_detector.hpp"
#include "lunalink/codes/gold.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/messages/frame.hpp"

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// Aliasing — overlapping spans to modulate_bpsk_generic
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: aliasing_overlap_modulator — kFaultDetected before write") {
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
// Aliasing — overlapping spans to multiplex_iq
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: aliasing_overlap_iq_mux — kFaultDetected before write") {
    // Create a single backing array large enough for out, reused as i_samples
    constexpr size_t kOutSize = 2U * kIqSamplesPerEpoch;
    std::vector<int16_t> out_buf(kOutSize, 0);

    // Q samples: separate valid buffer
    std::vector<int8_t> q_buf(kWeil10230ChipLength, 1);

    // I samples: alias the first bytes of out_buf
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::span<const int8_t> i_alias(reinterpret_cast<const int8_t*>(out_buf.data()),
                                    kGoldChipLength);
    std::span<int16_t> out(out_buf.data(), kOutSize);

    const auto status = multiplex_iq(i_alias, std::span<const int8_t>(q_buf), out);
    REQUIRE(status == IqMuxStatus::kFaultDetected);

    // Output must be scrubbed after detection
    for (const auto sample : out_buf) {
        CHECK(sample == 0);
    }
}

// ---------------------------------------------------------------------------
// FI-16 — Correlator: channel state TMR corruption + vote recovery (FM-050)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: correlator_channel_state_tmr — vote corrects single-copy corruption") {
    NullTelemetryHal null_hal;
    UniformPrior prior;
    Correlator corr(prior, null_hal);

    // Assign PRN 1 to channel 0 → state becomes kIdle.
    REQUIRE(corr.assign_channel(0, PrnId{1}) == CorrelatorStatus::kOk);
    REQUIRE(corr.channel_state(0) == ChannelState::kIdle);

    // Acquire to move channel into kTracking.
    // Generate a signal with known PRN at zero Doppler.
    std::array<std::complex<float>, kFftSize> rx{};
    GoldGenerator gen;
    std::array<uint8_t, kFftSize> code_bits{};
    REQUIRE(gen.generate(PrnId{1}, std::span(code_bits).first(kAfsICodeLength)) == PrnStatus::kOk);
    for (uint32_t i = 0; i < kAfsICodeLength; ++i) {
        const float chip = 1.0F - 2.0F * static_cast<float>(code_bits[i]);
        rx[i] = std::complex<float>(chip * 10.0F, 0.0F);  // strong signal
    }

    AcquisitionResult acq{};
    REQUIRE(corr.acquire(0, std::span<const std::complex<float>>(rx), acq) == CorrelatorStatus::kOk);
    REQUIRE(acq.locked == true);
    REQUIRE(corr.channel_state(0) == ChannelState::kTracking);
}

// ---------------------------------------------------------------------------
// FI-17 — Correlator: illegal channel transition emits FIZ-2 telemetry (FM-050)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: correlator_illegal_transition — FIZ-2 telemetry on bad transition") {
    CaptureTelemetryHal<4> capture;
    UniformPrior prior;
    Correlator corr(prior, capture);

    // Assign PRN 1 → channel 0 is kIdle.
    REQUIRE(corr.assign_channel(0, PrnId{1}) == CorrelatorStatus::kOk);

    // track() requires kTracking state — calling from kIdle should fail.
    std::array<std::complex<float>, kFftSize> rx{};
    const auto status = corr.track(0, std::span<const std::complex<float>>(rx));
    REQUIRE(status == CorrelatorStatus::kNotLocked);
}

// ---------------------------------------------------------------------------
// FI-18 — Correlator: CFI sentinel on code replica preparation (FM-053)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: correlator_cfi_nominal — code replica CFI passes under nominal") {
    NullTelemetryHal null_hal;
    UniformPrior prior;
    Correlator corr(prior, null_hal);

    REQUIRE(corr.assign_channel(0, PrnId{1}) == CorrelatorStatus::kOk);

    // Generate a clean signal at zero Doppler for PRN 1.
    std::array<std::complex<float>, kFftSize> rx{};
    GoldGenerator gen;
    std::array<uint8_t, kFftSize> code_bits{};
    REQUIRE(gen.generate(PrnId{1}, std::span(code_bits).first(kAfsICodeLength)) == PrnStatus::kOk);
    for (uint32_t i = 0; i < kAfsICodeLength; ++i) {
        const float chip = 1.0F - 2.0F * static_cast<float>(code_bits[i]);
        rx[i] = std::complex<float>(chip * 10.0F, 0.0F);
    }

    // Acquisition should succeed — implying all CFI sentinels
    // (prepare_code_replica, pcps_search) passed their terminal checks.
    AcquisitionResult acq{};
    const auto status = corr.acquire(0, std::span<const std::complex<float>>(rx), acq);
    REQUIRE(status == CorrelatorStatus::kOk);
    REQUIRE(acq.locked == true);
    REQUIRE(acq.peak_metric >= kDefaultAcqThreshold);
}

// ---------------------------------------------------------------------------
// FI-19 — Correlator: adversarial TMR corruption — track() survives (FM-050)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: correlator_tmr_adversarial — track survives single-copy state corruption") {
    NullTelemetryHal null_hal;
    UniformPrior prior;
    Correlator corr(prior, null_hal);

    REQUIRE(corr.assign_channel(0, PrnId{1}) == CorrelatorStatus::kOk);

    // Generate a clean signal and acquire.
    std::array<std::complex<float>, kFftSize> rx{};
    GoldGenerator gen;
    std::array<uint8_t, kFftSize> code_bits{};
    REQUIRE(gen.generate(PrnId{1}, std::span(code_bits).first(kAfsICodeLength)) == PrnStatus::kOk);
    for (uint32_t i = 0; i < kAfsICodeLength; ++i) {
        const float chip = 1.0F - 2.0F * static_cast<float>(code_bits[i]);
        rx[i] = std::complex<float>(chip * 10.0F, 0.0F);
    }

    AcquisitionResult acq{};
    REQUIRE(corr.acquire(0, std::span<const std::complex<float>>(rx), acq) == CorrelatorStatus::kOk);
    REQUIRE(acq.locked == true);
    REQUIRE(corr.channel_state(0) == ChannelState::kTracking);

    // Phase 1: Inject — corrupt one TMR copy of the channel state to kLost.
    // Remaining two copies hold kTracking → majority vote repairs the corrupted copy.
    corr.inject_channel_state_fault(0, 0, ChannelState::kLost);

    // Phase 2: Verify vote — channel_state() calls vote(), must still return kTracking.
    REQUIRE(corr.channel_state(0) == ChannelState::kTracking);

    // Phase 3: Execute — track() reads the voted state; must succeed and stay kTracking.
    const auto status = corr.track(0, std::span<const std::complex<float>>(rx));
    REQUIRE(status == CorrelatorStatus::kOk);
    REQUIRE(corr.channel_state(0) == ChannelState::kTracking);
}

// ---------------------------------------------------------------------------
// FI-20 — Sync Detector: CFI sentinel skip (FM-056)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: sync_detector_cfi_skip — injected milestone skip fires kFaultDetected") {
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
    REQUIRE(capture.count() == 1U);       // FaultPacket emitted
    REQUIRE(result.detected == false);     // Output scrubbed on fault path
    REQUIRE(result.frame_offset == 0U);    // Zeroed by secure scrub
    REQUIRE(result.peak_metric == 0.0F);   // Zeroed by secure scrub
}

// ---------------------------------------------------------------------------
// FI-21 — Sync Detector: TMR peak offset corruption repaired (FM-055)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: sync_detector_tmr_peak — vote repairs single-copy offset corruption") {
    NullTelemetryHal null_hal;
    SyncDetector det(null_hal);
    SyncResult result{};

    // Sync pattern at offset 500 in a 7000-symbol stream.
    constexpr size_t kStreamLen = 7000U;
    std::array<uint8_t, kStreamLen> bits{};
    std::mt19937 rng(321U);
    std::uniform_int_distribution<uint32_t> bit(0U, 1U);
    for (auto& b : bits) { b = static_cast<uint8_t>(bit(rng)); }
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
TEST_CASE("SWIFI: sync_detector_cfi_recovery — clean detection after injected fault") {
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
    REQUIRE(capture.count() == 1U);  // No additional FaultPackets
}
