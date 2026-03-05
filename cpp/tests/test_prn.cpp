#include <catch2/catch_test_macros.hpp>
#include "lunalink/signal/prn.hpp"

using namespace lunalink::signal;

TEST_CASE("gold_prn PRN 1 has correct length and chip values") {
    const auto* chips = gold_prn(1);
    for (uint16_t i = 0; i < kGoldChipLength; ++i)
        REQUIRE((chips[i] == 0u || chips[i] == 1u));
}

TEST_CASE("gold_prn returns distinct sequences for different PRNs") {
    const auto* p1 = gold_prn(1);
    const auto* p2 = gold_prn(2);
    bool differ = false;
    for (uint16_t i = 0; i < kGoldChipLength; ++i) {
        if (p1[i] != p2[i]) {
            differ = true;
            break;
        }
    }
    REQUIRE(differ);
}

TEST_CASE("gold_prn PRN 1 first byte matches reference") {
    // First byte of PRN 1 hex is 0x17 = 0001 0111b -> chips 0,0,0,1,0,1,1,1
    const uint8_t expected[8] = {0, 0, 0, 1, 0, 1, 1, 1};
    const auto* chips = gold_prn(1);
    for (int i = 0; i < 8; ++i)
        REQUIRE(chips[i] == expected[i]);
}

TEST_CASE("gold_prn all 210 PRNs have binary chip values") {
    for (uint8_t prn = 1; prn <= kGoldPrnCount; ++prn) {
        const auto* chips = gold_prn(prn);
        for (uint16_t i = 0; i < kGoldChipLength; ++i)
            REQUIRE((chips[i] == 0u || chips[i] == 1u));
    }
}
