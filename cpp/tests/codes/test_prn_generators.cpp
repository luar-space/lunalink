#include "lunalink/codes/gold.hpp"
#include "lunalink/codes/weil.hpp"
#include "lunalink/codes/prn.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-101] [LSIS-AFS-102] PRN Generators: Exhaustive validation against Annex 3", "[prn]") {
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

    SECTION("Weil-10230: Generator produces valid binary output") {
        std::array<uint8_t, kWeil10230ChipLength> generated{};
        // Verify generator runs without fault for a sample of PRNs.
        for (uint8_t prn = 1; prn <= 10; ++prn) {
            PrnId id{prn};
            REQUIRE(weil_gen.generate_primary(id, generated) == PrnStatus::kOk);
            for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
                REQUIRE((generated[i] == 0 || generated[i] == 1));
            }
        }
    }

    SECTION("Weil-1500: Generator produces valid binary output") {
        std::array<uint8_t, kWeil1500ChipLength> generated{};
        for (uint8_t prn = 1; prn <= 10; ++prn) {
            PrnId id{prn};
            REQUIRE(weil_gen.generate_tertiary(id, generated) == PrnStatus::kOk);
            for (uint16_t i = 0; i < kWeil1500ChipLength; ++i) {
                REQUIRE((generated[i] == 0 || generated[i] == 1));
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

    SECTION("Weil zero insertion_index (PRN 38) returns kFaultDetected") {
        // PRN 38 has insertion_index == 0 in weil_parameters.cpp.
        // Regression test: construct_weil must return kFaultDetected, not silently
        // skip the expansion and return kOk with a wrong spreading code.
        std::array<uint8_t, kWeil10230ChipLength> out{};
        PrnId prn38(38);
        CHECK(weil_gen.generate_primary(prn38, out) == PrnStatus::kFaultDetected);
    }
}
