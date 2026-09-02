// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/legendre.hpp"
#include "lunalink/codes/prn_codes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
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

// G-13: "gold_prn PRN 1 has binary chip values" deleted —
// `read_chip` extracts a single bit so its output is `<= 1` by
// construction; the loop was tautological. Generator correctness for
// Gold PRN 1 is anchored by the first-byte = 0x5D oracle below and the
// interop test against LANS-AFS-SIM in tests/test_interop_codes.py.

TEST_CASE("[LSIS-AFS-101] gold_prn returns distinct sequences for different PRNs", "[prn]") {
  const auto p1 = get_gold(1);
  const auto p2 = get_gold(2);
  bool differ = false;
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    if (read_chip(p1, i) != read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("[LSIS-AFS-101] gold_prn PRN 1 first byte matches reference", "[prn]") {
  // Provenance: LSIS V1.0 Annex 3, file "LNIS-AD1-Vol-A-Annex3-Gold-2046.hex",
  // PRN 1 first byte. The Annex 3 README
  // (docs/references/005_LNIS-AD1-Vol-A-Annex3-PRN-Spreading-Codes-README.pdf
  // §2.1) prepends two zero MSB pad bits to the published 2048-bit hex lines,
  // then strips those two MSBs after decoding — after padding normalisation
  // the first 8 chips of Gold PRN 1 are {0, 1, 0, 1, 1, 1, 0, 1} = 0x5D.
  const auto packed = get_gold(1);
  REQUIRE(packed.data[0] == 0x5Du);
  // Verify individual chips
  const uint8_t expected[8] = {0, 1, 0, 1, 1, 1, 0, 1};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

// G-13: "gold_prn all 210 PRNs have binary chip values" deleted —
// same reason as the PRN-1 version above. The 210 × 2046 = 429 660 CHECKs
// this test ran cannot fail by construction of `read_chip`. Full-length
// pairwise distinctness is covered by the test above this comment; full
// bit-exact validation of every PRN is covered by tests/test_interop_codes.py.

// --- Weil-10230 (AFS-Q primary code) ---

// G-13: tautological binary-chip check on Weil-10230 PRN 1 deleted —
// `read_chip` always returns 0 or 1. Correctness is anchored by the
// first-chip oracle + interop test against LANS-AFS-SIM (bit-exact
// across all 210 PRNs).

TEST_CASE("[LSIS-AFS-102] weil10230_prn returns distinct sequences for different PRNs", "[prn]") {
  const auto p1 = get_weil10230(1);
  const auto p2 = get_weil10230(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    if (read_chip(p1, i) != read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("[LSIS-AFS-102] weil10230_prn PRN 1 first chips match reference", "[prn]") {
  // Provenance: LSIS V1.0 Annex 3, file "LNIS-AD1-Vol-A-Annex3-Weil-10230.hex",
  // PRN 1 first 14 chips after applying the same 2-MSB padding convention
  // as Gold-2046 (see Annex 3 README §2.1 and the gold_prn PRN 1 test above).
  const auto packed = get_weil10230(1);
  const uint8_t expected[14] = {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1};
  for (uint16_t i = 0; i < 14; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

// --- Weil-1500 (AFS-Q tertiary code) ---

// G-13: tautological binary-chip check on Weil-1500 PRN 1 deleted.
// Correctness anchored by the 0xE4 first-byte oracle + distinctness
// test below.

TEST_CASE("[LSIS-AFS-102] weil1500_prn returns distinct sequences for different PRNs", "[prn]") {
  const auto p1 = get_weil1500(1);
  const auto p2 = get_weil1500(2);
  bool differ = false;
  for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
    if (read_chip(p1, i) != read_chip(p2, i)) {
      differ = true;
      break;
    }
  }
  REQUIRE(differ);
}

TEST_CASE("[LSIS-AFS-102] weil1500_prn PRN 1 first byte matches reference", "[prn]") {
  // Provenance: LSIS V1.0 Annex 3, file "LNIS-AD1-Vol-A-Annex3-Weil-1500.hex",
  // PRN 1. After the same 2-MSB padding strip described in the Annex 3 README
  // §2.1, the first 8 chips of Weil-1500 PRN 1 are {1, 1, 1, 0, 0, 1, 0, 0} = 0xE4.
  const auto packed = get_weil1500(1);
  REQUIRE(packed.data[0] == 0xE4u);
  const uint8_t expected[8] = {1, 1, 1, 0, 0, 1, 0, 0};
  for (uint16_t i = 0; i < 8; ++i)
    REQUIRE(read_chip(packed, i) == expected[i]);
}

TEST_CASE("PRN APIs: saturating logic (C-Pattern 4)", "[prn]") {
  PrnCode packed_gold;
  // PRN 0 saturates to 1
  REQUIRE(gold_prn_packed(PrnId{0}, packed_gold) == PrnStatus::kOk);

  PrnCode packed_weil;
  // PRN 211 saturates to 210
  REQUIRE(weil10230_prn_packed(PrnId{211}, packed_weil) == PrnStatus::kOk);

  uint8_t chip = 0;
  // Chip index still returns error as it's not a CheckedRange parameter but a loop boundary.
  REQUIRE(unpack_chip(get_gold(1), kGoldChipLength, chip) == PrnStatus::kInvalidChipIndex);
}

TEST_CASE("Weil codebook checksums (CBIT integrity)", "[prn]") {
  const uint64_t sum10230 = weil10230_codebook_checksum();
  const uint64_t sum1500 = weil1500_codebook_checksum();

  CHECK(sum10230 != 0);
  CHECK(sum1500 != 0);

  // Verify stability
  CHECK(weil10230_codebook_checksum() == sum10230);
  CHECK(weil1500_codebook_checksum() == sum1500);
}

TEST_CASE("PrnId: Triple Modular Redundancy (TMR) resilience", "[prn]") {
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

TEST_CASE("PRN: invalid PRN ID forces error", "[prn]") {
  PrnCode p;
  PrnId bad_prn(1);
  uint8_t over_prn = 250;
  std::memcpy(&bad_prn.storage.v1, &over_prn, 1);
  std::memcpy(&bad_prn.storage.v2, &over_prn, 1);
  std::memcpy(&bad_prn.storage.v3, &over_prn, 1);

  CHECK(static_cast<uint8_t>(gold_prn_packed(bad_prn, p)) ==
        static_cast<uint8_t>(PrnStatus::kInvalidPrn));
  CHECK(static_cast<uint8_t>(weil10230_prn_packed(bad_prn, p)) ==
        static_cast<uint8_t>(PrnStatus::kInvalidPrn));
  CHECK(static_cast<uint8_t>(weil1500_prn_packed(bad_prn, p)) ==
        static_cast<uint8_t>(PrnStatus::kInvalidPrn));
}

TEST_CASE("Legendre: hand-computed oracle for p=7 (true external anchor)", "[prn][legendre]") {
  // G-02: independent correctness anchor for `generate_legendre_sequence()`.
  //
  // The flight Legendre tables (p=10223, 1499) are protected at
  // runtime by a self-signed CRC — valid as CBIT but not as a correctness
  // oracle. Here we hand-compute the expected sequence for p=7 from the
  // definition of the Legendre symbol and assert bit-exact output, which
  // no other test in the suite does.
  //
  // For p=7 the non-zero quadratic residues are {x^2 mod 7 : x ∈ [1, 3]} =
  // {1, 4, 2}. LunaLink's convention (see `cpp/codes/legendre.cpp` line ~128
  // `out[res] = 1`) sets exactly those positions to 1, leaving 0 for the
  // identity position and the non-residues {3, 5, 6}.
  //
  // Expected output (indexed 0..6):
  //   index : 0 1 2 3 4 5 6
  //   chip  : 0 1 1 0 1 0 0
  std::array<uint8_t, 7> out{};
  REQUIRE(generate_legendre_sequence(7, out) == LegendreStatus::kOk);

  constexpr std::array<uint8_t, 7> expected = {0, 1, 1, 0, 1, 0, 0};
  for (size_t i = 0; i < 7; ++i) {
    CAPTURE(i);
    CHECK(out[i] == expected[i]);
  }
}

TEST_CASE("Legendre: checksum stability", "[prn]") {
  auto cs10223 = legendre_10223_checksum();
  auto cs1499 = legendre_1499_checksum();
  CHECK(cs10223 != 0);
  CHECK(cs1499 != 0);
  CHECK(legendre_10223_checksum() == cs10223);
  CHECK(legendre_1499_checksum() == cs1499);
}

TEST_CASE("Legendre: direct generation and cached access", "[prn]") {
  std::array<uint8_t, 1499> leg_out{};
  CHECK(generate_legendre_sequence(1499, leg_out) == LegendreStatus::kOk);

  // G-13: `b == 0 || b == 1` loop deleted — `generate_legendre_sequence`
  // initialises the output to 0 and only ever writes 1, so the check was
  // tautological. Algorithm correctness is anchored by the p=7 hand-
  // computed oracle earlier in this file.

  std::array<uint8_t, 10> small{};
  CHECK(generate_legendre_sequence(1499, small) == LegendreStatus::kOutputTooSmall);
  CHECK(generate_legendre_sequence(0, leg_out) == LegendreStatus::kInvalidPrime);

  auto span10223 = get_cached_legendre(10223);
  CHECK(span10223.size() == 10223);
  auto span1499 = get_cached_legendre(1499);
  CHECK(span1499.size() == 1499);

  auto span_bad = get_cached_legendre(997);
  CHECK(span_bad.empty());
}

// G-12: "PRN: invalid PRN via TMR bypass" was a verbatim duplicate of
// "PRN: invalid PRN ID forces error" above — same TMR-bypass pattern,
// same three checks, same expected kInvalidPrn. Deleted.
