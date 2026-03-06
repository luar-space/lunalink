#include "lunalink/signal/prn.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// --- Gold-2046 (AFS-I primary code) ---

TEST_CASE("gold_prn PRN 1 has binary chip values") {
  const auto *packed = gold_prn_packed(1);
  for (uint16_t i = 0; i < kGoldChipLength; ++i)
    REQUIRE(unpack_chip(packed, i) <= 1u);
}

TEST_CASE("gold_prn returns distinct sequences for different PRNs") {
  const auto *p1 = gold_prn_packed(1);
  const auto *p2 = gold_prn_packed(2);
  bool differ = false;
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    if (unpack_chip(p1, i) != unpack_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("gold_prn PRN 1 first byte matches reference") {
  // First hex byte of PRN 1 in the reference file (after 2-bit padding) maps
  // to chips: from 006_GoldCode2046hex210prns.txt, PRN 1 starts "17590C..."
  // 0x17 = 0001 0111, but that includes the 2 padding zero bits.
  // After stripping 2 MSB padding: bits 01 0111 01 -> chips 0,1,0,1,1,1,0,1
  // Wait — the generator strips padding, so packed byte 0 is the first 8 chips.
  // PRN 1 hex starts "17..." -> 0x17 = 0001_0111 -> 2 padding bits (00), then
  // chips start at bit 2: 0,1,0,1,1,1 from first byte, then 0x59 = 0101_1001
  // So packed[0] should hold chips 0-7 = first 6 from 0x17 + first 2 from 0x59
  // chips: 0,1,0,1,1,1,0,1 -> packed byte = 0x5D
  const auto *packed = gold_prn_packed(1);
  REQUIRE(packed[0] == 0x5Du);
  // Verify individual chips
  const uint8_t expected[8] = {0, 1, 0, 1, 1, 1, 0, 1};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(unpack_chip(packed, i) == expected[i]);
}

TEST_CASE("gold_prn all 210 PRNs have binary chip values") {
  for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
    const auto *packed = gold_prn_packed(prn);
    for (uint16_t i = 0; i < kGoldChipLength; ++i)
      REQUIRE(unpack_chip(packed, i) <= 1u);
  }
}

// --- Weil-10230 (AFS-Q primary code) ---

TEST_CASE("weil10230_prn PRN 1 has binary chip values") {
  const auto *packed = weil10230_prn_packed(1);
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i)
    REQUIRE(unpack_chip(packed, i) <= 1u);
}

TEST_CASE("weil10230_prn returns distinct sequences for different PRNs") {
  const auto *p1 = weil10230_prn_packed(1);
  const auto *p2 = weil10230_prn_packed(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    if (unpack_chip(p1, i) != unpack_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("weil10230_prn PRN 1 first chips match reference") {
  // 007_l1cp_hex210prns.txt PRN 1: "05F50DE01490DD50..."
  // 2 MSB padding bits stripped. 0x05 = 0000_0101, 0xF5 = 1111_0101
  // After dropping 2 padding bits from 0x05: chips start at bit 2.
  // Chips: 00|01 0111 1101 01 -> first 8 packed = 0,1,0,1,1,1,1,1
  // packed[0] should be 0x5F (0101_1111)... let's just verify first chip values
  const auto *packed = weil10230_prn_packed(1);
  // First hex after stripping 2 padding: "05F5" -> 0000_0101 1111_0101
  // drop 2: 00 0101 1111 0101 -> chips 0,0,0,1,0,1,1,1,1,1,0,1,0,1
  const uint8_t expected[14] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1};
  for (uint16_t i = 0; i < 14; ++i)
    REQUIRE(unpack_chip(packed, i) == expected[i]);
}

// --- Weil-1500 (AFS-Q tertiary code) ---

TEST_CASE("weil1500_prn PRN 1 has binary chip values") {
  const auto *packed = weil1500_prn_packed(1);
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i)
    REQUIRE(unpack_chip(packed, i) <= 1u);
}

TEST_CASE("weil1500_prn returns distinct sequences for different PRNs") {
  const auto *p1 = weil1500_prn_packed(1);
  const auto *p2 = weil1500_prn_packed(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
    if (unpack_chip(p1, i) != unpack_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("weil1500_prn PRN 1 first byte matches reference") {
  // 008_Weil1500hex210prns.txt PRN 1: "E4D09EFF..."
  // No padding. 0xE4 = 1110_0100 -> chips: 1,1,1,0,0,1,0,0
  const auto *packed = weil1500_prn_packed(1);
  REQUIRE(packed[0] == 0xE4u);
  const uint8_t expected[8] = {1, 1, 1, 0, 0, 1, 0, 0};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(unpack_chip(packed, i) == expected[i]);
}
