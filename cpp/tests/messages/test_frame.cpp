// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/messages/message_parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>

using namespace lunalink::signal;

// ===========================================================================
// Existing tests: frame_build_partial
// ===========================================================================

TEST_CASE("[LSIS-AFS-601] frame_build_partial returns kOk for valid inputs", "[frame]") {
  std::array<uint8_t, kFrameLength> frame{};
  FrameBuildWorkspace fbw{};
  CHECK(frame_build_partial(Fid::kNode1(), Toi(0), frame, fbw) == FrameStatus::kOk);
  CHECK(frame_build_partial(Fid::kNode4(), Toi(99), frame, fbw) == FrameStatus::kOk);
}

TEST_CASE("[LSIS-AFS-601] frame_build_partial SB1 region matches standalone bch_encode",
          "[frame]") {
  std::array<uint8_t, kFrameLength> frame{};
  FrameBuildWorkspace fbw{};
  std::array<uint8_t, kBchCodewordLength> expected_bch{};

  const Fid fid = Fid::kNode2();
  const Toi toi{42};

  REQUIRE(bch_encode(fid, toi, expected_bch) == BchStatus::kOk);
  REQUIRE(frame_build_partial(fid, toi, frame, fbw) == FrameStatus::kOk);

  auto sb1_region = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
  CHECK(std::ranges::equal(sb1_region, expected_bch));
}

TEST_CASE("[LSIS-AFS-601] frame_build_partial SB1 with spec test vector (FID=0, TOI=69)",
          "[frame]") {
  constexpr std::array<uint8_t, kBchCodewordLength> expected_bch = {
      0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
  };

  std::array<uint8_t, kFrameLength> frame{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build_partial(Fid::kNode1(), Toi(69), frame, fbw) == FrameStatus::kOk);

  auto sb1_region = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
  CHECK(std::ranges::equal(sb1_region, expected_bch));
}

TEST_CASE("[LSIS-AFS-601] frame_build_partial SB2-SB4 region is zero-padded", "[frame]") {
  std::array<uint8_t, kFrameLength> frame{};
  FrameBuildWorkspace fbw{};
  frame.fill(0xFF);
  REQUIRE(frame_build_partial(Fid::kNode1(), Toi(0), frame, fbw) == FrameStatus::kOk);

  const auto payload_start = static_cast<uint16_t>(kSyncLength + kSb1Length);
  for (uint16_t i = payload_start; i < kFrameLength; ++i) {
    REQUIRE(frame[i] == 0U);
  }
}

TEST_CASE("[LSIS-AFS-601] frame_build_partial exhaustive FID x TOI", "[frame]") {
  std::array<uint8_t, kFrameLength> frame{};
  FrameBuildWorkspace fbw{};
  for (uint8_t fid_val = 0; fid_val < 4; ++fid_val) {
    for (uint8_t toi_val = 0; toi_val < 100; ++toi_val) {
      REQUIRE(frame_build_partial(
                  static_cast<Fid>(static_cast<uint8_t>(fid_val)), Toi(toi_val), frame, fbw) ==
              FrameStatus::kOk);
    }
  }
}

// ===========================================================================
// Gateway 3: Subframe builders
// ===========================================================================

TEST_CASE("[LSIS-AFS-602] subframe_build_sb2 returns kOk for zero data", "[frame]") {
  std::array<uint8_t, kSb2DataBits> data{};
  std::array<uint8_t, kSb2Length> encoded{};
  LdpcEncodeWorkspace ws{};

  CHECK(subframe_build_sb2(data, encoded, ws) == FrameStatus::kOk);
}

TEST_CASE("[LSIS-AFS-603] subframe_build_sb3 returns kOk for zero data", "[frame]") {
  std::array<uint8_t, kSb3DataBits> data{};
  std::array<uint8_t, kSb3Length> encoded{};
  LdpcEncodeWorkspace ws{};

  CHECK(subframe_build_sb3(data, encoded, ws) == FrameStatus::kOk);
}

TEST_CASE("[LSIS-AFS-604] subframe_build_sb4 returns kOk for zero data", "[frame]") {
  std::array<uint8_t, kSb4DataBits> data{};
  std::array<uint8_t, kSb4Length> encoded{};
  LdpcEncodeWorkspace ws{};

  CHECK(subframe_build_sb4(data, encoded, ws) == FrameStatus::kOk);
}

TEST_CASE("[LSIS-AFS-602] subframe_build_sb2 output is binary", "[frame]") {
  std::array<uint8_t, kSb2DataBits> data{};
  // Set some non-trivial pattern
  for (size_t i = 0; i < data.size(); i += 3) {
    data[i] = 1;
  }
  std::array<uint8_t, kSb2Length> encoded{};
  LdpcEncodeWorkspace ws{};

  REQUIRE(subframe_build_sb2(data, encoded, ws) == FrameStatus::kOk);

  for (size_t i = 0; i < kSb2Length; ++i) {
    REQUIRE(encoded[i] <= 1U);
  }
}

