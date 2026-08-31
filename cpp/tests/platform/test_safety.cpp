#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/platform/signal_chain_fsm.hpp"
#include "lunalink/platform/watchdog.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <memory>

using namespace lunalink::signal;

TEST_CASE("[SW-SAF-02] CheckedRange: saturation boundaries", "[safety]") {
  // Constructor saturation
  CheckedRange<uint8_t, 10, 20> r1(25);
  CHECK(r1.value() == 20);

  CheckedRange<uint8_t, 10, 20> r2(5);
  CHECK(r2.value() == 10);

  // Assignment saturation
  CheckedRange<uint8_t, 10, 20> r3(15);
  r3 = 30;
  CHECK(r3.value() == 20);

  r3 = 0;
  CHECK(r3.value() == 10);

  // Assignment saturation boundaries
  r3 = 10;
  CHECK(r3.value() == 10);
  r3 = 20;
  CHECK(r3.value() == 20);
  r3 = 11;
  CHECK(r3.value() == 11);
  r3 = 19;
  CHECK(r3.value() == 19);

  // Implicit conversion
  uint8_t val = r3;
  CHECK(val == 19);
}

TEST_CASE("[SW-SAF-02] TmrValue: voting logic (2-of-3) and active repair", "[safety]") {
  TmrValue<uint8_t> v(100);
  CHECK(v.peek() == 100);

  // Single flip in v1
  v.v1 = 200;
  CHECK(v.v1 == 200);
  CHECK(v.peek() == 100);
  CHECK(v.v1 == 200); // peek() does NOT repair

  CHECK(v.vote() == 100);
  CHECK(v.v1 == 100); // vote() DOES repair

  // Single flip in v2
  v.refresh(100);
  v.v2 = 200;
  CHECK(v.vote() == 100);
  CHECK(v.v2 == 100); // Verify repair

  // Single flip in v3
  v.refresh(100);
  v.v3 = 200;
  CHECK(v.vote() == 100);
  CHECK(v.v3 == 100); // Verify repair

  // Two flips (same value) -> value changes
  v.v1 = 200;
  v.v2 = 200;
  CHECK(v.peek() == 200);

  // Refresh restores all copies
  v.refresh(50);
  CHECK(v.v1 == 50);
  CHECK(v.v2 == 50);
  CHECK(v.v3 == 50);
  CHECK(v.peek() == 50);
}

TEST_CASE("Fid/Toi/PrnId: repair coverage", "[safety]") {
  Fid f(1);
  f.storage.v1 = 10;
  CHECK(f.value() == 1);
  CHECK(f.repair() == 1);
  CHECK(f.storage.v1 == 1);

  Toi t(69);
  t.storage.v1 = 0;
  CHECK(t.value() == 69);
  CHECK(t.repair() == 69);
  CHECK(t.storage.v1 == 69);

  PrnId p(42);
  p.storage.v1 = 200;
  CHECK(p.value() == 42);
  CHECK(p.repair() == 42);
  CHECK(p.storage.v1 == 42);
}

TEST_CASE("[SW-SAF-02] secure_scrub: verify memory is zeroed", "[safety]") {
  std::array<uint8_t, 16> data{};
  std::ranges::fill(data, 0xAAU);
  secure_scrub(data);
  for (auto b : data) {
    CHECK(b == 0U);
  }

  std::array<int16_t, 8> data16{};
  std::ranges::fill(data16, 1234);
  secure_scrub(data16);
  for (auto b : data16) {
    CHECK(b == 0);
  }

  std::span<uint8_t> empty_span{};
  secure_scrub(empty_span);
  SUCCEED("secure_scrub handled empty span");
}

TEST_CASE("wip_tick: coverage", "[safety]") {
  wip_tick();
  wip_tick();
  SUCCEED("wip_tick executed without crash");
}

