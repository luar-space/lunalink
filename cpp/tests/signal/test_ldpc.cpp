#include "lunalink/signal/ldpc.hpp"
#include "lunalink/signal/ldpc_tables.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-320] LDPC Encoder: basic execution") {
    // SF2: 1200 bits
    std::array<uint8_t, 1200> msg_sf2{};
    msg_sf2[0] = 1;

    // SF3/4: 870 bits
    std::array<uint8_t, 870> msg_sf3{};
    msg_sf3[0] = 1;

    std::array<uint8_t, 2400> out_sf2{};
    std::array<uint8_t, 1740> out_sf3{};
    std::array<uint8_t, 1740> out_sf4{};
    LdpcEncodeWorkspace enc_ws{};

    SECTION("SF2 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg_sf2, out_sf2, enc_ws) == LdpcStatus::kOk);
        for (auto b : out_sf2) REQUIRE(b <= 1U);
    }

    SECTION("SF3 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF3, msg_sf3, out_sf3, enc_ws) == LdpcStatus::kOk);
        for (auto b : out_sf3) REQUIRE(b <= 1U);
    }

    SECTION("SF4 Encoding") {
        REQUIRE(ldpc_encode(LdpcSubframe::kSF4, msg_sf3, out_sf4, enc_ws) == LdpcStatus::kOk);
        for (auto b : out_sf4) REQUIRE(b <= 1U);
    }
}

TEST_CASE("[LSIS-AFS-320] LDPC Encoder: error paths") {
    std::array<uint8_t, 1199> short_msg{};
    std::array<uint8_t, 2400> out{};
    LdpcEncodeWorkspace enc_ws{};

    // SF2 expects 1200
    CHECK(ldpc_encode(LdpcSubframe::kSF2, short_msg, out, enc_ws) == LdpcStatus::kInvalidInput);

    std::array<uint8_t, 1200> msg{};
    std::span<uint8_t> short_out(out.data(), 2399);
    CHECK(ldpc_encode(LdpcSubframe::kSF2, msg, short_out, enc_ws) == LdpcStatus::kOutputTooSmall);
}

TEST_CASE("LDPC: Table CRC32 integrity (all 10 matrices)") {
    CHECK(kLdpc_sf2_a.verify_integrity());
    CHECK(kLdpc_sf2_b.verify_integrity());
    CHECK(kLdpc_sf2_b_inv.verify_integrity());
    CHECK(kLdpc_sf2_c.verify_integrity());
    CHECK(kLdpc_sf2_d.verify_integrity());
    CHECK(kLdpc_sf3_a.verify_integrity());
    CHECK(kLdpc_sf3_b.verify_integrity());
    CHECK(kLdpc_sf3_b_inv.verify_integrity());
    CHECK(kLdpc_sf3_c.verify_integrity());
    CHECK(kLdpc_sf3_d.verify_integrity());

    SECTION("CRC tamper detection: modified num_entries") {
        LdpcCsrMatrix tampered = kLdpc_sf2_a;
        tampered.num_entries += 1;
        CHECK_FALSE(tampered.verify_integrity());
    }

    SECTION("CRC tamper detection: modified crc32") {
        LdpcCsrMatrix tampered = kLdpc_sf2_a;
        tampered.crc32 ^= 1U;
        CHECK_FALSE(tampered.verify_integrity());
    }
}

TEST_CASE("LDPC: Matrix dimension consistency") {
    // Layer 1: A and B must have same row count
    CHECK(kLdpc_sf2_a.num_rows == kLdpc_sf2_b.num_rows);
    CHECK(kLdpc_sf3_a.num_rows == kLdpc_sf3_b.num_rows);
    // Layer 2: C and D must have same row count
    CHECK(kLdpc_sf2_c.num_rows == kLdpc_sf2_d.num_rows);
    CHECK(kLdpc_sf3_c.num_rows == kLdpc_sf3_d.num_rows);
    // B_inv must match B dimensions
    CHECK(kLdpc_sf2_b_inv.num_rows == kLdpc_sf2_b.num_rows);
    CHECK(kLdpc_sf3_b_inv.num_rows == kLdpc_sf3_b.num_rows);

    SECTION("Max row weight within buffer bounds") {
        auto max_row_weight = [](const LdpcCsrMatrix& m) {
            uint32_t max_w = 0;
            for (uint16_t r = 0; r < m.num_rows; ++r) {
                uint32_t w = m.row_ptr[r + 1] - m.row_ptr[r];
                if (w > max_w) max_w = w;
            }
            return max_w;
        };
        // Combined layer weights must fit in kMaxRowDegree (16)
        // Layer 1 max = A_max + B_max, Layer 2 max = C_max + D_max + 1 (identity)
        uint32_t l1_max = max_row_weight(kLdpc_sf2_a) + max_row_weight(kLdpc_sf2_b);
        uint32_t l2_max = max_row_weight(kLdpc_sf2_c) + max_row_weight(kLdpc_sf2_d) + 1;
        CHECK(l1_max <= 16);
        CHECK(l2_max <= 16);

        l1_max = max_row_weight(kLdpc_sf3_a) + max_row_weight(kLdpc_sf3_b);
        l2_max = max_row_weight(kLdpc_sf3_c) + max_row_weight(kLdpc_sf3_d) + 1;
        CHECK(l1_max <= 16);
        CHECK(l2_max <= 16);
    }
}

TEST_CASE("[LSIS-AFS-320] LDPC: Subframe types coverage") {
    std::array<uint8_t, 1200> msg_sf2{};
    std::array<uint8_t, 870> msg_sf3{};
    std::array<uint8_t, 2400> out_sf2{};
    std::array<uint8_t, 1740> out_sf3{};
    LdpcEncodeWorkspace enc_ws{};

    CHECK(ldpc_encode(LdpcSubframe::kSF2, msg_sf2, out_sf2, enc_ws) == LdpcStatus::kOk);
    CHECK(ldpc_encode(LdpcSubframe::kSF3, msg_sf3, out_sf3, enc_ws) == LdpcStatus::kOk);
    CHECK(ldpc_encode(LdpcSubframe::kSF4, msg_sf3, out_sf3, enc_ws) == LdpcStatus::kOk);
}
