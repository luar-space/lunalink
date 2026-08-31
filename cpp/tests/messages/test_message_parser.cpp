// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/messages/message_parser.hpp"
#include "lunalink/messages/time_of_transmission.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace lunalink::signal;

// ─── Helper: pack a value into a bit array (MSB-first) ───────────────────────

static void pack_bits(uint8_t* dst, uint16_t offset, uint32_t value, uint8_t n_bits) {
  for (uint8_t i = 0; i < n_bits; ++i) {
    const auto bit_pos = static_cast<uint8_t>(n_bits - 1U - i);
    dst[offset + i] = static_cast<uint8_t>((value >> bit_pos) & 1U);
  }
}

// ═══ extract_bits ════════════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-801][messages] extract_bits basic", "[messages]") {
  // Pack value 0b10110 = 22 at offset 0, 5 bits
  std::array<uint8_t, 8> bits = {1, 0, 1, 1, 0, 0, 0, 0};
  uint32_t out = 0;

  REQUIRE(extract_bits(bits, 0, 5, out) == MessageParseStatus::kOk);
  REQUIRE(out == 22U);
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits at offset", "[messages]") {
  std::array<uint8_t, 16> bits{};
  // Pack 0x1A3 = 419 (10 bits) at offset 3
  pack_bits(bits.data(), 3, 0x1A3U, 10);
  uint32_t out = 0;

  REQUIRE(extract_bits(bits, 3, 10, out) == MessageParseStatus::kOk);
  REQUIRE(out == 0x1A3U);
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits single bit", "[messages]") {
  std::array<uint8_t, 4> bits = {0, 1, 0, 1};
  uint32_t out = 0;

  REQUIRE(extract_bits(bits, 1, 1, out) == MessageParseStatus::kOk);
  REQUIRE(out == 1U);

  REQUIRE(extract_bits(bits, 2, 1, out) == MessageParseStatus::kOk);
  REQUIRE(out == 0U);
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits 32 bits", "[messages]") {
  std::array<uint8_t, 32> bits{};
  pack_bits(bits.data(), 0, 0xDEADBEEFU, 32);
  uint32_t out = 0;

  REQUIRE(extract_bits(bits, 0, 32, out) == MessageParseStatus::kOk);
  REQUIRE(out == 0xDEADBEEFU);
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits out of bounds", "[messages]") {
  std::array<uint8_t, 4> bits = {1, 0, 1, 0};
  uint32_t out = 42;

  REQUIRE(extract_bits(bits, 2, 5, out) == MessageParseStatus::kInvalidInput);
  REQUIRE(out == 0U); // cleared on error
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits zero n_bits", "[messages]") {
  std::array<uint8_t, 4> bits = {1, 0, 1, 0};
  uint32_t out = 42;

  REQUIRE(extract_bits(bits, 0, 0, out) == MessageParseStatus::kInvalidInput);
  REQUIRE(out == 0U);
}

TEST_CASE("[LSIS-AFS-801][messages] extract_bits invalid bit value", "[messages]") {
  std::array<uint8_t, 4> bits = {1, 2, 0, 0}; // 2 is invalid
  uint32_t out = 0;

  REQUIRE(extract_bits(bits, 0, 4, out) == MessageParseStatus::kInvalidInput);
}