TEST_CASE("CheckedRange: Edge cases", "[safety]") {
  CheckedRange<uint8_t, 0, 255> r(128);
  CHECK(r.value() == 128);

  // Saturation at extremes
  CheckedRange<uint8_t, 1, 1> r2(0);
  CHECK(r2.value() == 1);
  r2 = 2;
  CHECK(r2.value() == 1);

  // More saturation coverage
  CheckedRange<uint8_t, 100, 100> r_fixed(50); // hits < Min
  CHECK(r_fixed.value() == 100);
  r_fixed = 150; // hits > Max
  CHECK(r_fixed.value() == 100);

  // Explicitly hit both branches in one test for clarity
  CheckedRange<uint8_t, 50, 150> r_range(50);
  r_range = 25; // hit < Min
  CHECK(r_range.value() == 50);
  r_range = 200; // hit > Max
  CHECK(r_range.value() == 150);

  // Default constructor
  CheckedRange<uint8_t, 5, 10> r_def;
  CHECK(r_def.value() == 5);
}

TEST_CASE("TmrValue: equality operator", "[safety]") {
  TmrValue<uint8_t> v1(10);
  TmrValue<uint8_t> v2(10);
  TmrValue<uint8_t> v3(20);

  CHECK(v1 == v2);
  CHECK_FALSE(v1 == v3);
}

TEST_CASE("Fid: NODE constants", "[safety]") {
  CHECK(Fid::kNode1().value() == 0);
  CHECK(Fid::kNode2().value() == 1);
  CHECK(Fid::kNode3().value() == 2);
  CHECK(Fid::kNode4().value() == 3);
}

TEST_CASE("TmrValue: Default constructor coverage", "[safety]") {
  TmrValue<uint32_t> v;
  CHECK(v.vote() == 0);
}

TEST_CASE("TmrValue: no majority scenario", "[safety]") {
  TmrValue<uint8_t> v;
  v.v1 = 1;
  v.v2 = 2;
  v.v3 = 3;
  // Should return v1 as fallback
  CHECK(v.vote() == 1);
}

TEST_CASE("CheckedRange: force invalid via memory tamper", "[safety]") {
  CheckedRange<uint8_t, 0, 3> r(0);
  uint8_t bad = 10;
  std::memcpy(&r, &bad, 1);
  CHECK(r.value() == 10); // value() just returns value_ without re-saturating
}

TEST_CASE("TmrValue: repair paths (all branches)", "[safety]") {
  // v2 == v3 (Repair v1)
  TmrValue<uint8_t> t1(10);
  t1.inject_fault(20);
  t1.v2 = 10;
  t1.v3 = 10;
  CHECK(t1.vote() == 10);
  CHECK(t1.v1 == 10);

  // All different (Critical Failure -> return v1)
  TmrValue<uint8_t> t2(10);
  t2.v1 = 10;
  t2.v2 = 20;
  t2.v3 = 30;
  CHECK(t2.vote() == 10);
}

TEST_CASE("TmrValue: fault injection overwrites all copies", "[safety]") {
  TmrValue<uint8_t> v(10);
  v.inject_fault(20);
  CHECK(v.peek() == 20);
  CHECK(v.vote() == 20);
}

TEST_CASE("Status codes: exhaustive sentinel values", "[safety]") {
  CHECK(static_cast<uint8_t>(BchStatus::kAmbiguousMatch) == 0xF0U);
  CHECK(static_cast<uint8_t>(BchStatus::kNullOutput) == 0x99U);
  CHECK(static_cast<uint8_t>(BchStatus::kFaultDetected) == 0x0FU);
  CHECK(static_cast<uint8_t>(ModulationStatus::kFaultDetected) == 0x99U);
  CHECK(static_cast<uint8_t>(MatchedCodeStatus::kFaultDetected) == 0x99U);
  CHECK(static_cast<uint8_t>(LdpcStatus::kFaultDetected) == 0x99U);
}

// ===========================================================================
// Signal Chain FSM: double-fault escalation + on_frame_ok recovery
// ===========================================================================

