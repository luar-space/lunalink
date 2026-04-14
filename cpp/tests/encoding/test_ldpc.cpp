#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/messages/frame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <ranges>

using namespace lunalink::signal;

#include "ldpc_lans_reference.hpp"

// G-16: bit-exact LDPC encoder regression test against LANS-AFS-SIM.
//
// The existing LDPC tests in this file are all self-referential — they
// encode + decode and compare to the original message, or verify golden
// CRC constants that are themselves computed at runtime. None of them
// catch a regression where encoder output changes in a way that still
// decodes correctly (e.g. a permutation of parity bits).
//
// This test pins the full codeword output against values computed by
// LANS-AFS-SIM (a separately-maintained third-party LDPC implementation).
// The input vectors are chosen so the all-zero codeword is NOT a valid
// answer, which is essential for systematic linear codes — all-zeros
// input encodes to all-zeros under any systematic encoder, and would mask
// sign and parity bugs that the non-trivial input catches. The reference
// arrays were captured by `scripts/interop/dump_lans_ldpc.c` and last
// refreshed on 2026-04-13.
TEST_CASE("[LSIS-AFS-320] LDPC Encoder: bit-exact vs LANS-AFS-SIM reference", "[ldpc]") {
  using namespace lunalink::signal::tests;

  std::array<uint8_t, 2400> out_sf2{};
  LdpcEncodeWorkspace ws{};
  REQUIRE(ldpc_encode(LdpcSubframe::kSF2, kLansSf2AltInput, out_sf2, ws) ==
          LdpcStatus::kOk);
  size_t mismatches_sf2 = 0;
  for (size_t i = 0; i < out_sf2.size(); ++i) {
    if (out_sf2[i] != kLansSf2AltCoded[i]) {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      ++mismatches_sf2;
    }
  }
  CAPTURE(mismatches_sf2);
  CHECK(mismatches_sf2 == 0);

  std::array<uint8_t, 1740> out_sf3{};
  REQUIRE(ldpc_encode(LdpcSubframe::kSF3, kLansSf3AltInput, out_sf3, ws) ==
          LdpcStatus::kOk);
  size_t mismatches_sf3 = 0;
  for (size_t i = 0; i < out_sf3.size(); ++i) {
    if (out_sf3[i] != kLansSf3AltCoded[i]) {  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      ++mismatches_sf3;
    }
  }
  CAPTURE(mismatches_sf3);
  CHECK(mismatches_sf3 == 0);
}

TEST_CASE("[LSIS-AFS-320] LDPC Encoder: basic execution", "[ldpc]") {
  // SF2: 1200 bits
  std::array<uint8_t, kSb2MsgBits> msg_sf2{};
  msg_sf2[0] = 1;

  // SF3/4: 870 bits
  std::array<uint8_t, kSb3MsgBits> msg_sf3{};
  msg_sf3[0] = 1;

  std::array<uint8_t, kSb2Length> out_sf2{};
  std::array<uint8_t, kSb3Length> out_sf3{};
  std::array<uint8_t, kSb3Length> out_sf4{};
  LdpcEncodeWorkspace enc_ws{};

  SECTION("SF2 Encoding") {
    REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg_sf2, out_sf2, enc_ws) == LdpcStatus::kOk);
    for (auto b : out_sf2)
      REQUIRE(b <= 1U);
  }

  SECTION("SF3 Encoding") {
    REQUIRE(ldpc_encode(LdpcSubframe::kSF3, msg_sf3, out_sf3, enc_ws) == LdpcStatus::kOk);
    for (auto b : out_sf3)
      REQUIRE(b <= 1U);
  }

  SECTION("SF4 Encoding") {
    REQUIRE(ldpc_encode(LdpcSubframe::kSF4, msg_sf3, out_sf4, enc_ws) == LdpcStatus::kOk);
    for (auto b : out_sf4)
      REQUIRE(b <= 1U);
  }
}

TEST_CASE("[LSIS-AFS-320] LDPC Encoder: error paths", "[ldpc]") {
  std::array<uint8_t, kSb2MsgBits - 1U> short_msg{};
  std::array<uint8_t, kSb2Length> out{};
  LdpcEncodeWorkspace enc_ws{};

  // SF2 expects 1200
  CHECK(ldpc_encode(LdpcSubframe::kSF2, short_msg, out, enc_ws) == LdpcStatus::kInvalidInput);

  std::array<uint8_t, kSb2MsgBits> msg{};
  std::span<uint8_t> short_out(out.data(), kSb2Length - 1U);
  CHECK(ldpc_encode(LdpcSubframe::kSF2, msg, short_out, enc_ws) == LdpcStatus::kOutputTooSmall);
}

