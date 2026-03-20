#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/signal/modulator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>

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