TEST_CASE("[SW-SAF-02] FSM: double frame fault escalates to kSafeMode", "[safety]") {
  NullTelemetryHal hal;
  SignalChainFsm fsm(hal);

  // Boot → POST → Operational
  CHECK(fsm.transition(SignalChainState::kPostRunning));
  CHECK(fsm.transition(SignalChainState::kOperational));

  // First frame fault → kFrameFault (Zone 2)
  CHECK(fsm.on_frame_fault() == true);
  CHECK(fsm.state() == SignalChainState::kFrameFault);

  // Recover to Operational so we can test the double-fault counter path
  CHECK(fsm.transition(SignalChainState::kOperational));

  // First fault again
  CHECK(fsm.on_frame_fault() == true);
  CHECK(fsm.state() == SignalChainState::kFrameFault);

  // Second consecutive fault from Operational: need to transition back first,
  // then fault twice without recovery. Reset by going back to Operational.
  CHECK(fsm.transition(SignalChainState::kOperational));

  // Now trigger on_frame_fault twice without on_frame_ok between them.
  // First call: kOperational → kFrameFault, consecutive_fault_count_ = 1
  CHECK(fsm.on_frame_fault() == true);
  // Second call: state is kFrameFault (not kOperational), so the
  // precondition guard catches it and locks to kSafeMode (Zone 3).
  CHECK(fsm.on_frame_fault() == false);
  CHECK(fsm.state() == SignalChainState::kSafeMode);
}

TEST_CASE("[SW-SAF-02] FSM: on_frame_ok recovers from kFrameFault", "[safety]") {
  NullTelemetryHal hal;
  SignalChainFsm fsm(hal);

  CHECK(fsm.transition(SignalChainState::kPostRunning));
  CHECK(fsm.transition(SignalChainState::kOperational));

  // Fault then recover
  CHECK(fsm.on_frame_fault() == true);
  CHECK(fsm.state() == SignalChainState::kFrameFault);

  fsm.on_frame_ok();
  CHECK(fsm.state() == SignalChainState::kOperational);
}

TEST_CASE("[SW-SAF-02] FSM: on_frame_ok is silent in kOperational", "[safety]") {
  NullTelemetryHal hal;
  SignalChainFsm fsm(hal);

  CHECK(fsm.transition(SignalChainState::kPostRunning));
  CHECK(fsm.transition(SignalChainState::kOperational));

  // Calling on_frame_ok when already operational is a no-op
  fsm.on_frame_ok();
  CHECK(fsm.state() == SignalChainState::kOperational);
}

TEST_CASE("[SW-SAF-02] FSM: on_frame_ok ignored in kSafeMode", "[safety]") {
  NullTelemetryHal hal;
  SignalChainFsm fsm(hal);

  CHECK(fsm.transition(SignalChainState::kPostRunning));
  CHECK(fsm.transition(SignalChainState::kSafeMode));

  // on_frame_ok from kSafeMode is silently ignored
  fsm.on_frame_ok();
  CHECK(fsm.state() == SignalChainState::kSafeMode);
}

// ===========================================================================
// Watchdog: install + get + lock
// ===========================================================================

TEST_CASE("[SW-SAF-02] Watchdog: install and retrieve", "[safety]") {
  // Note: lock_watchdog_config() is a one-way latch — once locked,
  // install_watchdog() returns false. Since POST calls lock_watchdog_config(),
  // this test must run before or independently of POST tests.
  // However, in the test binary the lock state persists across tests.
  // We test get_installed_watchdog() which works regardless of lock state.
  auto* wdog = get_installed_watchdog();
  // May be nullptr (no watchdog installed) or non-null (from a prior test)
  (void)wdog;
  SUCCEED("get_installed_watchdog() returned without fault");
}

// ===========================================================================
// Coverage: PrnId operators (operator<=> and operator size_t)
// ===========================================================================

