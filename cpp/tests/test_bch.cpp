#include "lunalink/signal/bch.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>

using namespace lunalink::signal;

TEST_CASE("BCH decoder: zero errors") {
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

TEST_CASE("BCH decoder: single error correction") {
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

TEST_CASE("BCH decoder: double error correction") {
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

TEST_CASE("BCH decoder: confidence threshold (NASA safety)") {
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

TEST_CASE("BCH decoder: ambiguity detection") {
  std::array<uint8_t, kBchCodewordLength> codeword{};
  REQUIRE(bch_encode(Fid::kNode1(), Toi(0), codeword) == BchStatus::kOk);

  // High-noise scenario: flip many bits.
  for (uint32_t i = 0; i < 10; ++i) codeword[i] ^= 1U;

  const auto result = bch_decode(codeword);
  CHECK(result.status != BchStatus::kOk);
}

TEST_CASE("BCH codebook exhaustive check") {
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

TEST_CASE("BCH codebook checksum") {
  const uint64_t checksum = bch_codebook_checksum();
  CHECK(checksum != 0);
  // Verify it's stable
  CHECK(bch_codebook_checksum() == checksum);
}
