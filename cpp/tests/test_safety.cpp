#include "lunalink/signal/safety.hpp"
#include "lunalink/signal/bch.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

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
  
  // Implicit conversion (now that it's restored to implicit)
  uint8_t val = r3;
  CHECK(val == 10);
}

TEST_CASE("TmrValue: voting logic (2-of-3)") {
  TmrValue<uint8_t> v(100);
  CHECK(v.vote() == 100);
  
  // Single flip in v1
  v.v1 = 200;
  CHECK(v.vote() == 100);
  
  // Single flip in v2
  v.refresh(100);
  v.v2 = 200;
  CHECK(v.vote() == 100);
  
  // Single flip in v3
  v.refresh(100);
  v.v3 = 200;
  CHECK(v.vote() == 100);
  
  // Two flips (same value) -> value changes
  v.v1 = 200;
  v.v2 = 200;
  CHECK(v.vote() == 200);
  
  // Refresh restores all copies
  v.refresh(50);
  CHECK(v.v1 == 50);
  CHECK(v.v2 == 50);
  CHECK(v.v3 == 50);
  CHECK(v.vote() == 50);
}

TEST_CASE("CheckedRange: Edge cases") {
    CheckedRange<uint8_t, 0, 255> r(128);
    CHECK(r.value() == 128);
    
    // Saturation at extremes
    CheckedRange<uint8_t, 1, 1> r2(0);
    CHECK(r2.value() == 1);
    r2 = 2;
    CHECK(r2.value() == 1);
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