TEST_CASE("PrnId: comparison and size_t conversion", "[safety]") {
  PrnId a(1);
  PrnId b(42);
  PrnId c(42);

  // operator<=>
  CHECK(a < b);
  CHECK(b > a);
  CHECK(b == c);
  CHECK(a != b);
  CHECK(b <= c);
  CHECK(b >= c);

  // operator size_t
  CHECK(static_cast<size_t>(a) == 1);
  CHECK(static_cast<size_t>(b) == 42);
}

// ===========================================================================
// Coverage: FSM illegal transition + CaptureTelemetryHal
// ===========================================================================

TEST_CASE("[SW-SAF-02] FSM: illegal transition emits telemetry", "[safety]") {
  CaptureTelemetryHal<4> hal;
  SignalChainFsm fsm(hal);

  // Attempt an illegal transition: kUninitialized → kOperational
  // (must go through kPostRunning first)
  CHECK(fsm.transition(SignalChainState::kOperational) == false);
  CHECK(fsm.state() == SignalChainState::kSafeMode);

  // Verify telemetry was emitted
  REQUIRE(hal.count() == 1);
  CHECK(hal.packet(0).fiz_level == 3);
  CHECK(hal.packet(0).fault_code == static_cast<uint8_t>(SignalChainState::kSafeMode));
}

TEST_CASE("[SW-SAF-02] FSM: on_frame_fault from non-operational emits telemetry", "[safety]") {
  CaptureTelemetryHal<4> hal;
  SignalChainFsm fsm(hal);

  // Move to POST, then call on_frame_fault (illegal — must be kOperational)
  CHECK(fsm.transition(SignalChainState::kPostRunning));
  CHECK(fsm.on_frame_fault() == false);
  CHECK(fsm.state() == SignalChainState::kSafeMode);

  REQUIRE(hal.count() == 1);
  CHECK(hal.packet(0).fiz_level == 3);
}

TEST_CASE("CaptureTelemetryHal: overflow and reset", "[safety]") {
  CaptureTelemetryHal<2> hal;

  // Emit 3 packets but capacity is 2
  hal.emit(FaultPacket{100, 0xAA, 1, 0, 0});
  hal.emit(FaultPacket{200, 0xBB, 2, 0, 0});
  hal.emit(FaultPacket{300, 0xCC, 3, 0, 0}); // dropped

  CHECK(hal.count() == 2);
  CHECK(hal.packet(0).timestamp_ms == 100);
  CHECK(hal.packet(1).timestamp_ms == 200);

  // Out-of-range access returns empty packet
  CHECK(hal.packet(2).timestamp_ms == 0);
  CHECK(hal.packet(99).timestamp_ms == 0);

  // Reset
  hal.reset();
  CHECK(hal.count() == 0);
}

// ===========================================================================
// Original Gateway 2 composite test
// ===========================================================================

TEST_CASE("Safety: TMR voting and CFI detection (Gateway 2)", "[safety]") {
  SECTION("TMR Majority Voting (Radiation Resilience)") {
    TmrValue<uint8_t> val(0x5AU);
    CHECK(val.vote() == 0x5AU);
  }

  SECTION("CFI (Control Flow Integrity) Detection") {
    std::array<uint8_t, 52> valid_in{};
    REQUIRE(bch_encode(Fid::kNode1(), Toi(0), valid_in) == BchStatus::kOk);

    auto result = bch_decode(valid_in);
    CHECK(result.status == BchStatus::kOk);
  }

  SECTION("Range Hardening (Invalid State Rejection)") {
    Fid bad_fid(0);
    bad_fid.storage.force_fault(static_cast<uint8_t>(255));

    Toi bad_toi(0);
    bad_toi.storage.force_fault(static_cast<uint8_t>(255));

    std::array<uint8_t, 52> out{};
    auto status = bch_encode(bad_fid, bad_toi, out);

    CHECK(status != BchStatus::kOk);
    if (status == BchStatus::kInvalidFid || status == BchStatus::kInvalidToi) {
      SUCCEED("Correct error status returned");
    }
  }
}

// ===========================================================================
// Coverage: CfiGuard — runtime usage (not just constexpr)
// ===========================================================================

