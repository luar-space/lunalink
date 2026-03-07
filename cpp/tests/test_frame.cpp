#include "lunalink/signal/frame.hpp"
#include "lunalink/signal/bch.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <algorithm>

using namespace lunalink::signal;

TEST_CASE("frame_build_partial returns kOk for valid inputs") {
  std::array<uint8_t, kFrameLength> frame{};
  CHECK(frame_build_partial(Fid::kNode1, Toi(0), frame) == FrameStatus::kOk);
  CHECK(frame_build_partial(Fid::kNode4, Toi(99), frame) == FrameStatus::kOk);
}

TEST_CASE("frame_build_partial SB1 region matches standalone bch_encode") {
  std::array<uint8_t, kFrameLength> frame{};
  std::array<uint8_t, kBchCodewordLength> expected_bch{};
  
  const Fid fid = Fid::kNode2;
  const Toi toi{42};
  
  REQUIRE(bch_encode(fid, toi, expected_bch) == BchStatus::kOk);
  REQUIRE(frame_build_partial(fid, toi, frame) == FrameStatus::kOk);
  
  auto sb1_region = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
  CHECK(std::ranges::equal(sb1_region, expected_bch));
}

TEST_CASE("frame_build_partial SB1 with spec test vector (FID=0, TOI=69)") {
  constexpr std::array<uint8_t, kBchCodewordLength> expected_bch = {
      0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1,
      1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
  };

  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(Fid::kNode1, Toi(69), frame) == FrameStatus::kOk);

  auto sb1_region = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
  CHECK(std::ranges::equal(sb1_region, expected_bch));
}

TEST_CASE("frame_build_partial SB2-SB4 region is zero-padded") {
  std::array<uint8_t, kFrameLength> frame{};
  frame.fill(0xFF);
  REQUIRE(frame_build_partial(Fid::kNode1, Toi(0), frame) == FrameStatus::kOk);

  const auto payload_start = static_cast<uint16_t>(kSyncLength + kSb1Length);
  for (uint16_t i = payload_start; i < kFrameLength; ++i) {
    REQUIRE(frame[i] == 0U);
  }
}

TEST_CASE("frame_build_partial exhaustive FID x TOI") {
  std::array<uint8_t, kFrameLength> frame{};
  for (uint8_t fid_val = 0; fid_val < 4; ++fid_val) {
    for (uint8_t toi_val = 0; toi_val < 100; ++toi_val) {
      REQUIRE(frame_build_partial(static_cast<Fid>(fid_val), Toi(toi_val), frame) == FrameStatus::kOk);
    }
  }
}
