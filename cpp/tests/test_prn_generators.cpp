#include "lunalink/signal/gold.hpp"
#include "lunalink/signal/weil.hpp"
#include "lunalink/signal/prn.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>

using namespace lunalink::signal;

TEST_CASE(\"GoldGenerator matches Annex 3 LUT data\") {
    GoldGenerator gen;
    std::array<uint8_t, kGoldChipLength> generated{};
    
    for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
        DYNAMIC_SECTION(\"PRN \" << (int)prn) {
            PrnId id{prn};
            REQUIRE(gen.generate(id, generated) == PrnStatus::kOk);
            
            PrnCode lut;
            REQUIRE(gold_prn_packed(id, lut) == PrnStatus::kOk);
            
            for (uint16_t i = 0; i < kGoldChipLength; ++i) {
                uint8_t lut_chip = 0;
                REQUIRE(unpack_chip(lut, i, lut_chip) == PrnStatus::kOk);
                if (generated[i] != lut_chip) {
                    FAIL(\"Mismatch at chip \" << i << \": gen=\" << (int)generated[i] << \" lut=\" << (int)lut_chip);
                }
            }
        }
    }
}

TEST_CASE(\"WeilGenerator matches Annex 3 LUT data\") {
    WeilGenerator gen;
    
    SECTION(\"Primary Codes (all 210)\") {
        std::array<uint8_t, kWeil10230ChipLength> generated{};
        for (uint8_t prn = 1; prn <= kPrnCount; ++prn) {
            PrnId id{prn};
            REQUIRE(gen.generate_primary(id, generated) == PrnStatus::kOk);
            
            PrnCode lut;
            REQUIRE(weil10230_prn_packed(id, lut) == PrnStatus::kOk);
            
            for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
                uint8_t lut_chip = 0;
                REQUIRE(unpack_chip(lut, i, lut_chip) == PrnStatus::kOk);
                if (generated[i] != lut_chip) {
                    FAIL(\"Mismatch at primary chip \" << i << \" PRN \" << (int)prn << \": gen=\" << (int)generated[i] << \" lut=\" << (int)lut_chip);
                }
            }
        }
    }
}