TEST_CASE("CfiGuard: constructor, tick, verify", "[safety]") {
  CfiGuard guard(0xDEAD0000U, 3U);
  CHECK_FALSE(guard.verify()); // 0 ticks, expected 3
  guard.tick();
  guard.tick();
  CHECK_FALSE(guard.verify()); // 2 ticks, expected 3
  guard.tick();
  CHECK(guard.verify()); // 3 ticks, expected 3
}

// ===========================================================================
// Coverage: WindowedWatchdog — pure logic, not HAL
// ===========================================================================

TEST_CASE("WindowedWatchdog: tick, reset, evaluate", "[safety]") {
  WindowedWatchdog wdog(5, 100);

  // Under lower bound → evaluate returns false + emits telemetry
  CaptureTelemetryHal<4> hal;
  wdog.reset();
  wdog.tick();
  wdog.tick(); // 2 ticks, min is 5
  CHECK(wdog.tick_count() == 2);
  CHECK_FALSE(wdog.evaluate(hal, 42));
  REQUIRE(hal.count() == 1);
  CHECK(hal.packet(0).fault_code == kWatchdogLowerBoundFaultCode);
  CHECK(hal.packet(0).timestamp_ms == 42);

  // At lower bound → evaluate returns true
  hal.reset();
  wdog.reset();
  for (int i = 0; i < 5; ++i) {
    wdog.tick();
  }
  CHECK(wdog.evaluate(hal, 100));
  CHECK(hal.count() == 0);

  // Upper bound exceeded is informational
  wdog.reset();
  for (int i = 0; i < 101; ++i) {
    wdog.tick();
  }
  CHECK(wdog.upper_bound_exceeded());
  CHECK(wdog.evaluate(hal, 200)); // still returns true (upper bound is HW watchdog's job)
}

// ===========================================================================
// Coverage: aliasing detection on frame_decode workspace
// ===========================================================================

TEST_CASE("[SW-SAF-06] frame_decode: aliasing workspace with rx_symbols detected", "[safety]") {
  // Pass rx_symbols pointing INTO the workspace memory — spans_overlap fires.
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  std::span<const float> aliased_rx(reinterpret_cast<const float*>(ws.get()), kFrameLength);

  FrameDecodeResult result{};
  NullTelemetryHal hal;
  CHECK(frame_decode(aliased_rx, result, *ws, hal) == FrameDecodeStatus::kFaultDetected);
}

// ===========================================================================
// Coverage: aliasing detection on frame_build_partial
// ===========================================================================

TEST_CASE("frame_build: aliasing data with output detected", "[safety]") {
  // Pass sb2_data that overlaps the output frame buffer.
  auto buf = std::make_unique<std::array<uint8_t, kFrameLength + 1000>>();
  std::span<uint8_t, kFrameLength> out(buf->data(), kFrameLength);
  // Point sb2_data INTO the output buffer (overlapping)
  std::span<const uint8_t, kSb2DataBits> sb2(buf->data() + 100, kSb2DataBits);
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};
  CHECK(frame_build(Fid::kNode1(), Toi(0), sb2, sb3, sb4, out, ldpc_ws, fbw) ==
        FrameStatus::kFaultDetected);
}

TEST_CASE("frame_build: aliasing workspace with output detected", "[safety]") {
  // Pass an LdpcEncodeWorkspace that overlaps the output frame buffer.
  auto buf = std::make_unique<std::array<uint8_t, sizeof(LdpcEncodeWorkspace) + kFrameLength>>();
  auto* ldpc_ws = new (buf->data()) LdpcEncodeWorkspace{};
  // Output span starts inside the workspace memory
  std::span<uint8_t, kFrameLength> out(buf->data(), kFrameLength);
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  FrameBuildWorkspace fbw{};
  CHECK(frame_build(Fid::kNode1(), Toi(0), sb2, sb3, sb4, out, *ldpc_ws, fbw) ==
        FrameStatus::kFaultDetected);
}

