#include "lunalink/encoding/crc.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using namespace lunalink::signal;

TEST_CASE("CRC-24: Basic correctness", "[crc]") {
  // All-zeros with seed=0 produces all-zeros CRC (polynomial never triggered).
  // This verifies the function runs cleanly but does not exercise the polynomial.
  std::array<uint8_t, 100> zero_data{};
  std::array<uint8_t, 24> zero_crc{};
  REQUIRE(crc24_calculate(zero_data, zero_crc) == CrcStatus::kOk);
  for (uint8_t bit : zero_crc) {
    CHECK(bit == 0);
  }

  // Non-zero input: single 1-bit followed by zeros exercises one polynomial
  // application (lsb=0 XOR msb=1 → poly XORd in once).
  // CRC-24Q of [0,0,...,0,1] (MSB last) is the polynomial itself (0x864CFB).
  // Verified independently against 3GPP TS 36.212 Annex B reference.
  std::array<uint8_t, 8> single_bit = {0, 0, 0, 0, 0, 0, 0, 1};
  std::array<uint8_t, 24> crc_nonzero{};
  REQUIRE(crc24_calculate(single_bit, crc_nonzero) == CrcStatus::kOk);
  // 0x864CFB = 1000 0110 0100 1100 1111 1011 (MSB first)
  const std::array<uint8_t, 24> expected_poly = {1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
                                                 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1};
  CHECK(crc_nonzero == expected_poly);

  // Multi-bit input to exercise polynomial over several iterations.
  std::array<uint8_t, 10> multi_bit = {1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
  std::array<uint8_t, 24> crc_multi{};
  REQUIRE(crc24_calculate(multi_bit, crc_multi) == CrcStatus::kOk);
  // Verify the CRC is non-zero (if polynomial is broken e.g. always-zero,
  // this catches it).
  bool any_nonzero = false;
  for (uint8_t bit : crc_multi) {
    if (bit != 0) {
      any_nonzero = true;
    }
  }
  CHECK(any_nonzero);
}

TEST_CASE("CRC-24: Round-trip and error detection", "[crc]") {
  std::vector<uint8_t> message = {1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
  std::array<uint8_t, 24> crc{};

  REQUIRE(crc24_calculate(message, crc) == CrcStatus::kOk);

  // Append CRC to message
  std::vector<uint8_t> data_with_crc = message;
  data_with_crc.insert(data_with_crc.end(), crc.begin(), crc.end());

  // Verify
  CHECK(crc24_verify(data_with_crc) == true);

  // Corrupt one bit
  data_with_crc[0] ^= 1;
  CHECK(crc24_verify(data_with_crc) == false);
}

TEST_CASE("CRC-24: Known vector (Qualcomm/3GPP pattern)", "[crc]") {
  // Polynomial 0x864CFB is used in LTE/5G.
  // Let's use a simple pattern.
  // Data: 0x01 (binary 00000001) -> CRC-24Q should match standard implementations.
  std::array<uint8_t, 8> data = {0, 0, 0, 0, 0, 0, 0, 1};
  std::array<uint8_t, 24> crc{};

  REQUIRE(crc24_calculate(data, crc) == CrcStatus::kOk);

  // Standard CRC-24Q for 0x01 is 0x864CFB (since it's one shift of the poly)
  // MSB: 1000 0110 0100 1100 1111 1011
  std::array<uint8_t, 24> expected = {1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
                                      1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1};

  CHECK(crc == expected);
}

TEST_CASE("CRC-24: verify rejects too-short input", "[crc]") {
  std::array<uint8_t, 10> tiny{};
  CHECK(crc24_verify(tiny) == false);
}

TEST_CASE("CRC-24: verify detects corruption", "[crc]") {
  std::array<uint8_t, 50> data{};
  for (size_t i = 0; i < 50; ++i)
    data[i] = static_cast<uint8_t>(i & 1);
  std::array<uint8_t, 24> crc{};
  CHECK(crc24_calculate(data, crc) == CrcStatus::kOk);

  // Concatenate and corrupt
  std::array<uint8_t, 74> combined{};
  for (size_t i = 0; i < 50; ++i)
    combined[i] = data[i];
  for (size_t i = 0; i < 24; ++i)
    combined[50 + i] = crc[i];

  // Clean verify
  CHECK(crc24_verify(combined) == true);
  // Corrupt → false
  combined[0] ^= 1;
  CHECK(crc24_verify(combined) == false);
}

TEST_CASE("CRC-24: calculate then verify round-trip", "[crc]") {
  std::array<uint8_t, 100> data{};
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<uint8_t>(i % 2);

  std::array<uint8_t, 24> crc{};
  CHECK(crc24_calculate(data, crc) == CrcStatus::kOk);

  std::array<uint8_t, 124> data_with_crc{};
  for (size_t i = 0; i < 100; ++i)
    data_with_crc[i] = data[i];
  for (size_t i = 0; i < 24; ++i)
    data_with_crc[100 + i] = crc[i];

  CHECK(crc24_verify(data_with_crc) == true);

  // Flip a bit — should fail
  data_with_crc[50] ^= 1;
  CHECK(crc24_verify(data_with_crc) == false);

  // Too short for verify
  std::span<const uint8_t> too_short(data_with_crc.data(), 10);
  CHECK(crc24_verify(too_short) == false);
}