TEST_CASE("[LSIS-AFS-602] subframe_build_sb2 round-trip: encode, decode, verify CRC", "[frame]") {
  // Prepare test data with a non-trivial bit pattern
  std::array<uint8_t, kSb2DataBits> data{};
  for (size_t i = 0; i < data.size(); i += 5) {
    data[i] = 1;
  }

  // Encode
  std::array<uint8_t, kSb2Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(subframe_build_sb2(data, encoded, enc_ws) == FrameStatus::kOk);

  // Convert binary symbols to soft LLRs (perfect channel)
  // Positive = likely bit 0, Negative = likely bit 1
  std::array<float, kSb2Length> soft{};
  for (size_t i = 0; i < kSb2Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 10.0F : -10.0F;
  }

  // Decode
  std::array<uint8_t, kSb2MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  const auto ldpc_status = ldpc_decode(LdpcSubframe::kSF2, soft, 20, decoded, dec_ws);
  REQUIRE((ldpc_status == LdpcStatus::kOk || ldpc_status == LdpcStatus::kNotConverged));

  // Verify data recovery: first kSb2DataBits must match original, except
  // for the 26-bit spare region which the builder overwrites with the
  // LSIS-300 alternating 0/1 pattern (G-18).
  for (size_t i = 0; i < kSb2SpareOffset; ++i) {
    REQUIRE(decoded[i] == data[i]);
  }
  for (uint8_t i = 0; i < kSb2SpareBits; ++i) {
    REQUIRE(decoded[kSb2SpareOffset + i] == static_cast<uint8_t>(i % 2U));
  }

  // Verify CRC-24: the full decoded message (data + CRC) must pass
  CHECK(crc24_verify(decoded));
}

TEST_CASE("[LSIS-AFS-603] subframe_build_sb3 round-trip: encode, decode, verify CRC", "[frame]") {
  std::array<uint8_t, kSb3DataBits> data{};
  for (size_t i = 0; i < data.size(); i += 7) {
    data[i] = 1;
  }

  std::array<uint8_t, kSb3Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(subframe_build_sb3(data, encoded, enc_ws) == FrameStatus::kOk);

  std::array<float, kSb3Length> soft{};
  for (size_t i = 0; i < kSb3Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 10.0F : -10.0F;
  }

  std::array<uint8_t, kSb3MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  const auto ldpc_status = ldpc_decode(LdpcSubframe::kSF3, soft, 20, decoded, dec_ws);
  REQUIRE((ldpc_status == LdpcStatus::kOk || ldpc_status == LdpcStatus::kNotConverged));

  for (size_t i = 0; i < kSb3DataBits; ++i) {
    REQUIRE(decoded[i] == data[i]);
  }
  CHECK(crc24_verify(decoded));
}

TEST_CASE("[LSIS-AFS-604] subframe_build_sb4 round-trip: encode, decode, verify CRC", "[frame]") {
  std::array<uint8_t, kSb4DataBits> data{};
  for (size_t i = 1; i < data.size(); i += 11) {
    data[i] = 1;
  }

  std::array<uint8_t, kSb4Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(subframe_build_sb4(data, encoded, enc_ws) == FrameStatus::kOk);

  std::array<float, kSb4Length> soft{};
  for (size_t i = 0; i < kSb4Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 10.0F : -10.0F;
  }

  std::array<uint8_t, kSb4MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  const auto ldpc_status = ldpc_decode(LdpcSubframe::kSF4, soft, 20, decoded, dec_ws);
  REQUIRE((ldpc_status == LdpcStatus::kOk || ldpc_status == LdpcStatus::kNotConverged));

  for (size_t i = 0; i < kSb4DataBits; ++i) {
    REQUIRE(decoded[i] == data[i]);
  }
  CHECK(crc24_verify(decoded));
}

// ===========================================================================
// Gateway 3: Full frame assembler
// ===========================================================================

TEST_CASE("[LSIS-AFS-605] frame_build returns kOk for valid inputs", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace ws{};
  FrameBuildWorkspace fbw{};

  CHECK(frame_build(Fid::kNode1(), Toi(0), sb2, sb3, sb4, frame, ws, fbw) == FrameStatus::kOk);
}

TEST_CASE("[LSIS-AFS-605] frame_build all symbols are binary", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace ws{};
  FrameBuildWorkspace fbw{};

  // Set non-trivial data
  for (size_t i = 0; i < sb2.size(); i += 3) {
    sb2[i] = 1;
  }
  for (size_t i = 0; i < sb3.size(); i += 5) {
    sb3[i] = 1;
  }
  for (size_t i = 0; i < sb4.size(); i += 7) {
    sb4[i] = 1;
  }

  REQUIRE(frame_build(Fid::kNode2(), Toi(42), sb2, sb3, sb4, frame, ws, fbw) == FrameStatus::kOk);

  for (size_t i = 0; i < kFrameLength; ++i) {
    REQUIRE(frame[i] <= 1U);
  }
}

TEST_CASE("[LSIS-AFS-605] frame_build SB1 region matches standalone bch_encode", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace ws{};
  FrameBuildWorkspace fbw{};

  const Fid fid = Fid::kNode3();
  const Toi toi{77};

  REQUIRE(frame_build(fid, toi, sb2, sb3, sb4, frame, ws, fbw) == FrameStatus::kOk);

  // Verify SB1 matches standalone BCH encode
  std::array<uint8_t, kBchCodewordLength> expected_bch{};
  REQUIRE(bch_encode(fid, toi, expected_bch) == BchStatus::kOk);

  auto sb1_region = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
  CHECK(std::ranges::equal(sb1_region, expected_bch));
}

