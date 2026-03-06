#include "lunalink/signal/frame.hpp"

#include "lunalink/signal/bch.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// Basic frame assembly
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial returns kOk for valid inputs") {
  std::array<uint8_t, kFrameLength> out{};
  REQUIRE(frame_build_partial(0, 0, out) == FrameStatus::kOk);
}

TEST_CASE("frame_build_partial frame length is 6000 symbols") {
  // Static assertion via the constant.
  static_assert(kFrameLength == 6000, "kFrameLength must be 6000");
  static_assert(kSyncLength + kSb1Length + kPayloadLength == kFrameLength,
                "sync + SB1 + payload must equal total frame");
}

// ---------------------------------------------------------------------------
// Sync pattern verification
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial starts with 68-symbol sync pattern") {
  std::array<uint8_t, kFrameLength> out{};
  REQUIRE(frame_build_partial(0, 0, out) == FrameStatus::kOk);

  // First 68 symbols must match kSyncPattern exactly.
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    REQUIRE(out[i] == kSyncPattern[i]);
  }
}

TEST_CASE("sync pattern matches hex CC63F74536F49E04A") {
  // Verify the first and last nibbles of the sync word directly.
  // 0xC = 1100
  REQUIRE(kSyncPattern[0] == 1);
  REQUIRE(kSyncPattern[1] == 1);
  REQUIRE(kSyncPattern[2] == 0);
  REQUIRE(kSyncPattern[3] == 0);

  // Last nibble 0xA = 1010 (4 bits only: positions 64-67).
  REQUIRE(kSyncPattern[64] == 1);
  REQUIRE(kSyncPattern[65] == 0);
  REQUIRE(kSyncPattern[66] == 1);
  REQUIRE(kSyncPattern[67] == 0);
}

TEST_CASE("sync pattern values are binary") {
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    REQUIRE(kSyncPattern[i] <= 1U);
  }
}

// ---------------------------------------------------------------------------
// SB1 (BCH-encoded) region
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial SB1 region matches standalone bch_encode") {
  constexpr uint8_t fid = 1;
  constexpr uint8_t toi = 42;

  // Build frame.
  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(fid, toi, frame) == FrameStatus::kOk);

  // Standalone BCH encode.
  std::array<uint8_t, kBchCodewordLength> bch{};
  REQUIRE(bch_encode(fid, toi, bch) == BchStatus::kOk);

  // SB1 starts at offset kSyncLength (68) and runs for kSb1Length (52).
  for (uint8_t i = 0; i < kSb1Length; ++i) {
    REQUIRE(frame[kSyncLength + i] == bch[i]);
  }
}

TEST_CASE("frame_build_partial SB1 with spec test vector (FID=0, TOI=69)") {
  // BCH test vector from LSIS V1.0 Figure 8.
  constexpr std::array<uint8_t, kBchCodewordLength> expected_bch = {
      0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1,
      1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
  };

  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(0, 69, frame) == FrameStatus::kOk);

  for (uint8_t i = 0; i < kSb1Length; ++i) {
    REQUIRE(frame[kSyncLength + i] == expected_bch[i]);
  }
}

// ---------------------------------------------------------------------------
// SB2-SB4 zero-padded region
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial SB2-SB4 region is zero-padded") {
  std::array<uint8_t, kFrameLength> frame{};
  // Fill with 0xFF to detect any non-zero symbols.
  frame.fill(0xFF);

  REQUIRE(frame_build_partial(0, 0, frame) == FrameStatus::kOk);

  const auto payload_start = static_cast<uint16_t>(kSyncLength + kSb1Length);
  for (uint16_t i = payload_start; i < kFrameLength; ++i) {
    REQUIRE(frame[i] == 0U);
  }
}

TEST_CASE("frame_build_partial payload region is exactly 5880 symbols") {
  static_assert(kPayloadLength == 5880, "payload must be 5880 symbols");
  static_assert(kPayloadLength == kInterleaverRows * kInterleaverCols,
                "payload must equal 60 × 98");
}

