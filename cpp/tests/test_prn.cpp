#include "lunalink/signal/prn.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

namespace {

uint8_t read_chip(const uint8_t *packed, uint16_t chip_idx, uint16_t chip_count) {
  uint8_t chip = 0;
  REQUIRE(unpack_chip(packed, chip_idx, chip_count, &chip) == PrnStatus::kOk);
  return chip;
}

const uint8_t *get_gold(uint8_t prn_id) {
  const uint8_t *packed = nullptr;
  REQUIRE(gold_prn_packed(prn_id, &packed) == PrnStatus::kOk);
  REQUIRE(packed != nullptr);
  return packed;
}

const uint8_t *get_weil10230(uint8_t prn_id) {
  const uint8_t *packed = nullptr;
  REQUIRE(weil10230_prn_packed(prn_id, &packed) == PrnStatus::kOk);
  REQUIRE(packed != nullptr);
  return packed;
}

const uint8_t *get_weil1500(uint8_t prn_id) {
  const uint8_t *packed = nullptr;
  REQUIRE(weil1500_prn_packed(prn_id, &packed) == PrnStatus::kOk);
  REQUIRE(packed != nullptr);
  return packed;
}

} // namespace

// --- Gold-2046 (AFS-I primary code) ---

TEST_CASE("gold_prn PRN 1 has binary chip values") {
  const auto *packed = get_gold(1);
  for (uint16_t i = 0; i < kGoldChipLength; ++i)
    REQUIRE(read_chip(packed, i, kGoldChipLength) <= 1u);
}

TEST_CASE("gold_prn returns distinct sequences for different PRNs") {
  const auto *p1 = get_gold(1);
  const auto *p2 = get_gold(2);
  bool differ = false;
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    if (read_chip(p1, i, kGoldChipLength) !=
        read_chip(p2, i, kGoldChipLength)) {
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
  const auto *packed = get_gold(1);
  REQUIRE(packed[0] == 0x5Du);
  // Verify individual chips
  const uint8_t expected[8] = {0, 1, 0, 1, 1, 1, 0, 1};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i, kGoldChipLength) == expected[i]);
}

TEST_CASE("gold_prn all 210 PRNs have binary chip values") {
  for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
    const auto *packed = get_gold(prn);
    for (uint16_t i = 0; i < kGoldChipLength; ++i)
      REQUIRE(read_chip(packed, i, kGoldChipLength) <= 1u);
  }
}

// --- Weil-10230 (AFS-Q primary code) ---

TEST_CASE("weil10230_prn PRN 1 has binary chip values") {
  const auto *packed = get_weil10230(1);
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i)
    REQUIRE(read_chip(packed, i, kWeil10230ChipLength) <= 1u);
}

TEST_CASE("weil10230_prn returns distinct sequences for different PRNs") {
  const auto *p1 = get_weil10230(1);
  const auto *p2 = get_weil10230(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    if (read_chip(p1, i, kWeil10230ChipLength) !=
        read_chip(p2, i, kWeil10230ChipLength)) {
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
  const auto *packed = get_weil10230(1);
  // First hex after stripping 2 padding: "05F5" -> 0000_0101 1111_0101
  // drop 2: 00 0101 1111 0101 -> chips 0,0,0,1,0,1,1,1,1,1,0,1,0,1
  const uint8_t expected[14] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1};
  for (uint16_t i = 0; i < 14; ++i)
    REQUIRE(read_chip(packed, i, kWeil10230ChipLength) == expected[i]);
}

// --- Weil-1500 (AFS-Q tertiary code) ---

TEST_CASE("weil1500_prn PRN 1 has binary chip values") {
  const auto *packed = get_weil1500(1);
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i)
    REQUIRE(read_chip(packed, i, kWeil1500ChipLength) <= 1u);
}

TEST_CASE("weil1500_prn returns distinct sequences for different PRNs") {
  const auto *p1 = get_weil1500(1);
  const auto *p2 = get_weil1500(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
    if (read_chip(p1, i, kWeil1500ChipLength) !=
        read_chip(p2, i, kWeil1500ChipLength)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("weil1500_prn PRN 1 first byte matches reference") {
  // 008_Weil1500hex210prns.txt PRN 1: "E4D09EFF..."
  // No padding. 0xE4 = 1110_0100 -> chips: 1,1,1,0,0,1,0,0
  const auto *packed = get_weil1500(1);
  REQUIRE(packed[0] == 0xE4u);
  const uint8_t expected[8] = {1, 1, 1, 0, 0, 1, 0, 0};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i, kWeil1500ChipLength) == expected[i]);
}

TEST_CASE("PRN APIs return explicit status on invalid input") {
  const uint8_t *packed = nullptr;
  REQUIRE(gold_prn_packed(0, &packed) == PrnStatus::kInvalidPrn);
  REQUIRE(weil10230_prn_packed(211, &packed) == PrnStatus::kInvalidPrn);
  REQUIRE(weil1500_prn_packed(1, nullptr) == PrnStatus::kNullOutput);

  uint8_t chip = 0;
  REQUIRE(unpack_chip(nullptr, 0, kGoldChipLength, &chip) ==
          PrnStatus::kNullInput);
  REQUIRE(unpack_chip(get_gold(1), kGoldChipLength, kGoldChipLength, &chip) ==
          PrnStatus::kInvalidChipIndex);
  REQUIRE(unpack_chip(get_gold(1), 0, kGoldChipLength, nullptr) ==
          PrnStatus::kNullOutput);
}