TEST_CASE("[LSIS-AFS-605] frame_build accepts all valid FID values", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace ws{};
  FrameBuildWorkspace fbw{};

  for (uint8_t fid_val = 0; fid_val < 4; ++fid_val) {
    CHECK(frame_build(Fid(fid_val), Toi(0), sb2, sb3, sb4, frame, ws, fbw) == FrameStatus::kOk);
  }
}

TEST_CASE("[LSIS-AFS-605] frame_build symbol count: SP(68) + SB1(52) + payload(5880) = 6000",
          "[frame]") {
  // Verify the compile-time constants sum correctly
  static_assert(kSyncLength + kSb1Length + kPayloadLength == kFrameLength);
  static_assert(kSb2Length + kSb3Length + kSb4Length == kPayloadLength);
}

TEST_CASE("[LSIS-AFS-605] frame_build payload round-trip: deinterleave, decode, verify",
          "[frame]") {
  // Prepare distinct data for each subframe
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  for (size_t i = 0; i < sb2_data.size(); i += 3) {
    sb2_data[i] = 1;
  }
  for (size_t i = 0; i < sb3_data.size(); i += 5) {
    sb3_data[i] = 1;
  }
  for (size_t i = 0; i < sb4_data.size(); i += 7) {
    sb4_data[i] = 1;
  }

  // Build full frame
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(42), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  // Extract and deinterleave the payload region
  constexpr size_t payload_offset = kSyncLength + kSb1Length; // 120
  std::array<uint8_t, kPayloadLength> deinterleaved{};
  REQUIRE(block_deinterleave(std::span(frame).subspan(payload_offset, kPayloadLength),
                             deinterleaved) == InterleaverStatus::kOk);

  // Decode SB2 (first 2400 symbols of deinterleaved payload)
  {
    std::array<float, kSb2Length> soft{};
    for (size_t i = 0; i < kSb2Length; ++i) {
      soft[i] = (deinterleaved[i] == 0U) ? 10.0F : -10.0F;
    }
    std::array<uint8_t, kSb2MsgBits> decoded{};
    LdpcWorkspace dec_ws{};
    const auto st = ldpc_decode(LdpcSubframe::kSF2, soft, 20, decoded, dec_ws);
    REQUIRE((st == LdpcStatus::kOk || st == LdpcStatus::kNotConverged));

    // First kSb2SpareOffset bits must match the caller; the spare region
    // is overwritten by the builder to the LSIS-300 pattern (G-18).
    for (size_t i = 0; i < kSb2SpareOffset; ++i) {
      REQUIRE(decoded[i] == sb2_data[i]);
    }
    for (uint8_t i = 0; i < kSb2SpareBits; ++i) {
      REQUIRE(decoded[kSb2SpareOffset + i] == static_cast<uint8_t>(i % 2U));
    }
    CHECK(crc24_verify(decoded));
  }

  // Decode SB3 (next 1740 symbols)
  {
    std::array<float, kSb3Length> soft{};
    for (size_t i = 0; i < kSb3Length; ++i) {
      soft[i] = (deinterleaved[kSb2Length + i] == 0U) ? 10.0F : -10.0F;
    }
    std::array<uint8_t, kSb3MsgBits> decoded{};
    LdpcWorkspace dec_ws{};
    const auto st = ldpc_decode(LdpcSubframe::kSF3, soft, 20, decoded, dec_ws);
    REQUIRE((st == LdpcStatus::kOk || st == LdpcStatus::kNotConverged));

    for (size_t i = 0; i < kSb3DataBits; ++i) {
      REQUIRE(decoded[i] == sb3_data[i]);
    }
    CHECK(crc24_verify(decoded));
  }

  // Decode SB4 (last 1740 symbols)
  {
    std::array<float, kSb4Length> soft{};
    for (size_t i = 0; i < kSb4Length; ++i) {
      soft[i] = (deinterleaved[kSb2Length + kSb3Length + i] == 0U) ? 10.0F : -10.0F;
    }
    std::array<uint8_t, kSb4MsgBits> decoded{};
    LdpcWorkspace dec_ws{};
    const auto st = ldpc_decode(LdpcSubframe::kSF4, soft, 20, decoded, dec_ws);
    REQUIRE((st == LdpcStatus::kOk || st == LdpcStatus::kNotConverged));

    for (size_t i = 0; i < kSb4DataBits; ++i) {
      REQUIRE(decoded[i] == sb4_data[i]);
    }
    CHECK(crc24_verify(decoded));
  }

  // Also verify SB1 via BCH decode
  {
    auto sb1_span = std::span(frame).subspan(kSyncLength, kBchCodewordLength);
    std::array<uint8_t, kBchCodewordLength> sb1_arr{};
    std::ranges::copy(sb1_span, sb1_arr.begin());
    const auto result = bch_decode(sb1_arr);
    CHECK(result.status == BchStatus::kOk);
    CHECK(static_cast<uint8_t>(result.fid) == 0U);
    CHECK(static_cast<uint8_t>(result.toi) == 42U);
  }
}

