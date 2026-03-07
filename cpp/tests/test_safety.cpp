#include "lunalink/signal/safety.hpp"
#include "lunalink/signal/bch.hpp"
#include "lunalink/signal/prn.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

using namespace lunalink::signal;

TEST_CASE("CheckedRange: saturation boundaries") {
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

TEST_CASE("TmrValue: voting logic (2-of-3) and active repair") {
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

TEST_CASE("Fid/Toi/PrnId: repair coverage") {
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

TEST_CASE("secure_scrub: verify memory is zeroed") {
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

TEST_CASE("wip_tick: coverage") {
    wip_tick();
    wip_tick();
    SUCCEED("wip_tick executed without crash");
}

TEST_CASE("CheckedRange: Edge cases") {
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

TEST_CASE("TmrValue: equality operator") {
    TmrValue<uint8_t> v1(10);
    TmrValue<uint8_t> v2(10);
    TmrValue<uint8_t> v3(20);
    
    CHECK(v1 == v2);
    CHECK_FALSE(v1 == v3);
}

TEST_CASE("Fid: NODE constants") {
    CHECK(Fid::kNode1().value() == 0);
    CHECK(Fid::kNode2().value() == 1);
    CHECK(Fid::kNode3().value() == 2);
    CHECK(Fid::kNode4().value() == 3);
}

TEST_CASE("TmrValue: Default constructor coverage") {
    TmrValue<uint32_t> v;
    CHECK(v.vote() == 0);
}

TEST_CASE("TmrValue: no majority scenario") {
    TmrValue<uint8_t> v;
    v.v1 = 1;
    v.v2 = 2;
    v.v3 = 3;
    // Should return v1 as fallback
    CHECK(v.vote() == 1);
}

TEST_CASE("CheckedRange: force invalid via memory tamper") {
    CheckedRange<uint8_t, 0, 3> r(0);
    uint8_t bad = 10;
    std::memcpy(&r, &bad, 1);
    CHECK(r.value() == 10); // value() just returns value_ without re-saturating
}
