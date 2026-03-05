#include <array>
#include <catch2/catch_test_macros.hpp>
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"

using namespace lunalink::signal;

TEST_CASE("modulate_bpsk_i chip mapping with +1 symbol") {
    // Per spec section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1
    const uint8_t chips[4] = {0, 1, 0, 1};
    int8_t out[4]{};
    modulate_bpsk_i(chips, 4, 1, out);
    REQUIRE(out[0] == 1);
    REQUIRE(out[1] == -1);
    REQUIRE(out[2] == 1);
    REQUIRE(out[3] == -1);
}

TEST_CASE("modulate_bpsk_i chip mapping with -1 symbol") {
    const uint8_t chips[4] = {0, 1, 0, 1};
    int8_t out[4]{};
    modulate_bpsk_i(chips, 4, -1, out);
    REQUIRE(out[0] == -1);
    REQUIRE(out[1] == 1);
    REQUIRE(out[2] == -1);
    REQUIRE(out[3] == 1);
}

TEST_CASE("modulate_bpsk_i full PRN 1 all values in {-1, +1}") {
    std::array<int8_t, kGoldChipLength> out{};
    modulate_bpsk_i(gold_prn(1), kGoldChipLength, 1, out.data());
    for (auto v : out)
        REQUIRE((v == -1 || v == 1));
}