// ===========================================================================
// LSIS-300 SB2 spare-bit compliance: the *builder* must emit the
// alternating 0/1 pattern on the wire regardless of what the caller
// supplied in the spare region. G-18 resolved 2026-04-13 by
// `enforce_spare_bit_pattern` in `cpp/messages/frame.cpp`. SB3/SB4
// spare regions are message-type-specific (TBW in LSIS V1.0) so the
// builder cannot enforce them without per-message-type knowledge; the
// SB3 round-trip below remains a CRC/LDPC-transparency check only.
// ===========================================================================

TEST_CASE("[LSIS-AFS-300] SB2 builder enforces LSIS-300 pattern regardless of caller input",
          "[frame]") {
  // Caller supplies all-zero spare bits — the wrong pattern for LSIS-300.
  // After encode/decode the spare region must still carry 0,1,0,1,... per
  // spec, proving the builder — not the caller — is the enforcement point.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  for (size_t i = 0; i < kSb2SpareOffset; ++i) {
    sb2_data[i] = static_cast<uint8_t>(i % 3U == 0U ? 1U : 0U); // realistic payload
  }
  // sb2_data[kSb2SpareOffset..] already zero — deliberately non-compliant.

  std::array<uint8_t, kSb2Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(subframe_build_sb2(sb2_data, encoded, enc_ws) == FrameStatus::kOk);

  std::array<float, kSb2Length> soft{};
  for (size_t i = 0; i < kSb2Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 10.0F : -10.0F;
  }
  std::array<uint8_t, kSb2MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 20, decoded, dec_ws) == LdpcStatus::kOk);

  const auto spare_span = std::span<const uint8_t>(decoded).subspan(kSb2SpareOffset, kSb2SpareBits);
  CHECK(validate_spare_bits(spare_span));
  CHECK(crc24_verify(decoded));
}

TEST_CASE("[LSIS-AFS-300] frame_build emits LSIS-300 pattern in the SB2 spare region", "[frame]") {
  // End-to-end through the full-frame assembler: spare bits must still
  // come out right even though the caller's sb2_data is all ones.
  std::array<uint8_t, kSb2DataBits> sb2{};
  sb2.fill(1U);
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> out{};
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid(0), Toi(10), sb2, sb3, sb4, out, ldpc_ws, fbw) == FrameStatus::kOk);

  FrameDecodeResult result{};
  auto dec_ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  std::array<float, kFrameLength> soft{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    soft[i] = (out[i] == 0U) ? 10.0F : -10.0F;
  }
  REQUIRE(frame_decode(soft, result, *dec_ws, hal) == FrameDecodeStatus::kOk);

  REQUIRE(result.sb2.crc_ok);
  const auto spare_span =
      std::span<const uint8_t>(dec_ws->sb2_data).subspan(kSb2SpareOffset, kSb2SpareBits);
  CHECK(validate_spare_bits(spare_span));
}

TEST_CASE("SB3 round-trip preserves arbitrary spare-region bit patterns "
          "(SB3 spare layout is message-type-specific — builder enforcement not applicable)",
          "[frame]") {
  // The data below happens to be the LSIS-300 alternating pattern, but
  // the test would pass for any bit pattern the caller supplies. If you
  // are looking for the test that proves LSIS-300 compliance on the
  // transmitted wire, it does not exist yet — see G-18.

  // SB2: 1176 data bits. Fill first 100 as "real" data, rest as spare.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  for (size_t i = 0; i < 100; ++i) {
    sb2_data[i] = 1;
  } // Some payload
  // Fill spare region (bits 100..1175) with LSIS-300 pattern: 0,1,0,1,...
  for (size_t i = 100; i < kSb2DataBits; ++i) {
    sb2_data[i] = static_cast<uint8_t>((i - 100) % 2);
  }

  // Encode SB2 and verify round-trip recovers the exact spare pattern
  std::array<uint8_t, kSb2Length> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(subframe_build_sb2(sb2_data, encoded, enc_ws) == FrameStatus::kOk);

  // Decode (perfect channel)
  std::array<float, kSb2Length> soft{};
  for (size_t i = 0; i < kSb2Length; ++i) {
    soft[i] = (encoded[i] == 0U) ? 10.0F : -10.0F;
  }
  std::array<uint8_t, kSb2MsgBits> decoded{};
  LdpcWorkspace dec_ws{};
  REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 20, decoded, dec_ws) == LdpcStatus::kOk);

  // Verify spare bits survived CRC + LDPC round-trip intact
  for (size_t i = 100; i < kSb2DataBits; ++i) {
    REQUIRE(decoded[i] == static_cast<uint8_t>((i - 100) % 2));
  }
  CHECK(crc24_verify(decoded));

  // Same check for SB3
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  for (size_t i = 0; i < kSb3DataBits; ++i) {
    sb3_data[i] = static_cast<uint8_t>(i % 2); // 0,1,0,1,...
  }
  std::array<uint8_t, kSb3Length> sb3_enc{};
  REQUIRE(subframe_build_sb3(sb3_data, sb3_enc, enc_ws) == FrameStatus::kOk);

  std::array<float, kSb3Length> sb3_soft{};
  for (size_t i = 0; i < kSb3Length; ++i) {
    sb3_soft[i] = (sb3_enc[i] == 0U) ? 10.0F : -10.0F;
  }
  std::array<uint8_t, kSb3MsgBits> sb3_dec{};
  REQUIRE(ldpc_decode(LdpcSubframe::kSF3, sb3_soft, 20, sb3_dec, dec_ws) == LdpcStatus::kOk);

  for (size_t i = 0; i < kSb3DataBits; ++i) {
    REQUIRE(sb3_dec[i] == static_cast<uint8_t>(i % 2));
  }
  CHECK(crc24_verify(sb3_dec));
}

