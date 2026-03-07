#include "lunalink/signal/ldpc.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <algorithm>

using namespace lunalink::signal;

TEST_CASE("LDPC Encoder: basic execution") {
    std::array<uint8_t, 200> msg{};
    std::fill(msg.begin(), msg.end(), 0U);
    msg[0] = 1; // Set one bit
    
    std::array<uint8_t, 6000> out_sf2{};
    std::array<uint8_t, 4400> out_sf3{};
    std::array<uint8_t, 4400> out_sf4{};

    SECTION("SF2 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg, out_sf2) == LdpcStatus::kOk);
        // Check binary values
        for (auto b : out_sf2) {
            REQUIRE(b <= 1U);
        }
    }

    SECTION("SF3 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF3, msg, out_sf3) == LdpcStatus::kOk);
        for (auto b : out_sf3) {
            REQUIRE(b <= 1U);
        }
    }

    SECTION("SF4 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF4, msg, out_sf4) == LdpcStatus::kOk);
        for (auto b : out_sf4) {
            REQUIRE(b <= 1U);
        }
    }
}

TEST_CASE("LDPC Encoder: error paths") {
    std::array<uint8_t, 199> short_msg{};
    std::array<uint8_t, 6000> out{};
    
    CHECK(ldpc_encode(LdpcSubframe::kSF2, short_msg, out) == LdpcStatus::kInvalidInput);
    
    std::array<uint8_t, 200> msg{};
    std::span<uint8_t> short_out(out.data(), 5999);
    CHECK(ldpc_encode(LdpcSubframe::kSF2, msg, short_out) == LdpcStatus::kOutputTooSmall);
}
