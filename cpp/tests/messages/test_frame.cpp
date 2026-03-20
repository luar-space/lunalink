#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

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

  // Verify data recovery: first kSb2DataBits must match original
  for (size_t i = 0; i < kSb2DataBits; ++i) {
    REQUIRE(decoded[i] == data[i]);
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

    for (size_t i = 0; i < kSb2DataBits; ++i) {
      REQUIRE(decoded[i] == sb2_data[i]);
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
// LSIS-300: Spare bits — alternating 0/1 starting with 0 on MSB
// ===========================================================================

TEST_CASE("[LSIS-AFS-300] Spare bits pattern: alternating 0/1 starting with 0", "[frame]") {
  // LSIS-300: "spare bits shall be filled with a sequence of alternating
  // zeros and ones starting with zero on the MSB."
  // Verify that CRC correctly covers spare bits with this pattern.

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