TEST_CASE("frame_decode: input too small rejected", "[frame]") {
  std::array<float, 100> short_rx{};
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  CHECK(frame_decode(short_rx, result, *ws, hal) == FrameDecodeStatus::kInvalidInput);
}

TEST_CASE("frame_decode: sync not found on zero input", "[frame]") {
  auto rx = std::make_unique<std::array<float, kFrameLength + 100>>();
  rx->fill(0.0F);
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);
  CHECK((status == FrameDecodeStatus::kSyncNotFound || status == FrameDecodeStatus::kBchFailed));
}

TEST_CASE("frame_build_partial: validates FID/TOI", "[frame]") {
  std::array<uint8_t, 6000> frame{};
  FrameBuildWorkspace fbw{};
  CHECK(frame_build_partial(Fid(0), Toi(0), frame, fbw) == FrameStatus::kOk);
}

TEST_CASE("frame_build: full build with non-zero data", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  sb2.fill(1);
  sb3.fill(0);
  sb4.fill(1);
  std::array<uint8_t, kFrameLength> out{};
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};
  auto status = frame_build(Fid(1), Toi(50), sb2, sb3, sb4, out, ldpc_ws, fbw);
  CHECK(status == FrameStatus::kOk);
}

TEST_CASE("commit_frame: CRC failure detection", "[frame]") {
  std::array<uint8_t, 100> data{};
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i & 1);
  }
  std::array<uint8_t, 24> crc{};
  REQUIRE(crc24_calculate(data, crc) == CrcStatus::kOk);

  std::array<uint8_t, 124> buf{};
  for (size_t i = 0; i < 100; ++i)
    buf[i] = data[i];
  for (size_t i = 0; i < 24; ++i)
    buf[100 + i] = crc[i];

  CHECK(commit_frame(buf) == FrameStatus::kOk);
  buf[50] ^= 1;
  CHECK(commit_frame(buf) == FrameStatus::kFaultDetected);
}

TEST_CASE("frame_build_partial: invalid FID/TOI rejected", "[frame]") {
  std::array<uint8_t, 6000> frame{};

  Fid bad_fid(0);
  uint8_t bad_val = 250;
  std::memcpy(&bad_fid.storage.v1, &bad_val, 1);
  std::memcpy(&bad_fid.storage.v2, &bad_val, 1);
  std::memcpy(&bad_fid.storage.v3, &bad_val, 1);
  FrameBuildWorkspace fbw{};
  CHECK(frame_build_partial(bad_fid, Toi(0), frame, fbw) == FrameStatus::kInvalidFid);

  Toi bad_toi(0);
  uint8_t bad_toi_val = 250;
  std::memcpy(&bad_toi.storage.v1, &bad_toi_val, 1);
  std::memcpy(&bad_toi.storage.v2, &bad_toi_val, 1);
  std::memcpy(&bad_toi.storage.v3, &bad_toi_val, 1);
  CHECK(frame_build_partial(Fid(0), bad_toi, frame, fbw) == FrameStatus::kInvalidToi);
}

// ===========================================================================
// Coverage: frame_decode round-trip (exercises full RX pipeline)
// ===========================================================================

namespace {
/// Convert binary symbols (0/1) to bipolar soft symbols (+1/-1).
void binary_to_bipolar(std::span<const uint8_t> in, std::span<float> out) {
  for (size_t i = 0; i < in.size(); ++i) {
    out[i] = (in[i] == 0U) ? 1.0F : -1.0F;
  }
}
} // namespace