// ═══ parse_sb2 ═══════════════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 extracts WN and ITOW", "[messages]") {
  std::array<uint8_t, kSb2DataBits> sb2{};

  const uint16_t wn = 1234U;
  const uint16_t itow = 456U;

  pack_bits(sb2.data(), 0, wn, kWnBits);
  pack_bits(sb2.data(), kWnBits, itow, kItowBits);

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(sb2, result) == MessageParseStatus::kOk);
  REQUIRE(result.wn == wn);
  REQUIRE(result.itow == itow);
  REQUIRE(result.data_offset == kTotHeaderBits);
}

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 boundary WN=8191 ITOW=503", "[messages]") {
  std::array<uint8_t, kSb2DataBits> sb2{};

  pack_bits(sb2.data(), 0, kWnMax, kWnBits);
  pack_bits(sb2.data(), kWnBits, kItowMax, kItowBits);

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(sb2, result) == MessageParseStatus::kOk);
  REQUIRE(result.wn == kWnMax);
  REQUIRE(result.itow == kItowMax);
}

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 zero values", "[messages]") {
  std::array<uint8_t, kSb2DataBits> sb2{};

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(sb2, result) == MessageParseStatus::kOk);
  REQUIRE(result.wn == 0U);
  REQUIRE(result.itow == 0U);
}

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 wrong size", "[messages]") {
  std::array<uint8_t, 100> too_small{};

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(too_small, result) == MessageParseStatus::kInvalidInput);
}

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 propagates extract_bits error", "[messages]") {
  // Inject an invalid bit value (2) into the WN field region.
  // extract_bits should return kInvalidInput, and parse_sb2 must propagate it.
  std::array<uint8_t, kSb2DataBits> sb2{};
  sb2[5] = 2; // invalid bit value in WN region

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(sb2, result) == MessageParseStatus::kInvalidInput);
  REQUIRE(result.wn == 0);   // scrubbed
  REQUIRE(result.itow == 0); // scrubbed
}

TEST_CASE("[LSIS-AFS-801][messages] parse_sb2 propagates ITOW extract error", "[messages]") {
  // Valid WN region, but invalid bit in ITOW region.
  std::array<uint8_t, kSb2DataBits> sb2{};
  sb2[kWnBits + 3] = 2; // invalid bit in ITOW field

  Sb2ParseResult result{};
  REQUIRE(parse_sb2(sb2, result) == MessageParseStatus::kInvalidInput);
  REQUIRE(result.wn == 0); // scrubbed despite WN extraction succeeding
  REQUIRE(result.itow == 0);
}

// ═══ parse_sb3 ═══════════════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-802][messages] parse_sb3 extracts type field", "[messages]") {
  std::array<uint8_t, kSb3DataBits> sb3{};

  // Pack type = 5 (MSG-G5 MOrbit Almanac) into first 6 bits
  pack_bits(sb3.data(), 0, 5U, kSubframeTypeBits);

  Sb3ParseResult result{};
  REQUIRE(parse_sb3(sb3, result) == MessageParseStatus::kOk);
  REQUIRE(result.type_raw == 5U);
  REQUIRE(result.type == MessageType::kG5);
  REQUIRE(result.data_offset == kSubframeTypeBits);
}

TEST_CASE("[LSIS-AFS-802][messages] parse_sb3 all known types", "[messages]") {
  struct TypeCase {
    uint8_t raw;
    MessageType expected;
  };
  const TypeCase cases[] = {
      {1U, MessageType::kG1},   {2U, MessageType::kG2},   {3U, MessageType::kG3},
      {4U, MessageType::kG4},   {5U, MessageType::kG5},   {8U, MessageType::kG8},
      {10U, MessageType::kG10}, {11U, MessageType::kG11}, {14U, MessageType::kG14},
      {15U, MessageType::kG15}, {17U, MessageType::kG17}, {19U, MessageType::kS19},
      {20U, MessageType::kS20}, {22U, MessageType::kG22}, {23U, MessageType::kG23},
      {24U, MessageType::kG24}, {25U, MessageType::kG25}, {27U, MessageType::kG27},
      {28U, MessageType::kG28}, {29U, MessageType::kG29}, {30U, MessageType::kG30},
      {31U, MessageType::kG31}, {32U, MessageType::kG32},
  };

  for (const auto& tc : cases) {
    std::array<uint8_t, kSb3DataBits> sb3{};
    pack_bits(sb3.data(), 0, tc.raw, kSubframeTypeBits);

    Sb3ParseResult result{};
    REQUIRE(parse_sb3(sb3, result) == MessageParseStatus::kOk);
    REQUIRE(result.type == tc.expected);
  }
}

TEST_CASE("[LSIS-AFS-802][messages] parse_sb3 unknown type", "[messages]") {
  std::array<uint8_t, kSb3DataBits> sb3{};
  pack_bits(sb3.data(), 0, 63U, kSubframeTypeBits); // not a known type

  Sb3ParseResult result{};
  REQUIRE(parse_sb3(sb3, result) == MessageParseStatus::kOk);
  REQUIRE(result.type == MessageType::kUnknown);
  REQUIRE(result.type_raw == 63U);
}

