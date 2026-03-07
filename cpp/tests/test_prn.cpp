#include "lunalink/signal/prn.hpp"
#include <catch2/catch_test_macros.hpp>
#include <span>

using namespace lunalink::signal;

namespace {

uint8_t read_chip(const PrnCode& code, uint16_t chip_idx) {
  uint8_t chip = 0;
  REQUIRE(unpack_chip(code, chip_idx, chip) == PrnStatus::kOk);
  return chip;
}

PrnCode get_gold(uint8_t prn_id) {
  PrnCode packed;
  REQUIRE(gold_prn_packed(PrnId{prn_id}, packed) == PrnStatus::kOk);
  return packed;
}

PrnCode get_weil10230(uint8_t prn_id) {
  PrnCode packed;
  REQUIRE(weil10230_prn_packed(PrnId{prn_id}, packed) == PrnStatus::kOk);
  return packed;
}

PrnCode get_weil1500(uint8_t prn_id) {
  PrnCode packed;
  REQUIRE(weil1500_prn_packed(PrnId{prn_id}, packed) == PrnStatus::kOk);
  return packed;
}

} // namespace

// --- Gold-2046 (AFS-I primary code) ---

TEST_CASE("gold_prn PRN 1 has binary chip values") {
  const auto packed = get_gold(1);
  for (uint16_t i = 0; i < kGoldChipLength; ++i)
    REQUIRE(read_chip(packed, i) <= 1u);
}

TEST_CASE("gold_prn returns distinct sequences for different PRNs") {
  const auto p1 = get_gold(1);
  const auto p2 = get_gold(2);
  bool differ = false;
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    if (read_chip(p1, i) !=
        read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("gold_prn PRN 1 first byte matches reference") {
  const auto packed = get_gold(1);
  REQUIRE(packed.data[0] == 0x5Du);
  // Verify individual chips
  const uint8_t expected[8] = {0, 1, 0, 1, 1, 1, 0, 1};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

TEST_CASE("gold_prn all 210 PRNs have binary chip values") {
  for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
    const auto packed = get_gold(prn);
    for (uint16_t i = 0; i < kGoldChipLength; ++i)
      REQUIRE(read_chip(packed, i) <= 1u);
  }
}

// --- Weil-10230 (AFS-Q primary code) ---

TEST_CASE("weil10230_prn PRN 1 has binary chip values") {
  const auto packed = get_weil10230(1);
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i)
    REQUIRE(read_chip(packed, i) <= 1u);
}

TEST_CASE("weil10230_prn returns distinct sequences for different PRNs") {
  const auto p1 = get_weil10230(1);
  const auto p2 = get_weil10230(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    if (read_chip(p1, i) !=
        read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("weil10230_prn PRN 1 first chips match reference") {
  const auto packed = get_weil10230(1);
  const uint8_t expected[14] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1};
  for (uint16_t i = 0; i < 14; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

// --- Weil-1500 (AFS-Q tertiary code) ---

TEST_CASE("weil1500_prn PRN 1 has binary chip values") {
  const auto packed = get_weil1500(1);
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i)
    REQUIRE(read_chip(packed, i) <= 1u);
}

TEST_CASE("weil1500_prn returns distinct sequences for different PRNs") {
  const auto p1 = get_weil1500(1);
  const auto p2 = get_weil1500(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
    if (read_chip(p1, i) !=
        read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("weil1500_prn PRN 1 first byte matches reference") {
  const auto packed = get_weil1500(1);
  REQUIRE(packed.data[0] == 0xE4u);
  const uint8_t expected[8] = {1, 1, 1, 0, 0, 1, 0, 0};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

TEST_CASE("PRN APIs: saturating logic (C-Pattern 4)") {
  PrnCode packed_gold;
  // PRN 0 saturates to 1
  REQUIRE(gold_prn_packed(PrnId{0}, packed_gold) == PrnStatus::kOk);
  
  PrnCode packed_weil;
  // PRN 211 saturates to 210
  REQUIRE(weil10230_prn_packed(PrnId{211}, packed_weil) == PrnStatus::kOk);

  uint8_t chip = 0;
  // Chip index still returns error as it's not a CheckedRange parameter but a loop boundary.
  REQUIRE(unpack_chip(get_gold(1), kGoldChipLength, chip) ==
          PrnStatus::kInvalidChipIndex);
}

TEST_CASE("Weil codebook checksums (CBIT integrity)") {
  const uint64_t sum10230 = weil10230_codebook_checksum();
  const uint64_t sum1500  = weil1500_codebook_checksum();
  
  CHECK(sum10230 != 0);
  CHECK(sum1500 != 0);
  
  // Verify stability
  CHECK(weil10230_codebook_checksum() == sum10230);
  CHECK(weil1500_codebook_checksum() == sum1500);
}

TEST_CASE("PrnId: Triple Modular Redundancy (TMR) resilience") {
  PrnId id(42);
  REQUIRE(id.value() == 42);
  REQUIRE(id.valid());
  
  // Simulate single event upset (SEU) in one of the storage slots.
  id.storage.v1 = CheckedRange<uint8_t, 1, kPrnCount>{99};
  CHECK(id.value() == 42); // Majority vote should preserve the correct value.
  CHECK(id.valid());
  
  id.storage.refresh(CheckedRange<uint8_t, 1, kPrnCount>{42});
  id.storage.v2 = CheckedRange<uint8_t, 1, kPrnCount>{1}; // valid but wrong
  CHECK(id.value() == 42);
  
  id.storage.refresh(CheckedRange<uint8_t, 1, kPrnCount>{42});
  id.storage.v3 = CheckedRange<uint8_t, 1, kPrnCount>{210};
  CHECK(id.value() == 42);
}