TEST_CASE("[LSIS-AFS-720] frame_decode: clean round-trip recovers data", "[frame]") {
  // Prepare distinct data for each subframe
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::mt19937 rng(42);
  for (auto& b : sb2_data) {
    b = static_cast<uint8_t>(rng() & 1U);
  }
  for (auto& b : sb3_data) {
    b = static_cast<uint8_t>(rng() & 1U);
  }
  for (auto& b : sb4_data) {
    b = static_cast<uint8_t>(rng() & 1U);
  }

  // TX: build a complete frame
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  const Fid fid = Fid::kNode2();
  const Toi toi{73};
  REQUIRE(frame_build(fid, toi, sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  // Convert to soft bipolar symbols (perfect channel)
  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  // RX: decode the frame
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  REQUIRE(status == FrameDecodeStatus::kOk);
  CHECK(result.status == FrameDecodeStatus::kOk);

  // Verify sync detection
  CHECK(result.sync.detected);
  CHECK(result.sync.frame_offset == 0);

  // Verify BCH (SB1) decoded correctly
  CHECK(result.bch.status == BchStatus::kOk);
  CHECK(result.bch.fid == fid);
  CHECK(result.bch.toi == toi);

  // Verify all subframes decoded and CRC-verified
  CHECK(result.sb2.ldpc_converged);
  CHECK(result.sb2.crc_ok);
  CHECK(result.sb3.ldpc_converged);
  CHECK(result.sb3.crc_ok);
  CHECK(result.sb4.ldpc_converged);
  CHECK(result.sb4.crc_ok);

  // Verify recovered data matches original, except for the SB2 spare
  // region which the builder rewrites to the LSIS-300 pattern (G-18).
  for (size_t i = 0; i < kSb2SpareOffset; ++i) {
    REQUIRE(ws->sb2_data[i] == sb2_data[i]);
  }
  for (uint8_t i = 0; i < kSb2SpareBits; ++i) {
    REQUIRE(ws->sb2_data[kSb2SpareOffset + i] == static_cast<uint8_t>(i % 2U));
  }
  for (size_t i = 0; i < kSb3DataBits; ++i) {
    REQUIRE(ws->sb3_data[i] == sb3_data[i]);
  }
  for (size_t i = 0; i < kSb4DataBits; ++i) {
    REQUIRE(ws->sb4_data[i] == sb4_data[i]);
  }
}

TEST_CASE("[LSIS-AFS-720] frame_decode: clean round-trip with offset", "[frame]") {
  // Test that frame_decode works when sync pattern is not at position 0
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};

  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(0), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  // Prepend 200 symbols of noise, then the frame
  constexpr size_t kOffset = 200;
  auto rx = std::make_unique<std::array<float, kFrameLength + kOffset>>();
  // Fill prefix with small random-ish values (not matching sync)
  for (size_t i = 0; i < kOffset; ++i) {
    (*rx)[i] = 0.1F;
  }
  // Append frame as bipolar
  for (size_t i = 0; i < kFrameLength; ++i) {
    (*rx)[kOffset + i] = (frame[i] == 0U) ? 1.0F : -1.0F;
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  REQUIRE(status == FrameDecodeStatus::kOk);
  CHECK(result.sync.frame_offset == kOffset);
  CHECK(result.bch.status == BchStatus::kOk);
  CHECK(result.sb2.ldpc_converged);
  CHECK(result.sb2.crc_ok);
}

TEST_CASE("frame_decode: BCH decodes with noisy SB1 (wrong FID/TOI)", "[frame]") {
  // Build a valid frame with FID=1, TOI=42
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode2(), Toi(42), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  // Convert to soft and flip ~15 SB1 symbols (exceeds BCH correction capability)
  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);
  for (size_t i = kSyncLength; i < kSyncLength + 15; ++i) {
    (*rx)[i] = -(*rx)[i];
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  // BCH ML decoder always finds a codeword, but with >7 errors it may decode
  // to the wrong FID/TOI. The pipeline continues past BCH to LDPC/CRC.
  // We just verify the pipeline doesn't crash and runs to completion.
  CHECK(status != FrameDecodeStatus::kInvalidInput);
  CHECK(status != FrameDecodeStatus::kSyncNotFound);
}

TEST_CASE("frame_decode: NaN in SB1 causes BCH failure", "[frame]") {
  // bch_decode_soft returns kInvalidInput when LLRs contain NaN/Inf.
  // This exercises the kBchFailed path in frame_decode.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(0), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  // Inject NaN into the SB1 region (after sync pattern).
  // NaN comparisons are always false, so the sync detector still finds the
  // clean sync pattern at offset 0. bch_decode_soft then returns kInvalidInput
  // on the non-finite LLRs, which frame_decode maps to kBchFailed.
  (*rx)[kSyncLength + 5] = std::numeric_limits<float>::quiet_NaN();
  (*rx)[kSyncLength + 20] = std::numeric_limits<float>::quiet_NaN();

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  CHECK(status == FrameDecodeStatus::kBchFailed);
}

TEST_CASE("frame_decode: LDPC fails (not kOk) when payload replaced with noise", "[frame]") {
  // G-08 rewrite: the earlier version used zero-filled SB2/SB3/SB4 data,
  // so the all-zero codeword was the valid transmitted codeword, and a
  // decoder given noise could legitimately converge back to it — the
  // test then accepted `kOk || kLdpcFailed || kCrcFailed` and had zero
  // discriminating power. Filling the subframes with a non-trivial
  // deterministic pattern makes the all-zero codeword a WRONG decode,
  // so converging to it fails CRC. After payload replacement the
  // decoder must now fail at LDPC or CRC; `kOk` must not occur.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  // Deterministic alternating-bit fill — no RNG dependency, easy to
  // reproduce in a debugger.
  for (size_t i = 0; i < kSb2DataBits; ++i)
    sb2_data[i] = static_cast<uint8_t>(i & 1U);
  for (size_t i = 0; i < kSb3DataBits; ++i)
    sb3_data[i] = static_cast<uint8_t>((i + 1U) & 1U);
  for (size_t i = 0; i < kSb4DataBits; ++i)
    sb4_data[i] = static_cast<uint8_t>(i & 1U);

  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(0), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  // Replace payload with near-zero soft values — the decoder cannot
  // recover the non-zero transmitted codeword from pure noise.
  std::mt19937 rng(123);
  std::normal_distribution<float> noise(0.0F, 0.01F);
  const size_t payload_start = kSyncLength + kSb1Length;
  for (size_t i = payload_start; i < kFrameLength; ++i) {
    (*rx)[i] = noise(rng);
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal, 5);

  CHECK(status != FrameDecodeStatus::kOk);
  CHECK((status == FrameDecodeStatus::kLdpcFailed || status == FrameDecodeStatus::kCrcFailed));
}

TEST_CASE("frame_decode: classify result distinguishes LDPC and CRC failures", "[frame]") {
  // G-08 rewrite: same problem as above — zero-filled subframes let the
  // decoder return kOk with only 1 iteration and made the test
  // tautological. Build a non-trivial frame, then corrupt just the SB2
  // LDPC payload with bit flips so LDPC gives up with a detectable
  // failure (not kOk) while sync + BCH still pass.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  for (size_t i = 0; i < kSb2DataBits; ++i)
    sb2_data[i] = static_cast<uint8_t>(i & 1U);
  for (size_t i = 0; i < kSb3DataBits; ++i)
    sb3_data[i] = static_cast<uint8_t>((i + 1U) & 1U);
  for (size_t i = 0; i < kSb4DataBits; ++i)
    sb4_data[i] = static_cast<uint8_t>(i & 1U);

  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(0), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  // Flip every second bit of the SB2 payload region — enough corruption
  // that 1 LDPC iteration cannot recover the codeword.
  const size_t payload_start = kSyncLength + kSb1Length;
  for (size_t i = payload_start; i < payload_start + kSb2Length; i += 2) {
    (*rx)[i] = -(*rx)[i];
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal, 1);

  // The decoder must flag the failure — not silently return kOk.
  CHECK(status != FrameDecodeStatus::kOk);
  CHECK((status == FrameDecodeStatus::kLdpcFailed || status == FrameDecodeStatus::kCrcFailed));
}

// ===========================================================================
// Coverage: frame_build error paths through full builder
// ===========================================================================

TEST_CASE("frame_decode: insufficient symbols after sync offset", "[frame]") {
  // Plant a sync pattern partway into a buffer that's too short for a full frame.
  // The sync detector finds it, but offset + kFrameLength > rx.size() → kInvalidInput.
  constexpr size_t kBufSize = kFrameLength + 50;
  constexpr size_t kSyncOffset = 100; // sync at offset 100, need 6000 more, but only 5950 remain

  auto rx = std::make_unique<std::array<float, kBufSize>>();
  rx->fill(0.1F); // background

  // Plant bipolar sync pattern at offset
  for (size_t i = 0; i < kSyncLength; ++i) {
    (*rx)[kSyncOffset + i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  CHECK(status == FrameDecodeStatus::kInvalidInput);
}

TEST_CASE("frame_decode: non-finite sync threshold rejected", "[frame]") {
  // set_threshold rejects non-finite values. frame_decode forwards
  // sync_threshold > 0 to set_threshold, so +Inf returns kFaultDetected.
  // (NaN is not > 0 so it skips the setter entirely — tested separately.)
  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  rx->fill(0.0F);
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  CHECK(frame_decode(*rx,
                     result,
                     *ws,
                     hal,
                     kFrameDecodeLdpcIterations,
                     std::numeric_limits<float>::infinity()) == FrameDecodeStatus::kFaultDetected);
}

// ===========================================================================
// Coverage: frame_build error paths through full builder
// ===========================================================================

TEST_CASE("frame_build: invalid FID rejected", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> out{};
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};

  Fid bad_fid(0);
  uint8_t bad_val = 250;
  std::memcpy(&bad_fid.storage.v1, &bad_val, 1);
  std::memcpy(&bad_fid.storage.v2, &bad_val, 1);
  std::memcpy(&bad_fid.storage.v3, &bad_val, 1);

  CHECK(frame_build(bad_fid, Toi(0), sb2, sb3, sb4, out, ldpc_ws, fbw) == FrameStatus::kInvalidFid);
}

TEST_CASE("frame_build: invalid TOI rejected", "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> out{};
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};

  Toi bad_toi(0);
  uint8_t bad_val = 250;
  std::memcpy(&bad_toi.storage.v1, &bad_val, 1);
  std::memcpy(&bad_toi.storage.v2, &bad_val, 1);
  std::memcpy(&bad_toi.storage.v3, &bad_val, 1);

  CHECK(frame_build(Fid::kNode1(), bad_toi, sb2, sb3, sb4, out, ldpc_ws, fbw) ==
        FrameStatus::kInvalidToi);
}

// ===========================================================================
// frame_decode: channel-LLR scale and partial-failure consistency
// ===========================================================================

namespace {
/// Fill three subframe buffers with deterministic non-trivial bits.
void fill_subframes(std::span<uint8_t> sb2,
                    std::span<uint8_t> sb3,
                    std::span<uint8_t> sb4,
                    std::mt19937& rng) {
  for (auto& b : sb2) {
    b = static_cast<uint8_t>(rng() & 1U);
  }
  for (auto& b : sb3) {
    b = static_cast<uint8_t>(rng() & 1U);
  }
  for (auto& b : sb4) {
    b = static_cast<uint8_t>(rng() & 1U);
  }
}
} // namespace

TEST_CASE(
    "[LSIS-AFS-720] frame_decode: llr_scale decodes a noisy unit-amplitude frame at an offset",
    "[frame]") {
  // The sync detector needs unit-amplitude symbols (its threshold is an
  // absolute correlation sum) and the LDPC decoder needs channel LLRs.  The
  // caller passes unit-amplitude symbols plus llr_scale; frame_decode applies
  // the scale to the payload only.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::mt19937 rng(7);
  fill_subframes(sb2_data, sb3_data, sb4_data, rng);

  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode3(), Toi(17), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  // A noise-only prefix, then the frame, all at noise standard deviation 0.6.
  constexpr size_t kOffset = 300;
  constexpr float kSigma = 0.6F;
  auto rx = std::make_unique<std::array<float, kFrameLength + kOffset>>();
  std::normal_distribution<float> noise(0.0F, kSigma);
  for (size_t i = 0; i < kOffset; ++i) {
    (*rx)[i] = noise(rng);
  }
  for (size_t i = 0; i < kFrameLength; ++i) {
    (*rx)[kOffset + i] = ((frame[i] == 0U) ? 1.0F : -1.0F) + noise(rng);
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const float llr_scale = 2.0F / (kSigma * kSigma);
  const auto status = frame_decode(
      *rx, result, *ws, hal, kFrameDecodeLdpcIterations, kDefaultSyncThreshold, llr_scale);

  REQUIRE(status == FrameDecodeStatus::kOk);
  CHECK(result.sync.frame_offset == kOffset);
  CHECK(result.bch.fid == Fid::kNode3());
  CHECK(result.bch.toi == Toi(17));
  CHECK(result.sb2.crc_ok);
  CHECK(result.sb3.crc_ok);
  CHECK(result.sb4.crc_ok);
  CHECK(std::equal(sb2_data.begin(), sb2_data.begin() + kSb2SpareOffset, ws->sb2_data.begin()));
  CHECK(std::equal(sb3_data.begin(), sb3_data.end(), ws->sb3_data.begin()));
  CHECK(std::equal(sb4_data.begin(), sb4_data.end(), ws->sb4_data.begin()));
}

TEST_CASE("[LSIS-AFS-720] frame_decode: rejects a non-finite or non-positive llr_scale",
          "[frame]") {
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(0), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);
  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  for (const float bad : {0.0F,
                          -1.0F,
                          std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
    FrameDecodeResult result{};
    CHECK(frame_decode(
              *rx, result, *ws, hal, kFrameDecodeLdpcIterations, kDefaultSyncThreshold, bad) ==
          FrameDecodeStatus::kInvalidInput);
    CHECK(result.status == FrameDecodeStatus::kInvalidInput);
  }
}

TEST_CASE("[LSIS-AFS-720] frame_decode: a failed frame leaves no crc_ok flag set", "[frame]") {
  // SB2 and SB3 are intact; SB4 is replaced with junk.  frame_decode scrubs
  // every data buffer on a non-kOk status, so it must also clear crc_ok for
  // the subframes that did pass: otherwise a caller reads zeros as verified
  // navigation data.
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::mt19937 rng(11);
  fill_subframes(sb2_data, sb3_data, sb4_data, rng);

  std::array<uint8_t, kFrameLength> frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid::kNode1(), Toi(5), sb2_data, sb3_data, sb4_data, frame, enc_ws, fbw) ==
          FrameStatus::kOk);
  auto rx = std::make_unique<std::array<float, kFrameLength>>();
  binary_to_bipolar(frame, *rx);

  // Find where the SB4 codeword lands after interleaving, and overwrite it.
  auto mask = std::make_unique<std::array<uint8_t, kPayloadLength>>();
  auto mask_tx = std::make_unique<std::array<uint8_t, kPayloadLength>>();
  std::fill(mask->begin() + kSb2Length + kSb3Length, mask->end(), uint8_t{1});
  REQUIRE(block_interleave(*mask, *mask_tx) == InterleaverStatus::kOk);
  const size_t payload_start = kSyncLength + kSb1Length;
  for (size_t i = 0; i < kPayloadLength; ++i) {
    if ((*mask_tx)[i] == 1U) {
      (*rx)[payload_start + i] = ((rng() & 1U) == 0U) ? 1.0F : -1.0F;
    }
  }

  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();
  NullTelemetryHal hal;
  const auto status = frame_decode(*rx, result, *ws, hal);

  REQUIRE(status != FrameDecodeStatus::kOk);
  // The intact subframes converged; that diagnostic survives.
  CHECK(result.sb2.ldpc_converged);
  CHECK(result.sb3.ldpc_converged);
  // No subframe claims a verified CRC, and no data is left behind.
  CHECK_FALSE(result.sb2.crc_ok);
  CHECK_FALSE(result.sb3.crc_ok);
  CHECK_FALSE(result.sb4.crc_ok);
  CHECK(std::all_of(ws->sb2_data.begin(), ws->sb2_data.end(), [](uint8_t b) { return b == 0U; }));
  CHECK(std::all_of(ws->sb3_data.begin(), ws->sb3_data.end(), [](uint8_t b) { return b == 0U; }));
}
