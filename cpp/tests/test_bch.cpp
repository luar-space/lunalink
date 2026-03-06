#include "lunalink/signal/bch.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Hamming weight (popcount) of a symbol array.
uint16_t hamming_weight(const std::array<uint8_t, kBchCodewordLength>& cw) {
  uint16_t w = 0;
  for (auto v : cw) {
    w = static_cast<uint16_t>(w + v);
  }
  return w;
}

/// Hamming distance between two codewords.
uint16_t hamming_distance(const std::array<uint8_t, kBchCodewordLength>& a,
                          const std::array<uint8_t, kBchCodewordLength>& b) {
  uint16_t d = 0;
  for (uint8_t i = 0; i < kBchCodewordLength; ++i) {
    if (a[i] != b[i]) {
      d = static_cast<uint16_t>(d + 1U);
    }
  }
  return d;
}

} // namespace

// ---------------------------------------------------------------------------
// Spec test vector — Figure 8 of LSIS V1.0 §2.4.2.1
// SB1 data = 0x045 (FID=0, TOI=69), encoded field = 0x229f61dbb84a0
// ---------------------------------------------------------------------------
TEST_CASE("bch_encode matches spec Figure 8 test vector") {
  // 0x229f61dbb84a0 as 52 individual bits (MSB first)
  constexpr std::array<uint8_t, kBchCodewordLength> expected = {
      0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1,
      1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
  };

  std::array<uint8_t, kBchCodewordLength> out{};
  const auto status = bch_encode(0, 69, out.data(), out.size());

  REQUIRE(status == BchStatus::kOk);
  REQUIRE(out == expected);
}

TEST_CASE("bch_encode with bit0=1 XORs all LFSR outputs") {
  // FID=2 (bit0=1), TOI=69 — same 8 data bits as Figure 8 but bit0 flips.
  std::array<uint8_t, kBchCodewordLength> out_bit0_0{};
  std::array<uint8_t, kBchCodewordLength> out_bit0_1{};

  REQUIRE(bch_encode(0, 69, out_bit0_0.data(), out_bit0_0.size()) == BchStatus::kOk);
  REQUIRE(bch_encode(2, 69, out_bit0_1.data(), out_bit0_1.size()) == BchStatus::kOk);

  // Bit 0 should be raw MSB of FID.
  REQUIRE(out_bit0_0[0] == 0);
  REQUIRE(out_bit0_1[0] == 1);

  // All remaining 51 symbols should be flipped.
  for (uint8_t i = 1; i < kBchCodewordLength; ++i) {
    REQUIRE(out_bit0_1[i] == static_cast<uint8_t>(out_bit0_0[i] ^ 1U));
  }
}

TEST_CASE("bch_encode all four FID values produce valid distinct codewords") {
  // FID 0-3 with same TOI=42 — exercises all bit0/bit1 combinations.
  std::array<std::array<uint8_t, kBchCodewordLength>, 4> cws{};
  for (uint8_t fid = 0; fid < 4; ++fid) {
    REQUIRE(bch_encode(fid, 42, cws[fid].data(), cws[fid].size()) == BchStatus::kOk);
    // Verify binary output.
    for (auto v : cws[fid]) {
      REQUIRE(v <= 1U);
    }
    // Verify bit0 matches FID MSB.
    REQUIRE(cws[fid][0] == static_cast<uint8_t>(fid >> 1U));
  }
  // All four must be distinct.
  for (uint8_t i = 0; i < 4; ++i) {
    for (uint8_t j = static_cast<uint8_t>(i + 1U); j < 4; ++j) {
      REQUIRE(cws[i] != cws[j]);
    }
  }
}

TEST_CASE("bch_encode output values are binary") {
  std::array<uint8_t, kBchCodewordLength> out{};
  REQUIRE(bch_encode(0, 0, out.data(), out.size()) == BchStatus::kOk);
  for (auto v : out) {
    REQUIRE(v <= 1U);
  }
}

