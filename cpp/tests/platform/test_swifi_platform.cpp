/**
 * @file test_swifi_platform.cpp
 * @brief SWIFI tests for platform subsystem: TMR, CheckedRange, CfiGuard,
 *        FSM, watchdog, saturating arithmetic, POST, and checksum golden.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"
#include "lunalink/platform/signal_chain_fsm.hpp"
#include "lunalink/platform/watchdog.hpp"
#include "lunalink/platform/post.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/codes/codebook_checksums.hpp"
#include "lunalink/codes/legendre.hpp"

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// TMR — single copy corruption
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: tmr_single_copy_corrupt — vote() corrects minority copy") {
    constexpr uint32_t kGood = 0xA5A5A5A5U;
    constexpr uint32_t kBad  = 0xDEADBEEFU;

    TmrValue<uint32_t> tmr(kGood);

    // Corrupt v1 only
    tmr.inject_fault(0U, kBad);
    REQUIRE(tmr.vote() == kGood);  // majority wins
    // After vote(), v1 should have been repaired
    REQUIRE(tmr.v1 == kGood);

    // Corrupt v2 only
    tmr.refresh(kGood);
    tmr.inject_fault(1U, kBad);
    REQUIRE(tmr.vote() == kGood);
    REQUIRE(tmr.v2 == kGood);

    // Corrupt v3 only
    tmr.refresh(kGood);
    tmr.inject_fault(2U, kBad);
    REQUIRE(tmr.vote() == kGood);
    REQUIRE(tmr.v3 == kGood);
}

// ---------------------------------------------------------------------------
// TMR — all copies corrupted (residual risk, no majority)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: tmr_all_copies_same_corrupt — documents residual risk") {
    // When all three copies hold the same wrong value, TMR cannot detect the
    // error (no majority disagreement). This is an acknowledged residual risk
    // in the FMEA. The test documents and verifies this is the only undetectable
    // scenario; it is NOT a pass/fail requirement.
    constexpr uint32_t kBad = 0xDEADBEEFU;
    TmrValue<uint32_t> tmr(42U);
    tmr.inject_fault(kBad);           // all-copies overwrite
    const uint32_t result = tmr.vote();
    // vote() returns the (wrong) value — no detection possible
    CHECK(result == kBad);  // documents residual risk, not a defect
}

// ---------------------------------------------------------------------------
// CheckedRange — force beyond max, next assignment saturates
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: checked_range_force_beyond_max — next write saturates") {
    CheckedRange<uint8_t, 0U, 10U> cr(5U);

    // force_set bypasses saturation
    cr.force_set(200U);
    REQUIRE(cr.value() == 200U);  // raw value escaped bounds

    // Normal assignment re-saturates
    cr = 200U;
    REQUIRE(cr.value() == 10U);   // saturated back to Max
}

// ---------------------------------------------------------------------------
// CfiGuard — inject_skip causes verify() to return false
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: cfi_guard_inject_skip — detect skipped step") {
    // Guard expects 3 ticks
    CfiGuard guard(0xBEEF0000U, 3U);
    guard.tick();
    guard.tick();
    guard.tick();
    REQUIRE(guard.verify() == true);

    // Now inject a skip of 2 — as if two ticks never happened
    CfiGuard guard2(0xBEEF0000U, 3U);
    guard2.tick();
    guard2.inject_skip(1U);  // simulate one missing tick
    REQUIRE(guard2.verify() == false);
}

// ---------------------------------------------------------------------------
// FSM — illegal transition detected, telemetry captured
// ---------------------------------------------------------------------------
TEST_CASE("[SW-SAF-03] SWIFI: fsm_illegal_transition — Zone 3, telemetry packet emitted") {
    CaptureTelemetryHal<4> capture;
    SignalChainFsm fsm(capture);

    // Walk to kSafeMode via a legal path
    REQUIRE(fsm.transition(SignalChainState::kPostRunning) == true);
    REQUIRE(fsm.transition(SignalChainState::kSafeMode) == true);  // POST fail

    // Attempt illegal: kSafeMode -> kOperational (not in legal table)
    const bool ok = fsm.transition(SignalChainState::kOperational);
    REQUIRE(ok == false);
    REQUIRE(fsm.state() == SignalChainState::kSafeMode);

    // A telemetry packet must have been emitted
    REQUIRE(capture.count() >= 1U);
    const auto& pkt = capture.packet(capture.count() - 1U);
    CHECK(pkt.fiz_level == 3U);
}

// ---------------------------------------------------------------------------
// FSM — two consecutive frame faults escalate to Zone 3
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: fsm_two_consecutive_frame_faults — escalates to kSafeMode") {
    CaptureTelemetryHal<4> capture;
    SignalChainFsm fsm(capture);

    REQUIRE(fsm.transition(SignalChainState::kPostRunning) == true);
    REQUIRE(fsm.transition(SignalChainState::kOperational) == true);

    // First frame fault: Zone 2, stays recoverable
    REQUIRE(fsm.on_frame_fault() == true);
    REQUIRE(fsm.state() == SignalChainState::kFrameFault);

    // Second consecutive frame fault: Zone 3
    REQUIRE(fsm.on_frame_fault() == false);
    REQUIRE(fsm.state() == SignalChainState::kSafeMode);

    // Verify telemetry was emitted
    REQUIRE(capture.count() >= 1U);
    CHECK(capture.packet(capture.count() - 1U).fiz_level == 3U);
}

// ---------------------------------------------------------------------------
// Saturating arithmetic — CheckedRange clamps over- and under-range values
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: saturating_arithmetic_clamp — CheckedRange clamps out-of-range correctly") {
    // Use a narrow range (0..100) so clamping is observable at the type level.
    using Sat = CheckedRange<int32_t, 0, 100>;

    // Over-range: assigning 200 must clamp to 100.
    Sat hi(200);
    REQUIRE(hi.value() == 100);

    // Under-range: assigning -1 must clamp to 0.
    Sat lo(-1);
    REQUIRE(lo.value() == 0);

    // Assignment operator also saturates.
    Sat val(50);
    val = 150;
    REQUIRE(val.value() == 100);
    val = -10;
    REQUIRE(val.value() == 0);

    // force_set bypasses saturation — then reassignment re-clamps.
    val.force_set(-99);
    REQUIRE(val.value() == -99);  // bypassed
    val = 50;
    REQUIRE(val.value() == 50);   // back in range, no clamping needed
}

// ---------------------------------------------------------------------------
// FM-033 — Windowed Watchdog lower-bound trip (Zone 2 → Zone 3 escalation)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: watchdog_lower_bound_trip — FaultPacket emitted on under-run") {
    constexpr uint32_t kTMin = 100U;
    constexpr uint32_t kTMax = 10000U;
    WindowedWatchdog wdog(kTMin, kTMax);
    CaptureTelemetryHal<4> capture;

    // --- Nominal path: exactly T_min ticks, no fault ---
    wdog.reset();
    for (uint32_t i = 0U; i < kTMin; ++i) { wdog.tick(); }
    const bool nominal = wdog.evaluate(capture, 0U);
    REQUIRE(nominal == true);
    REQUIRE(capture.count() == 0U);  // no packet on nominal path

    // --- SWIFI: inject tick count below T_min (radiation-induced logic skip) ---
    wdog.reset();
    for (uint32_t i = 0U; i < kTMin; ++i) { wdog.tick(); }
    wdog.inject_tick_count(kTMin - 1U);  // one tick short → lower-bound violation

    const bool fault = wdog.evaluate(capture, 42000U);
    REQUIRE(fault == false);

    // Exactly one FaultPacket must have been emitted
    REQUIRE(capture.count() == 1U);
    const auto& pkt = capture.packet(0U);
    CHECK(pkt.fiz_level          == 2U);
    CHECK(pkt.fault_code         == kWatchdogLowerBoundFaultCode);
    CHECK(pkt.originating_step   == 0U);
    CHECK(pkt.reserved           == 0U);
    CHECK(pkt.timestamp_ms       == 42000U);

    // Upper-bound query (informational, no fault)
    wdog.reset();
    for (uint32_t i = 0U; i < kTMax + 1U; ++i) { wdog.tick(); }
    REQUIRE(wdog.upper_bound_exceeded() == true);
}

// ---------------------------------------------------------------------------
// FSM — on_frame_fault from kSafeMode: FIZ-3 telemetry, stays in kSafeMode
// ---------------------------------------------------------------------------
TEST_CASE("FSM: on_frame_fault from kSafeMode emits FIZ-3 telemetry and stays in kSafeMode") {
    CaptureTelemetryHal<4> capture;
    SignalChainFsm fsm(capture);

    // Walk FSM into kSafeMode via a POST-fail legal path
    REQUIRE(fsm.transition(SignalChainState::kPostRunning) == true);
    REQUIRE(fsm.transition(SignalChainState::kSafeMode)    == true);  // POST fail → Zone 3
    REQUIRE(fsm.state() == SignalChainState::kSafeMode);
    capture.reset();  // clear setup packets

    // on_frame_fault() called from kSafeMode (radiation-induced CFI diversion)
    const bool result = fsm.on_frame_fault();
    REQUIRE(result == false);
    REQUIRE(fsm.state() == SignalChainState::kSafeMode);

    // Must emit exactly one FIZ-3 telemetry packet
    REQUIRE(capture.count() == 1U);
    CHECK(capture.packet(0U).fiz_level == 3U);
}

// ---------------------------------------------------------------------------
// POST step 2 — LDPC LUT corruption detected
// ---------------------------------------------------------------------------
TEST_CASE("[SW-SAF-04] SWIFI: post_step2_lut_corrupt — run_post returns {kFaultDetected, 2}") {
    // Inject a bit flip into one LDPC matrix
    kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

    const PostResult result = run_post();
    kLdpc_sf2_a.clear_fault();  // always restore, even if CHECK fails

    REQUIRE(result.status == PostStatus::kFaultDetected);
    REQUIRE(result.failed_step == 2U);
}

// ---------------------------------------------------------------------------
// [checksum-golden] — print golden checksum values (one-time helper)
// Used by `task compute-golden-checksums` to populate codebook_checksums.hpp.
// Run: ./lunalink_swifi_tests "[checksum-golden]" --reporter console
// ---------------------------------------------------------------------------
TEST_CASE("checksum-golden: print BCH, Weil, and Legendre codebook checksums", "[checksum-golden]") {
    const uint64_t bch = bch_codebook_checksum();
    const uint64_t w10 = weil10230_codebook_checksum();
    const uint64_t w15 = weil1500_codebook_checksum();
    const uint64_t leg10223 = legendre_10223_checksum();
    const uint64_t leg1499  = legendre_1499_checksum();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    (void)std::printf(
        "LUNALINK_BCH_CODEBOOK_CHECKSUM  = 0x%016llxULL\n"
        "LUNALINK_WEIL10230_CHECKSUM     = 0x%016llxULL\n"
        "LUNALINK_WEIL1500_CHECKSUM      = 0x%016llxULL\n"
        "LUNALINK_LEGENDRE10223_CHECKSUM = 0x%016llxULL\n"
        "LUNALINK_LEGENDRE1499_CHECKSUM  = 0x%016llxULL\n",
        static_cast<unsigned long long>(bch),
        static_cast<unsigned long long>(w10),
        static_cast<unsigned long long>(w15),
        static_cast<unsigned long long>(leg10223),
        static_cast<unsigned long long>(leg1499));
    // Verify they match the committed golden constants
    CHECK(bch == kBchCodebookChecksum);
    CHECK(w10 == kWeil10230Checksum);
    CHECK(w15 == kWeil1500Checksum);
    CHECK(leg10223 == kLegendre10223Checksum);
    CHECK(leg1499  == kLegendre1499Checksum);
}

// ---------------------------------------------------------------------------
// POST step 2 — BCH/Weil checksums match golden constants; POST passes step 2
// ---------------------------------------------------------------------------
TEST_CASE("[SW-SAF-04] POST step 2: all codebook checksums match golden constants; run_post passes",
          "[checksum-golden]") {
    // Verify that the committed golden constants in codebook_checksums.hpp match
    // the actual runtime checksums of all codebook/sequence tables.
    // When they match, run_post() must pass step 2 (status == kPass).
    const uint64_t bch = bch_codebook_checksum();
    const uint64_t w10 = weil10230_codebook_checksum();
    const uint64_t w15 = weil1500_codebook_checksum();
    const uint64_t leg10223 = legendre_10223_checksum();
    const uint64_t leg1499  = legendre_1499_checksum();
    CHECK(bch == kBchCodebookChecksum);
    CHECK(w10 == kWeil10230Checksum);
    CHECK(w15 == kWeil1500Checksum);
    CHECK(leg10223 == kLegendre10223Checksum);
    CHECK(leg1499  == kLegendre1499Checksum);
    // Nominal POST must pass when all checksums match.
    const PostResult result = run_post();
    REQUIRE(result.status == PostStatus::kPass);
}