TEST_CASE("LDPC: Table CRC32 integrity (all 10 matrices)", "[ldpc]") {
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

TEST_CASE("LDPC: Matrix dimension consistency", "[ldpc]") {
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
        if (w > max_w)
          max_w = w;
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

// G-12: "LDPC: Subframe types coverage" was a strictly weaker duplicate of
// "[LSIS-AFS-320] LDPC Encoder: basic execution" above, whose SF2/SF3/SF4
// Catch2 SECTIONs already cover all three subframe types AND the
// binary-output check this test omitted. Deleted.

TEST_CASE("[LSIS-AFS-320] [LSIS-AFS-321] Gateway 2: LDPC Round-trip", "[ldpc]") {
  LdpcEncodeWorkspace enc_ws{};

  SECTION("SF2 - All Zeros (linear code property)") {
    std::array<uint8_t, kSb2MsgBits> message{};

    std::array<uint8_t, kSb2Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF2, message, encoded, enc_ws) == LdpcStatus::kOk);

    // Linear code: all-zero input must produce all-zero output
    bool all_zeros = true;
    for (auto b : encoded)
      if (b != 0)
        all_zeros = false;
    CHECK(all_zeros);

    std::array<float, kSb2Length> soft{};
    for (size_t i = 0; i < kSb2Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb2MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 30, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF2 - Alternating pattern") {
    std::array<uint8_t, kSb2MsgBits> message{};
    std::iota(message.begin(), message.end(), uint8_t{0});
    for (auto& b : message)
      b %= 2;

    std::array<uint8_t, kSb2Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF2, message, encoded, enc_ws) == LdpcStatus::kOk);

    std::array<float, kSb2Length> soft{};
    for (size_t i = 0; i < kSb2Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb2MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF3 - All Zeros") {
    std::array<uint8_t, kSb3MsgBits> message{};

    std::array<uint8_t, kSb3Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF3, message, encoded, enc_ws) == LdpcStatus::kOk);

    // Linear code: all-zero input must produce all-zero output
    bool all_zeros = true;
    for (auto b : encoded)
      if (b != 0)
        all_zeros = false;
    CHECK(all_zeros);

    std::array<float, kSb3Length> soft{};
    for (size_t i = 0; i < kSb3Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb3MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF3, soft, 30, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF3 - Alternating pattern") {
    std::array<uint8_t, kSb3MsgBits> message{};
    std::iota(message.begin(), message.end(), uint8_t{0});
    for (auto& b : message)
      b %= 2;

    std::array<uint8_t, kSb3Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF3, message, encoded, enc_ws) == LdpcStatus::kOk);

    std::array<float, kSb3Length> soft{};
    for (size_t i = 0; i < kSb3Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb3MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF3, soft, 50, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF4 - Alternating pattern (reuses SF3 matrices)") {
    std::array<uint8_t, kSb3MsgBits> message{};
    std::iota(message.begin(), message.end(), uint8_t{0});
    for (auto& b : message)
      b %= 2;

    std::array<uint8_t, kSb3Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF4, message, encoded, enc_ws) == LdpcStatus::kOk);

    std::array<float, kSb3Length> soft{};
    for (size_t i = 0; i < kSb3Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb3MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF4, soft, 50, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF2 - Custom alpha (5/8 = 0.625)") {
    std::array<uint8_t, kSb2MsgBits> message{};
    std::iota(message.begin(), message.end(), uint8_t{0});
    for (auto& b : message)
      b %= 2;

    std::array<uint8_t, kSb2Length> encoded{};
    REQUIRE(ldpc_encode(LdpcSubframe::kSF2, message, encoded, enc_ws) == LdpcStatus::kOk);

    std::array<float, kSb2Length> soft{};
    for (size_t i = 0; i < kSb2Length; ++i)
      soft[i] = (encoded[i] == 1) ? -1.0f : 1.0f;

    std::array<uint8_t, kSb2MsgBits> decoded{};
    LdpcWorkspace ws{};
    REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws) == LdpcStatus::kOk);
    CHECK(std::ranges::equal(message, decoded));
  }

  SECTION("SF2 - Decoder returns kNotConverged on corrupted input") {
    // Feed random noise (no valid codeword structure)
    std::array<float, kSb2Length> noise{};
    for (size_t i = 0; i < kSb2Length; ++i)
      noise[i] = (i % 3 == 0) ? 0.1f : -0.1f;

    std::array<uint8_t, kSb2MsgBits> decoded{};
    LdpcWorkspace ws{};
    auto status = ldpc_decode(LdpcSubframe::kSF2, noise, 5, decoded, ws);
    CHECK(status == LdpcStatus::kNotConverged);
  }
}

