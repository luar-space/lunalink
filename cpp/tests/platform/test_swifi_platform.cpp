/**
 * @file test_swifi_platform.cpp
 * @brief SWIFI tests for platform subsystem: TMR, CheckedRange, CfiGuard,
 *        FSM, watchdog, saturating arithmetic, POST, and checksum golden.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include "lunalink/codes/codebook_checksums.hpp"
#include "lunalink/codes/legendre.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/platform/post.hpp"
#include "lunalink/platform/signal_chain_fsm.hpp"
#include "lunalink/platform/watchdog.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-01 — TMR single-copy corruption: vote repairs minority copy
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-01 — tmr_single_copy_corrupt — vote() corrects minority copy", "[swifi][platform]") {
  constexpr uint32_t kGood = 0xA5A5A5A5U;
  constexpr uint32_t kBad = 0xDEADBEEFU;

  TmrValue<uint32_t> tmr(kGood);

  // Corrupt v1 only
  tmr.inject_fault(0U, kBad);
  REQUIRE(tmr.vote() == kGood); // majority wins
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
// FI-35 — TMR two-of-three corruption: different bad values, no majority
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: tmr_two_of_three_different — no majority, recovery fails", "[swifi][platform]") {
    // When two copies hold different bad values, no two copies agree.
    // vote() falls back to v1 (the first copy). This documents the detection
    // boundary: TMR detects single-copy corruption but not double-copy
    // corruption with distinct values.
    constexpr uint32_t kGood = 0xA5A5A5A5U;
    constexpr uint32_t kBad1 = 0xDEADBEEFU;
    constexpr uint32_t kBad2 = 0xCAFEBABEU;

    TmrValue<uint32_t> tmr(kGood);

    // Corrupt v1 and v2 with different values: [BAD1, BAD2, GOOD]
    tmr.inject_fault(0U, kBad1);
    tmr.inject_fault(1U, kBad2);

    // No majority exists — vote() returns v1 (first copy) as fallback.
    // v3 holds the correct value but is outnumbered by two disagreeing copies.
    const uint32_t result = tmr.vote();

    // Document: with two different corruptions, TMR cannot recover.
    // It returns v1 (which happens to be kBad1). This is a known limitation:
    // TMR only guarantees correction of single-copy faults.
    CHECK(result != kGood);  // documents that recovery failed
}

// ---------------------------------------------------------------------------
// FI-36 — TMR two-of-three corruption: same bad value, wrong majority wins
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: tmr_two_of_three_same — wrong majority wins", "[swifi][platform]") {
    // When two copies hold the same wrong value, they form a (false) majority.
    // vote() returns the wrong value and repairs the correct copy to match.
    // This confirms the FI-02 residual risk from the other direction.
    constexpr uint32_t kGood = 0xA5A5A5A5U;
    constexpr uint32_t kBad  = 0xDEADBEEFU;

    TmrValue<uint32_t> tmr(kGood);
    tmr.inject_fault(0U, kBad);
    tmr.inject_fault(1U, kBad);

    // [BAD, BAD, GOOD] — majority is BAD (wrong)
    CHECK(tmr.vote() == kBad);  // wrong majority wins — residual risk
}

// ---------------------------------------------------------------------------
// FI-02 — TMR all-copies corruption: residual risk documentation
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-02 — tmr_all_copies_same_corrupt — documents residual risk", "[swifi][platform]") {
  // When all three copies hold the same wrong value, TMR cannot detect the
  // error (no majority disagreement). This is an acknowledged residual risk
  // in the FMEA. The test documents and verifies this is the only undetectable
  // scenario; it is NOT a pass/fail requirement.
  constexpr uint32_t kBad = 0xDEADBEEFU;
  TmrValue<uint32_t> tmr(42U);
  tmr.inject_fault(kBad); // all-copies overwrite
  const uint32_t result = tmr.vote();
  // vote() returns the (wrong) value — no detection possible
  CHECK(result == kBad); // documents residual risk, not a defect
}

// ---------------------------------------------------------------------------
// FI-03 — CheckedRange force beyond max: next assignment re-saturates
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-03 — checked_range_force_beyond_max — next write saturates", "[swifi][platform]") {
  CheckedRange<uint8_t, 0U, 10U> cr(5U);

  // force_set bypasses saturation
  cr.force_set(200U);
  REQUIRE(cr.value() == 200U); // raw value escaped bounds

  // Normal assignment re-saturates
  cr = 200U;
  REQUIRE(cr.value() == 10U); // saturated back to Max
}

// ---------------------------------------------------------------------------
// FI-04 — CfiGuard injected skip: verify() returns false, caller faults
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-04 — cfi_guard_inject_skip — detect skipped step", "[swifi][platform]") {
  // Guard expects 3 ticks
  CfiGuard guard(0xBEEF0000U, 3U);
  guard.tick();
  guard.tick();
  guard.tick();
  REQUIRE(guard.verify() == true);

  // Now inject a skip of 2 — as if two ticks never happened
  CfiGuard guard2(0xBEEF0000U, 3U);
  guard2.tick();
  guard2.inject_skip(1U); // simulate one missing tick
  REQUIRE(guard2.verify() == false);
}

// ---------------------------------------------------------------------------
// FI-08 — FSM illegal transition: detection and Zone-3 telemetry emission
// ---------------------------------------------------------------------------
TEST_CASE("[SW-SAF-03] SWIFI: FI-08 — fsm_illegal_transition — Zone 3, telemetry packet emitted", "[swifi][platform]") {
  CaptureTelemetryHal<4> capture;
  SignalChainFsm fsm(capture);

  // Walk to kSafeMode via a legal path
  REQUIRE(fsm.transition(SignalChainState::kPostRunning) == true);
  REQUIRE(fsm.transition(SignalChainState::kSafeMode) == true); // POST fail

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
// FI-09 — FSM two consecutive frame faults: Zone-2 → Zone-3 escalation
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-09 — fsm_two_consecutive_frame_faults — escalates to kSafeMode", "[swifi][platform]") {
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
// FI-11 — Saturating arithmetic: CheckedRange clamps over/under-range writes
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-11 — saturating_arithmetic_clamp — CheckedRange clamps out-of-range correctly", "[swifi][platform]") {
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
  REQUIRE(val.value() == -99); // bypassed
  val = 50;
  REQUIRE(val.value() == 50); // back in range, no clamping needed
}

// ---------------------------------------------------------------------------
// FI-12 — Windowed watchdog lower-bound violation (FM-033 Zone 2 escalation)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-12 — watchdog_lower_bound_trip — FaultPacket emitted on under-run", "[swifi][platform]") {
  constexpr uint32_t kTMin = 100U;
  constexpr uint32_t kTMax = 10000U;
  WindowedWatchdog wdog(kTMin, kTMax);
  CaptureTelemetryHal<4> capture;

  // --- Nominal path: exactly T_min ticks, no fault ---
  wdog.reset();
  for (uint32_t i = 0U; i < kTMin; ++i) {
    wdog.tick();
  }
  const bool nominal = wdog.evaluate(capture, 0U);
  REQUIRE(nominal == true);
  REQUIRE(capture.count() == 0U); // no packet on nominal path

  // --- SWIFI: inject tick count below T_min (radiation-induced logic skip) ---
  wdog.reset();
  for (uint32_t i = 0U; i < kTMin; ++i) {
    wdog.tick();
  }
  wdog.inject_tick_count(kTMin - 1U); // one tick short → lower-bound violation

  const bool fault = wdog.evaluate(capture, 42000U);
  REQUIRE(fault == false);

  // Exactly one FaultPacket must have been emitted
  REQUIRE(capture.count() == 1U);
  const auto& pkt = capture.packet(0U);
  CHECK(pkt.fiz_level == 2U);
  CHECK(pkt.fault_code == kWatchdogLowerBoundFaultCode);
  CHECK(pkt.originating_step == 0U);
  CHECK(pkt.reserved == 0U);
  CHECK(pkt.timestamp_ms == 42000U);

  // Upper-bound query (informational, no fault)
  wdog.reset();
  for (uint32_t i = 0U; i < kTMax + 1U; ++i) {
    wdog.tick();
  }
  REQUIRE(wdog.upper_bound_exceeded() == true);
}

// ---------------------------------------------------------------------------
// FI-32 — Timing injection: watchdog upper-bound exceeded (informational)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: watchdog_upper_bound_exceeded — informational detection via inject", "[swifi][platform]") {
    constexpr uint32_t kTMin = 100U;
    constexpr uint32_t kTMax = 10000U;
    WindowedWatchdog wdog(kTMin, kTMax);
    CaptureTelemetryHal<4> capture;

    // Normal operation: tick to within bounds
    wdog.reset();
    for (uint32_t i = 0U; i < kTMin; ++i) { wdog.tick(); }
    REQUIRE(wdog.upper_bound_exceeded() == false);

    // Inject tick count just above upper bound
    wdog.inject_tick_count(kTMax + 1U);
    REQUIRE(wdog.upper_bound_exceeded() == true);

    // evaluate() still passes (upper bound is informational, hardware watchdog
    // owns the hang path) — but the counter exceeds t_min so no lower-bound trip
    const bool eval = wdog.evaluate(capture, 0U);
    REQUIRE(eval == true);
    REQUIRE(capture.count() == 0U);  // no FaultPacket on upper-bound path
}

// ---------------------------------------------------------------------------
// FI-33 — Timing injection: zero-tick starvation (complete wip_tick absence)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: watchdog_zero_tick_starvation — lower-bound trip on zero ticks", "[swifi][platform]") {
    constexpr uint32_t kTMin = 50U;
    constexpr uint32_t kTMax = 500U;
    WindowedWatchdog wdog(kTMin, kTMax);
    CaptureTelemetryHal<4> capture;

    // Reset and immediately evaluate — zero ticks (complete starvation)
    wdog.reset();
    REQUIRE(wdog.tick_count() == 0U);

    const bool eval = wdog.evaluate(capture, 1000U);
    REQUIRE(eval == false);  // lower-bound violated (0 < t_min)

    // FaultPacket must have been emitted
    REQUIRE(capture.count() == 1U);
    CHECK(capture.packet(0U).fiz_level == 2U);
    CHECK(capture.packet(0U).fault_code == kWatchdogLowerBoundFaultCode);
}

// ---------------------------------------------------------------------------
// FI-34 — Timing injection: inject_tick_count at exact boundaries
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: watchdog_boundary_ticks — exact t_min and t_max pass", "[swifi][platform]") {
    constexpr uint32_t kTMin = 100U;
    constexpr uint32_t kTMax = 10000U;
    WindowedWatchdog wdog(kTMin, kTMax);
    CaptureTelemetryHal<4> capture;

    // Exact lower bound: t_min ticks — must pass
    wdog.reset();
    wdog.inject_tick_count(kTMin);
    REQUIRE(wdog.evaluate(capture, 0U) == true);
    REQUIRE(wdog.upper_bound_exceeded() == false);

    // Exact upper bound: t_max ticks — must pass (upper bound is <=)
    wdog.reset();
    wdog.inject_tick_count(kTMax);
    REQUIRE(wdog.evaluate(capture, 0U) == true);
    REQUIRE(wdog.upper_bound_exceeded() == false);

    // One below lower bound — must fail
    wdog.reset();
    wdog.inject_tick_count(kTMin - 1U);
    REQUIRE(wdog.evaluate(capture, 0U) == false);
    REQUIRE(capture.count() == 1U);

    // No packets on the passing cases
}

// ---------------------------------------------------------------------------
// FI-10 — FSM on_frame_fault from kSafeMode: precondition guard + FIZ-3 packet
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-10 — fsm_on_frame_fault_from_kSafeMode — FIZ-3 telemetry, stays in kSafeMode", "[swifi][platform]") {
  CaptureTelemetryHal<4> capture;
  SignalChainFsm fsm(capture);

  // Walk FSM into kSafeMode via a POST-fail legal path
  REQUIRE(fsm.transition(SignalChainState::kPostRunning) == true);
  REQUIRE(fsm.transition(SignalChainState::kSafeMode) == true); // POST fail → Zone 3
  REQUIRE(fsm.state() == SignalChainState::kSafeMode);
  capture.reset(); // clear setup packets

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
TEST_CASE("[SW-SAF-04] SWIFI: post_step2_lut_corrupt — run_post returns {kFaultDetected, 2}", "[swifi][platform]") {
  // Inject a bit flip into one LDPC matrix
  kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

  const PostResult result = run_post();
  kLdpc_sf2_a.clear_fault(); // always restore, even if CHECK fails

  REQUIRE(result.status == PostStatus::kFaultDetected);
  REQUIRE(result.failed_step == 2U);
}

// ---------------------------------------------------------------------------
// [checksum-golden] — print golden checksum values (one-time helper)
// Used by `task compute-golden-checksums` to populate codebook_checksums.hpp.
// Run: ./lunalink_swifi_tests "[checksum-golden]" --reporter console
// ---------------------------------------------------------------------------
TEST_CASE("checksum-golden: print BCH, Weil, and Legendre codebook checksums",
          "[checksum-golden]") {
  const uint64_t bch = bch_codebook_checksum();
  const uint64_t w10 = weil10230_codebook_checksum();
  const uint64_t w15 = weil1500_codebook_checksum();
  const uint64_t leg10223 = legendre_10223_checksum();
  const uint64_t leg1499 = legendre_1499_checksum();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  (void)std::printf("LUNALINK_BCH_CODEBOOK_CHECKSUM  = 0x%016llxULL\n"
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
  CHECK(leg1499 == kLegendre1499Checksum);
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
  const uint64_t leg1499 = legendre_1499_checksum();
  CHECK(bch == kBchCodebookChecksum);
  CHECK(w10 == kWeil10230Checksum);
  CHECK(w15 == kWeil1500Checksum);
  CHECK(leg10223 == kLegendre10223Checksum);
  CHECK(leg1499 == kLegendre1499Checksum);
  // Nominal POST must pass when all checksums match.
  const PostResult result = run_post();
  REQUIRE(result.status == PostStatus::kPass);
}
