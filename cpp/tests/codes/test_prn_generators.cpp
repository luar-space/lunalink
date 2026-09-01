// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/gold.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/codes/weil.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-101] [LSIS-AFS-102] PRN Generators: Exhaustive validation against Annex 3",
          "[prn]") {
  // The packed `*_prn_packed()` codebooks are the Annex 3 attachments
  // transcribed verbatim by `scripts/gen_prn_table.py` from
  // `docs/references/{006,007,008}_*.txt`, and are checksum-guarded at load
  // (`codebook_checksums.hpp`).  Asserting generator == codebook is therefore
  // the bit-exact Annex 3 conformance check for the live generators, for every
  // chip of every PRN 1-210 in all three code families.
  GoldGenerator gold_gen;
  WeilGenerator weil_gen;

  SECTION("Gold-2046: All 210 PRNs") {
    std::array<uint8_t, kGoldChipLength> generated{};
    for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
      PrnId id{prn};
      REQUIRE(gold_gen.generate(id, generated) == PrnStatus::kOk);

      PrnCode lut;
      REQUIRE(gold_prn_packed(id, lut) == PrnStatus::kOk);

      for (uint16_t i = 0; i < kGoldChipLength; ++i) {
        uint8_t lut_chip = 0;
        REQUIRE(unpack_chip(lut, i, lut_chip) == PrnStatus::kOk);
        if (generated[i] != lut_chip) {
          FAIL("Gold Mismatch PRN " << static_cast<int>(prn) << " chip " << static_cast<int>(i));
        }
      }
    }
  }

  SECTION("Weil-10230: All 210 PRNs") {
    std::array<uint8_t, kWeil10230ChipLength> generated{};
    for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
      PrnId id{prn};
      REQUIRE(weil_gen.generate_primary(id, generated) == PrnStatus::kOk);

      PrnCode lut;
      REQUIRE(weil10230_prn_packed(id, lut) == PrnStatus::kOk);

      for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
        uint8_t lut_chip = 0;
        REQUIRE(unpack_chip(lut, i, lut_chip) == PrnStatus::kOk);
        if (generated[i] != lut_chip) {
          FAIL("Weil-10230 Mismatch PRN " << static_cast<int>(prn) << " chip "
                                          << static_cast<int>(i));
        }
      }
    }
  }

  SECTION("Weil-1500: All 210 PRNs") {
    std::array<uint8_t, kWeil1500ChipLength> generated{};
    for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
      PrnId id{prn};
      REQUIRE(weil_gen.generate_tertiary(id, generated) == PrnStatus::kOk);

      PrnCode lut;
      REQUIRE(weil1500_prn_packed(id, lut) == PrnStatus::kOk);

      for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
        uint8_t lut_chip = 0;
        REQUIRE(unpack_chip(lut, i, lut_chip) == PrnStatus::kOk);
        if (generated[i] != lut_chip) {
          FAIL("Weil-1500 Mismatch PRN " << static_cast<int>(prn) << " chip "
                                         << static_cast<int>(i));
        }
      }
    }
  }

  SECTION("Generator error paths") {
    std::array<uint8_t, kWeil10230ChipLength> out{};
    PrnId bad_prn(1);
    uint8_t over = 250;
    std::memcpy(&bad_prn.storage.v1, &over, 1);
    std::memcpy(&bad_prn.storage.v2, &over, 1);
    std::memcpy(&bad_prn.storage.v3, &over, 1);

    CHECK(gold_gen.generate(bad_prn, out) == PrnStatus::kInvalidPrn);
    CHECK(weil_gen.generate_primary(bad_prn, out) == PrnStatus::kInvalidPrn);
    std::array<uint8_t, kWeil1500ChipLength> out1500{};
    CHECK(weil_gen.generate_tertiary(bad_prn, out1500) == PrnStatus::kInvalidPrn);

    // Output too small
    std::span<uint8_t> small(out.data(), 10);
    PrnId good_prn(1);
    CHECK(gold_gen.generate(good_prn, small) == PrnStatus::kNullOutput);
    CHECK(weil_gen.generate_primary(good_prn, small) == PrnStatus::kNullOutput);
    CHECK(weil_gen.generate_tertiary(good_prn, small) == PrnStatus::kNullOutput);
  }

  SECTION("PRN 38 generates successfully (was zeroed placeholder, now fixed)") {
    // PRN 38 previously had k=0, p=0 (zeroed placeholder). Fixed to match
    // spec_tables/appendix_d_weil_primary_params.csv: k=4858, p=824.
    std::array<uint8_t, kWeil10230ChipLength> out{};
    PrnId prn38(38);
    CHECK(weil_gen.generate_primary(prn38, out) == PrnStatus::kOk);
  }
}
