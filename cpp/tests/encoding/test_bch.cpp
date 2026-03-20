#include "lunalink/encoding/bch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-501] BCH decoder: zero errors", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};

  // Test some sample FID/TOI pairs
  for (uint32_t f_idx = 0; f_idx < 4; ++f_idx) {
    for (uint32_t t_idx : {0U, 10U, 69U, 99U}) {
      const Fid fid = static_cast<Fid>(static_cast<uint8_t>(f_idx));
      const Toi toi{static_cast<uint8_t>(t_idx)};
      REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

      const auto result = bch_decode(codeword);
      CHECK(result.status == BchStatus::kOk);
      CHECK(result.fid == fid);
      CHECK(result.toi.value() == toi.value());
      CHECK(result.hamming_distance == 0);
    }
  }
}

TEST_CASE("[LSIS-AFS-501] BCH decoder: single error correction", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  const Fid fid = Fid::kNode3();
  const Toi toi{42};

  REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

  // Flip each bit individually and ensure recovery
  for (uint32_t i = 0; i < 52U; ++i) {
    auto corrupted = codeword;
    corrupted[i] ^= 1U;

    const auto result = bch_decode(corrupted);
    CHECK(result.status == BchStatus::kOk);
    CHECK(result.fid == fid);
    CHECK(result.toi.value() == toi.value());
    CHECK(result.hamming_distance == 1);
  }
}

TEST_CASE("[LSIS-AFS-501] BCH decoder: double error correction", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  const Fid fid = Fid::kNode2();
  const Toi toi{99};

  REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

  // Flip two bits
  codeword[5] ^= 1U;
  codeword[20] ^= 1U;

  const auto result = bch_decode(codeword);
  CHECK(result.status == BchStatus::kOk);
  CHECK(result.fid == fid);
  CHECK(result.toi.value() == toi.value());
  CHECK(result.hamming_distance == 2);
}

TEST_CASE("BCH decoder: confidence threshold (NASA safety)", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  REQUIRE(bch_encode(Fid::kNode1(), Toi(0), codeword) == BchStatus::kOk);

  // Flip 3 bits. ML decoder will find the match but status should be kNullOutput
  codeword[0] ^= 1U;
  codeword[1] ^= 1U;
  codeword[2] ^= 1U;

  const auto result = bch_decode(codeword);
  CHECK(result.status == BchStatus::kNullOutput);
  CHECK(result.hamming_distance == 3);
}

TEST_CASE("BCH decoder: ambiguity detection", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  REQUIRE(bch_encode(Fid::kNode1(), Toi(0), codeword) == BchStatus::kOk);

  // High-noise scenario: flip many bits.
  for (uint32_t i = 0; i < 10; ++i)
    codeword[i] ^= 1U;

  const auto result = bch_decode(codeword);
  CHECK(result.status != BchStatus::kOk);
}

TEST_CASE("[LSIS-AFS-501] BCH codebook exhaustive check", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  for (uint32_t f_idx = 0; f_idx < 4; ++f_idx) {
    for (uint32_t t_idx = 0; t_idx < 100; ++t_idx) {
      const Fid fid = static_cast<Fid>(static_cast<uint8_t>(f_idx));
      const Toi toi{static_cast<uint8_t>(t_idx)};
      REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

      const auto result = bch_decode(codeword);
      CHECK(result.status == BchStatus::kOk);
      CHECK(result.fid == fid);
      CHECK(result.toi.value() == toi.value());
      CHECK(result.hamming_distance == 0);
    }
  }
}

TEST_CASE("BCH codebook checksum", "[bch]") {
  const uint64_t checksum = bch_codebook_checksum();
  CHECK(checksum != 0);
  // Verify it's stable
  CHECK(bch_codebook_checksum() == checksum);
}

TEST_CASE("[LSIS-AFS-501] BCH soft decoder: clean symbols", "[bch]") {
  // Perfect BPSK: bit=0 → LLR=+1, bit=1 → LLR=-1
  std::array<uint8_t, kBchCodewordLength> codeword{};
  const Fid fid = Fid::kNode3();
  const Toi toi{77};

  REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

  std::array<float, kBchCodewordLength> llr{};
  for (uint32_t i = 0; i < kBchCodewordLength; ++i) {
    llr[i] = (codeword[i] == 0U) ? 1.0F : -1.0F;
  }

  const auto result = bch_decode_soft(llr);
  CHECK(result.status == BchStatus::kOk);
  CHECK(result.fid == fid);
  CHECK(result.toi.value() == toi.value());
}