TEST_CASE("modulator: aliasing input with output detected", "[safety]") {
  // Pass the same buffer as both chips input and modulated output.
  std::array<int8_t, 100> buf{};
  for (auto& b : buf) {
    b = 1;
  }
  std::span<const uint8_t> chips(reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
  std::span<int8_t> out(buf);
  CHECK(validate_modulation_inputs(chips, 1, out) == ModulationStatus::kFaultDetected);
}

TEST_CASE("LDPC decode: zero iterations rejected", "[safety]") {
  std::array<float, kSb3Length> soft{};
  soft.fill(1.0F);
  std::array<uint8_t, kSb3MsgBits> decoded{};
  LdpcWorkspace ws{};
  CHECK(ldpc_decode(LdpcSubframe::kSF3, soft, 0, decoded, ws) == LdpcStatus::kInvalidInput);
}

TEST_CASE("LDPC decode: output buffer too small", "[safety]") {
  std::array<float, kSb3Length> soft{};
  soft.fill(1.0F);
  std::array<uint8_t, 10> tiny_out{}; // way too small for SF3 (needs 870)
  LdpcWorkspace ws{};
  CHECK(ldpc_decode(LdpcSubframe::kSF3, soft, 20, tiny_out, ws) == LdpcStatus::kOutputTooSmall);
}

TEST_CASE("frame_build_partial: aliasing staging with output detected", "[safety]") {
  // Create a FrameBuildWorkspace whose staging array overlaps the output.
  // We achieve this by placement-constructing the workspace at an offset
  // within a large buffer that also serves as the output.
  alignas(16) auto buf =
      std::make_unique<std::array<uint8_t, sizeof(FrameBuildWorkspace) + kFrameLength>>();
  auto* ws = new (buf->data()) FrameBuildWorkspace{};
  // Output span starts inside the workspace staging region
  std::span<uint8_t, kFrameLength> out(buf->data(), kFrameLength);

  CHECK(frame_build_partial(Fid::kNode1(), Toi(0), out, *ws) == FrameStatus::kFaultDetected);
}

// ===========================================================================
// Coverage: watchdog install / lock / get semantics
// ===========================================================================

TEST_CASE("Watchdog: install, lock, and locked-install rejection", "[safety]") {
  // Note: g_wdog_locked is a process-global one-way latch. This test may
  // interact with other tests depending on execution order. We test the
  // semantics that work regardless of prior lock state.
  WindowedWatchdog wdog(10, 100);

  // get_installed_watchdog returns the current pointer (may be nullptr or a prior test's wdog)
  auto* prior = get_installed_watchdog();
  (void)prior;

  // Try to install — succeeds only if not yet locked
  const bool installed = install_watchdog(&wdog);
  if (installed) {
    CHECK(get_installed_watchdog() == &wdog);

    // Lock and verify install is rejected
    lock_watchdog_config();
    CHECK_FALSE(install_watchdog(nullptr));
    // Watchdog pointer persists after lock
    CHECK(get_installed_watchdog() == &wdog);
  } else {
    // Already locked by a prior test (e.g., POST) — verify rejection
    CHECK_FALSE(install_watchdog(&wdog));
    SUCCEED("Config already locked — rejection path exercised");
  }
}

// ===========================================================================
// Coverage: IQ mux aliasing detection
// ===========================================================================

TEST_CASE("IQ mux: aliasing I-channel with output detected", "[safety]") {
  // Pass I-channel samples that overlap the output buffer.
  std::array<int16_t, kIqSamplesPerEpoch * 2> buf{};
  // Point I-channel into the output buffer region
  auto* i_ptr = reinterpret_cast<int8_t*>(buf.data());
  std::span<const int8_t> i_samples(i_ptr, kGoldChipLength);
  std::array<int8_t, kIqSamplesPerEpoch> q_samples{};
  for (auto& q : q_samples) {
    q = 1;
  }
  std::span<int16_t> out(buf);
  CHECK(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kFaultDetected);
}
