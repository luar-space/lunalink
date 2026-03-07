#include "lunalink/signal/ldpc.hpp"
#include "lunalink/signal/ldpc_tables.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>

using namespace lunalink::signal;

TEST_CASE("LDPC Encoder: basic execution") {
    std::array<uint8_t, 200> msg{};
    std::fill(msg.begin(), msg.end(), 0U);
    msg[0] = 1; // Set one bit
    
    std::array<uint8_t, 2400> out_sf2{};
    std::array<uint8_t, 1740> out_sf3{};
    std::array<uint8_t, 1740> out_sf4{};

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
    std::array<uint8_t, 2400> out{};
    
    CHECK(ldpc_encode(LdpcSubframe::kSF2, short_msg, out) == LdpcStatus::kInvalidInput);
    
    std::array<uint8_t, 200> msg{};
    std::span<uint8_t> short_out(out.data(), 2399);
    CHECK(ldpc_encode(LdpcSubframe::kSF2, msg, short_out) == LdpcStatus::kOutputTooSmall);
}

TEST_CASE("LDPC: Table integrity and failure paths") {
    CHECK(kLdpc_sf2_a.verify_integrity());
    CHECK(kLdpc_sf2_b_inv.verify_integrity());
    CHECK(kLdpc_sf2_c.verify_integrity());
    CHECK(kLdpc_sf2_d.verify_integrity());
    CHECK(kLdpc_sf3_a.verify_integrity());
    CHECK(kLdpc_sf3_b_inv.verify_integrity());
    CHECK(kLdpc_sf3_c.verify_integrity());
    CHECK(kLdpc_sf3_d.verify_integrity());

    SECTION("Integrity failure detection") {
        LdpcCsrMatrix tampered = kLdpc_sf2_a;
        tampered.num_entries += 1; // Tamper
        CHECK_FALSE(tampered.verify_integrity());
    }
}

TEST_CASE("LDPC: Subframe types coverage") {
    std::array<uint8_t, 200> msg{};
    std::array<uint8_t, 2400> out{};
    
    // SF2
    CHECK(ldpc_encode(LdpcSubframe::kSF2, msg, out) == LdpcStatus::kOk);
    // SF3
    CHECK(ldpc_encode(LdpcSubframe::kSF3, msg, out) == LdpcStatus::kOk);
    // SF4
    CHECK(ldpc_encode(LdpcSubframe::kSF4, msg, out) == LdpcStatus::kOk);
}