TEST_CASE("[LSIS-AFS-802][messages] parse_sb3 wrong size", "[messages]") {
  std::array<uint8_t, 100> too_small{};

  Sb3ParseResult result{};
  REQUIRE(parse_sb3(too_small, result) == MessageParseStatus::kInvalidInput);
}

TEST_CASE("[LSIS-AFS-802][messages] parse_sb3 propagates extract_bits error", "[messages]") {
  std::array<uint8_t, kSb3DataBits> sb3{};
  sb3[2] = 2; // invalid bit value in type field

  Sb3ParseResult result{};
  REQUIRE(parse_sb3(sb3, result) == MessageParseStatus::kInvalidInput);
  REQUIRE(result.type_raw == 0); // scrubbed
}

// ═══ parse_sb4 ═══════════════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-803][messages] parse_sb4 extracts type field", "[messages]") {
  std::array<uint8_t, kSb4DataBits> sb4{};

  // Pack type = 1 (MSG-G1 Network Access Info)
  pack_bits(sb4.data(), 0, 1U, kSubframeTypeBits);

  Sb4ParseResult result{};
  REQUIRE(parse_sb4(sb4, result) == MessageParseStatus::kOk);
  REQUIRE(result.type_raw == 1U);
  REQUIRE(result.type == MessageType::kG1);
  REQUIRE(result.data_offset == kSubframeTypeBits);
}

TEST_CASE("[LSIS-AFS-803][messages] parse_sb4 wrong size", "[messages]") {
  std::array<uint8_t, 100> too_small{};

  Sb4ParseResult result{};
  REQUIRE(parse_sb4(too_small, result) == MessageParseStatus::kInvalidInput);
}

TEST_CASE("[LSIS-AFS-803][messages] parse_sb4 propagates extract_bits error", "[messages]") {
  std::array<uint8_t, kSb4DataBits> sb4{};
  sb4[1] = 2; // invalid bit value in type field

  Sb4ParseResult result{};
  REQUIRE(parse_sb4(sb4, result) == MessageParseStatus::kInvalidInput);
  REQUIRE(result.type_raw == 0); // scrubbed
}

// ═══ validate_spare_bits ═════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-801][messages] validate_spare_bits correct pattern", "[messages]") {
  // LSIS-300: alternating 0/1, starting with 0 on MSB
  std::array<uint8_t, 8> spare = {0, 1, 0, 1, 0, 1, 0, 1};
  REQUIRE(validate_spare_bits(spare));
}

TEST_CASE("[LSIS-AFS-801][messages] validate_spare_bits incorrect pattern", "[messages]") {
  std::array<uint8_t, 8> bad = {1, 0, 1, 0, 1, 0, 1, 0}; // starts with 1
  REQUIRE_FALSE(validate_spare_bits(bad));
}

TEST_CASE("[LSIS-AFS-801][messages] validate_spare_bits empty rejects", "[messages]") {
  REQUIRE_FALSE(validate_spare_bits(std::span<const uint8_t>{}));
}

TEST_CASE("[LSIS-AFS-801][messages] validate_spare_bits single bit", "[messages]") {
  std::array<uint8_t, 1> zero = {0};
  std::array<uint8_t, 1> one = {1};
  REQUIRE(validate_spare_bits(zero));
  REQUIRE_FALSE(validate_spare_bits(one));
}

// ═══ type_field_to_message_type ══════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-802][messages] type_field_to_message_type unknown values", "[messages]") {
  REQUIRE(type_field_to_message_type(0U) == MessageType::kUnknown);
  REQUIRE(type_field_to_message_type(6U) == MessageType::kUnknown);
  REQUIRE(type_field_to_message_type(7U) == MessageType::kUnknown);
  REQUIRE(type_field_to_message_type(9U) == MessageType::kUnknown);
  REQUIRE(type_field_to_message_type(255U) == MessageType::kUnknown);
}