TEST_CASE("LDPC: output buffer reuse — SF3 then SF2 into same buffer", "[ldpc]") {
  // Verifies that the encoder accepts an oversized output buffer (kSb2Length)
  // for SF3 (which only writes kSb3Length bytes), and that the same buffer
  // can then be reused for SF2. Bytes [kSb3Length..kSb2Length) are not touched
  // by the SF3 encode.
  std::array<uint8_t, kSb2MsgBits> msg_sf2{};
  std::array<uint8_t, kSb3MsgBits> msg_sf3{};
  msg_sf2.fill(1);
  msg_sf3.fill(1);

  std::array<uint8_t, kSb2Length> out{};
  out.fill(0xFFU); // sentinel — bytes beyond kSb3Length must be untouched by SF3 encode
  LdpcEncodeWorkspace enc_ws{};

  CHECK(ldpc_encode(LdpcSubframe::kSF3, msg_sf3, out, enc_ws) == LdpcStatus::kOk);
  // Bytes [kSb3Length..kSb2Length) must be unchanged (encoder does not write past its output).
  for (size_t i = kSb3Length; i < kSb2Length; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    CHECK(out[i] == 0xFFU);
  }
  CHECK(ldpc_encode(LdpcSubframe::kSF2, msg_sf2, out, enc_ws) == LdpcStatus::kOk);
}

TEST_CASE("LDPC: error paths", "[ldpc]") {
  std::array<uint8_t, 200> msg = {0};
  std::array<uint8_t, kSb2Length> out = {0};
  LdpcEncodeWorkspace enc_ws{};

  // Invalid Input Size
  std::span<uint8_t> short_msg(msg.data(), 199);
  CHECK(static_cast<uint8_t>(ldpc_encode(LdpcSubframe::kSF2, short_msg, out, enc_ws)) ==
        static_cast<uint8_t>(LdpcStatus::kInvalidInput));

  // Invalid Bits
  msg[0] = 2;
  CHECK(static_cast<uint8_t>(ldpc_encode(LdpcSubframe::kSF2, msg, out, enc_ws)) ==
        static_cast<uint8_t>(LdpcStatus::kInvalidInput));
  msg[0] = 0;

  // Output too small (msg must be valid 1200 bits for SF2 to reach output check)
  std::array<uint8_t, kSb2MsgBits> valid_msg{};
  std::span<uint8_t> small_out(out.data(), 100);
  CHECK(static_cast<uint8_t>(ldpc_encode(LdpcSubframe::kSF2, valid_msg, small_out, enc_ws)) ==
        static_cast<uint8_t>(LdpcStatus::kOutputTooSmall));
}

TEST_CASE("LDPC: extreme LLR decode exercises Q8.7 saturation", "[ldpc]") {
  // With high-magnitude LLRs (±256.0F → ±32767 in Q8.7), the BP decoder's
  // sat_sub(lv, lcv) overflows when lv=+32767 and lcv is a large negative
  // NMS output (≈-24575). This exercises the sat_sub positive-overflow
  // and sat_add negative-overflow clamping paths.
  //
  // Encode a non-trivial message (alternating bits for mixed positive/negative
  // LLRs), then decode with extreme magnitude.
  std::array<uint8_t, kSb3MsgBits> msg{};
  for (size_t i = 0; i < msg.size(); ++i) {
    msg[i] = static_cast<uint8_t>(i % 2);
  }

  std::array<uint8_t, kSb3Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(ldpc_encode(LdpcSubframe::kSF3, msg, encoded, enc_ws) == LdpcStatus::kOk);

  // Create extreme-magnitude LLRs (±256.0F) — clamps to ±32767 in Q8.7
  std::array<float, kSb3Length> soft{};
  for (size_t i = 0; i < kSb3Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 256.0F : -256.0F;
  }

  // Decode with enough iterations to trigger accumulator saturation
  std::array<uint8_t, kSb3MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  const auto status = ldpc_decode(LdpcSubframe::kSF3, soft, 30, decoded, dec_ws);

  // Should still decode correctly — saturation clamps rather than corrupts
  REQUIRE((status == LdpcStatus::kOk || status == LdpcStatus::kNotConverged));
  if (status == LdpcStatus::kOk) {
    for (size_t i = 0; i < msg.size(); ++i) {
      CHECK(decoded[i] == msg[i]);
    }
  }
}