TEST_CASE("[LSIS-AFS-501] BCH soft decoder: noisy symbols corrected", "[bch]") {
  // Add small perturbation — soft decoder should still recover
  std::array<uint8_t, kBchCodewordLength> codeword{};
  const Fid fid = Fid::kNode2();
  const Toi toi{33};

  REQUIRE(bch_encode(fid, toi, codeword) == BchStatus::kOk);

  std::array<float, kBchCodewordLength> llr{};
  for (uint32_t i = 0; i < kBchCodewordLength; ++i) {
    llr[i] = (codeword[i] == 0U) ? 0.9F : -0.9F;
  }
  // Flip soft polarity on 3 symbols: use opposite polarity (low confidence)
  // regardless of what the codeword bit is at each position.
  for (uint32_t pos : {7U, 15U, 30U}) {
    // Wrong polarity at low confidence: bit=0→-0.1, bit=1→+0.1
    llr[pos] = (codeword[pos] == 0U) ? -0.1F : 0.1F;
  }

  const auto result = bch_decode_soft(llr);
  CHECK(result.status == BchStatus::kOk);
  CHECK(result.fid == fid);
  CHECK(result.toi.value() == toi.value());
}

TEST_CASE("[LSIS-AFS-501] BCH soft decoder: exhaustive codebook", "[bch]") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  for (uint32_t f_idx = 0; f_idx < 4; ++f_idx) {
    for (uint32_t t_idx = 0; t_idx < 100; ++t_idx) {
      REQUIRE(bch_encode(Fid(static_cast<uint8_t>(f_idx)),
                         Toi(static_cast<uint8_t>(t_idx)),
                         codeword) == BchStatus::kOk);

      std::array<float, kBchCodewordLength> llr{};
      for (uint32_t i = 0; i < kBchCodewordLength; ++i) {
        llr[i] = (codeword[i] == 0U) ? 1.0F : -1.0F;
      }

      const auto result = bch_decode_soft(llr);
      CHECK(result.status == BchStatus::kOk);
      CHECK(result.fid.value() == f_idx);
      CHECK(result.toi.value() == t_idx);
    }
  }
}

TEST_CASE("[LSIS-AFS-501] BCH spec test vector: Figure 8 (FID=0, TOI=69)", "[bch]") {
  // LSIS-AFS V1.0 Figure 8: SB1 data 0x045 (FID=0, TOI=69)
  // Expected encoded field: 0x229f61dbb84a0
  constexpr std::array<uint8_t, kBchCodewordLength> expected = {
      0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
  };

  std::array<uint8_t, kBchCodewordLength> codeword{};
  REQUIRE(bch_encode(Fid::kNode1(), Toi(69), codeword) == BchStatus::kOk);
  CHECK(std::ranges::equal(codeword, expected));

  // Verify hex representation matches spec: 0x229f61dbb84a0
  uint64_t packed = 0;
  for (uint32_t i = 0; i < kBchCodewordLength; ++i) {
    packed = (packed << 1U) | static_cast<uint64_t>(codeword[i]);
  }
  CHECK(packed == 0x229f61dbb84a0ULL);

  // Decode must recover FID=0, TOI=69
  const auto result = bch_decode(codeword);
  CHECK(result.status == BchStatus::kOk);
  CHECK(result.fid == Fid::kNode1());
  CHECK(result.toi.value() == 69);
}

TEST_CASE("[LSIS-AFS-501] Gateway 2: BCH Round-trip", "[bch]") {
  Fid fid{2};
  Toi toi{42};
  std::array<uint8_t, 52> encoded{};

  REQUIRE(bch_encode(fid, toi, encoded) == BchStatus::kOk);

  auto result = bch_decode(encoded);

  CHECK(result.status == BchStatus::kOk);
  CHECK(result.fid.value() == 2);
  CHECK(result.toi.value() == 42);
  CHECK(result.hamming_distance == 0);
}

TEST_CASE("BCH: Ambiguous Match Detection", "[bch]") {
  std::array<uint8_t, 52> corrupted{};
  corrupted.fill(0);
  for (size_t i = 0; i < 26; ++i)
    corrupted[i] = 1;

  const auto result = bch_decode(corrupted);
  CHECK(result.status != BchStatus::kOk);
}

TEST_CASE("BCH soft decode: invalid LLR (NaN)", "[bch]") {
  std::array<float, 52> llrs{};
  llrs.fill(1.0F);
  llrs[10] = std::nanf("");
  const auto result = bch_decode_soft(std::span<const float, 52>(llrs.data(), 52));
  CHECK(result.status != BchStatus::kOk);
}

TEST_CASE("BCH soft decode: infinite LLR", "[bch]") {
  std::array<float, 52> llrs{};
  llrs.fill(1.0F);
  llrs[5] = std::numeric_limits<float>::infinity();
  const auto result = bch_decode_soft(std::span<const float, 52>(llrs.data(), 52));
  CHECK(result.status != BchStatus::kOk);
}

TEST_CASE("BCH codebook checksum: stable across calls", "[bch]") {
  uint64_t c1 = bch_codebook_checksum();
  uint64_t c2 = bch_codebook_checksum();
  CHECK(c1 == c2);
  CHECK(c1 != 0);
}