// ---------------------------------------------------------------------------
// Output values are binary
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial all output values are binary {0, 1}") {
  // Test across several FID/TOI combinations.
  for (uint8_t fid = 0; fid < 4; ++fid) {
    std::array<uint8_t, kFrameLength> frame{};
    REQUIRE(frame_build_partial(fid, 50, frame) == FrameStatus::kOk);
    for (uint16_t i = 0; i < kFrameLength; ++i) {
      REQUIRE(frame[i] <= 1U);
    }
  }
}

// ---------------------------------------------------------------------------
// Frame timing constants
// ---------------------------------------------------------------------------
TEST_CASE("frame timing constants are consistent") {
  static_assert(kSymbolRate == 500, "symbol rate must be 500 sps");
  static_assert(kFrameDurationS == 12, "frame duration must be 12 s");
  static_assert(kFrameLength == kSymbolRate * kFrameDurationS,
                "6000 = 500 × 12");
}

// ---------------------------------------------------------------------------
// Different FID/TOI produce different frames
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial different FID/TOI produce different frames") {
  std::array<uint8_t, kFrameLength> frame_a{};
  std::array<uint8_t, kFrameLength> frame_b{};

  REQUIRE(frame_build_partial(0, 1, frame_a) == FrameStatus::kOk);
  REQUIRE(frame_build_partial(0, 2, frame_b) == FrameStatus::kOk);

  // Sync region must be identical.
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    REQUIRE(frame_a[i] == frame_b[i]);
  }

  // SB1 region must differ (different TOI).
  bool sb1_differs = false;
  for (uint8_t i = 0; i < kSb1Length; ++i) {
    if (frame_a[kSyncLength + i] != frame_b[kSyncLength + i]) {
      sb1_differs = true;
      break;
    }
  }
  REQUIRE(sb1_differs);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial rejects undersized buffer") {
  std::array<uint8_t, kFrameLength - 1U> small{};
  REQUIRE(frame_build_partial(0, 0, small) == FrameStatus::kOutputTooSmall);
}

TEST_CASE("frame_build_partial propagates BCH errors for invalid FID") {
  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(4, 0, frame) == FrameStatus::kInvalidFid);
}

TEST_CASE("frame_build_partial propagates BCH errors for invalid TOI") {
  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(0, 100, frame) == FrameStatus::kInvalidToi);
}

// ---------------------------------------------------------------------------
// Boundary values
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial boundary FID=3 TOI=99") {
  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(3, 99, frame) == FrameStatus::kOk);

  // Binary output check.
  for (uint16_t i = 0; i < kFrameLength; ++i) {
    REQUIRE(frame[i] <= 1U);
  }
}

TEST_CASE("frame_build_partial exact-size buffer succeeds") {
  std::array<uint8_t, kFrameLength> frame{};
  REQUIRE(frame_build_partial(0, 0, frame) == FrameStatus::kOk);
}

TEST_CASE("frame_build_partial oversized buffer succeeds") {
  std::array<uint8_t, kFrameLength + 100U> frame{};
  frame.fill(0xAA);
  // Must subspan to the exact expected fixed size.
  auto frame_view = std::span<uint8_t, kFrameLength>(frame.data(), kFrameLength);
  REQUIRE(frame_build_partial(1, 50, frame_view) == FrameStatus::kOk);

  // Verify content within frame region.
  for (uint16_t i = 0; i < kFrameLength; ++i) {
    REQUIRE(frame[i] <= 1U);
  }
  // Beyond frame length should be untouched.
  for (uint16_t i = kFrameLength; i < kFrameLength + 100U; ++i) {
    REQUIRE(frame[i] == 0xAA);
  }
}

// ---------------------------------------------------------------------------
// Exhaustive — all valid FID × TOI produce kOk and binary output
// ---------------------------------------------------------------------------
TEST_CASE("frame_build_partial exhaustive FID x TOI") {
  std::array<uint8_t, kFrameLength> frame{};
  for (uint8_t fid = 0; fid < 4; ++fid) {
    for (uint8_t toi = 0; toi < 100; ++toi) {
      REQUIRE(frame_build_partial(fid, toi, frame) == FrameStatus::kOk);
      // Spot-check a few positions for binary values.
      REQUIRE(frame[0] <= 1U);
      REQUIRE(frame[kSyncLength] <= 1U);
      REQUIRE(frame[kFrameLength - 1U] <= 1U);
    }
  }
}