TEST_CASE("bch_encode all-zero SB1") {
  // FID=0, TOI=0 → SB1 = 000000000, bit0=0, data=00000000
  // All-zero LFSR state → all-zero output.
  std::array<uint8_t, kBchCodewordLength> out{};
  REQUIRE(bch_encode(0, 0, out.data(), out.size()) == BchStatus::kOk);
  for (auto v : out) {
    REQUIRE(v == 0U);
  }
}

TEST_CASE("bch_encode rejects null output") {
  REQUIRE(bch_encode(0, 0, nullptr, kBchCodewordLength) == BchStatus::kNullOutput);
}

TEST_CASE("bch_encode rejects undersized output buffer") {
  std::array<uint8_t, kBchCodewordLength - 1U> out{};
  REQUIRE(bch_encode(0, 0, out.data(), out.size()) == BchStatus::kOutputTooSmall);
}

TEST_CASE("bch_encode rejects invalid FID") {
  std::array<uint8_t, kBchCodewordLength> out{};
  REQUIRE(bch_encode(4, 0, out.data(), out.size()) == BchStatus::kInvalidFid);
}

TEST_CASE("bch_encode rejects invalid TOI") {
  std::array<uint8_t, kBchCodewordLength> out{};
  REQUIRE(bch_encode(0, 100, out.data(), out.size()) == BchStatus::kInvalidToi);
  REQUIRE(bch_encode(0, 127, out.data(), out.size()) == BchStatus::kInvalidToi);
}

TEST_CASE("bch_encode boundary TOI=99 succeeds") {
  std::array<uint8_t, kBchCodewordLength> out{};
  REQUIRE(bch_encode(0, 99, out.data(), out.size()) == BchStatus::kOk);
}

TEST_CASE("bch_encode different inputs produce different codewords") {
  std::array<uint8_t, kBchCodewordLength> out_a{};
  std::array<uint8_t, kBchCodewordLength> out_b{};
  REQUIRE(bch_encode(0, 1, out_a.data(), out_a.size()) == BchStatus::kOk);
  REQUIRE(bch_encode(0, 2, out_b.data(), out_b.size()) == BchStatus::kOk);
  REQUIRE(out_a != out_b);
}

// ---------------------------------------------------------------------------
// Structural code property: minimum Hamming distance >= 5 (t=2 correction).
//
// Exhaustive over all valid inputs: FID 0-3 × TOI 0-99 = 400 codewords.
// BCH(51,8) with the prepended
// bit0 yields a (52,9) code. The minimum distance between any two distinct
// codewords of a linear-like code equals the minimum weight of any non-zero
// codeword (when codewords form a coset). We check both min distance and min
// weight to be safe.
// ---------------------------------------------------------------------------
TEST_CASE("bch_encode minimum Hamming distance >= 5 over all valid inputs") {
  // Generate all valid codewords.
  struct Entry {
    std::array<uint8_t, kBchCodewordLength> cw;
  };
  std::vector<Entry> codewords;
  codewords.reserve(400);

  for (uint8_t fid = 0; fid < 4; ++fid) {
    for (uint8_t toi = 0; toi < 100; ++toi) {
      Entry e{};
      REQUIRE(bch_encode(fid, toi, e.cw.data(), e.cw.size()) == BchStatus::kOk);
      codewords.push_back(e);
    }
  }
  REQUIRE(codewords.size() == 400);

  // Check minimum non-zero weight.
  uint16_t min_weight = kBchCodewordLength;
  for (const auto& e : codewords) {
    const auto w = hamming_weight(e.cw);
    if (w > 0 && w < min_weight) {
      min_weight = w;
    }
  }
  REQUIRE(min_weight >= 5);

  // Check pairwise minimum distance (400 choose 2 = 79800 pairs — fast).
  uint16_t min_dist = kBchCodewordLength;
  for (size_t i = 0; i < codewords.size(); ++i) {
    for (size_t j = i + 1; j < codewords.size(); ++j) {
      const auto d = hamming_distance(codewords[i].cw, codewords[j].cw);
      if (d < min_dist) {
        min_dist = d;
      }
    }
  }
  REQUIRE(min_dist >= 5);
}
