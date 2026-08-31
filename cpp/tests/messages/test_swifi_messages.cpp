/**
 * @file test_swifi_messages.cpp
 * @brief SWIFI tests for messages subsystem: frame_build aliasing,
 *        subframe builder CFI, frame assembler CFI, frame_decode CFI,
 *        return-value injection.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include "lunalink/core/telemetry.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-24: Aliasing — overlapping workspace staging and output in frame_build_partial
// ---------------------------------------------------------------------------
TEST_CASE(
    "[LSIS-AFS-601] SWIFI: aliasing_overlap_frame_build_partial — kFaultDetected before write",
    "[swifi][messages]") {
  // Arena large enough for FrameBuildWorkspace + kFrameLength with overlap.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
  alignas(16) std::array<uint8_t, sizeof(FrameBuildWorkspace) + kFrameLength> arena{};
  arena.fill(0xFFU);
  auto* ws = new (arena.data()) FrameBuildWorkspace{};
  // staging lives at arena[0..5880); out at arena[2000..8000) — overlap at [2000..5880).
  std::span<uint8_t, kFrameLength> out(arena.data() + 2000U, kFrameLength);

  const auto status = frame_build_partial(Fid{0U}, Toi{0U}, out, *ws);
  REQUIRE(status == FrameStatus::kFaultDetected);

  // Staging must be scrubbed after aliasing detection.
  for (const auto byte : ws->staging) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-25: Aliasing — overlapping workspace staging and output in frame_build
// ---------------------------------------------------------------------------
TEST_CASE("[LSIS-AFS-605] SWIFI: aliasing_overlap_frame_build — kFaultDetected before write",
          "[swifi][messages]") {
  alignas(16) std::array<uint8_t, sizeof(FrameBuildWorkspace) + kFrameLength> arena{};
  arena.fill(0xFFU);
  auto* ws = new (arena.data()) FrameBuildWorkspace{};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) — intentional: constructing an
  // overlapping span to test aliasing detection in frame_build
  auto* out_ptr = arena.data() + 2000U;
  std::span<uint8_t, kFrameLength> out(out_ptr, kFrameLength);

  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  LdpcEncodeWorkspace ldpc_ws{};

  const auto status = frame_build(Fid{0U}, Toi{0U}, sb2, sb3, sb4, out, ldpc_ws, *ws);
  REQUIRE(status == FrameStatus::kFaultDetected);

  // Staging must be scrubbed.
  for (const auto byte : ws->staging) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-13 — Frame-assembly CFI: subframe builder milestone skip (FM-025)
// ---------------------------------------------------------------------------
TEST_CASE("[LSIS-AFS-610] SWIFI: subframe_builder_cfi_milestone_skip — LDPC LUT fault skips "
          "milestone 3",
          "[swifi][messages]") {
  // Inject a single bit flip into the LDPC SF2 matrix (same as FI-05).
  kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

  // Prepare valid SB2 input data (1176 bits) and output buffer (2400 symbols).
  std::array<uint8_t, kSb2DataBits> data{};
  data.fill(0U);
  std::array<uint8_t, kSb2Length> out{};
  out.fill(0xFFU); // poison to verify scrub
  LdpcEncodeWorkspace workspace{};

  const auto status = subframe_build_sb2(data, out, workspace);

  // Always restore matrix before any CHECK/REQUIRE.
  kLdpc_sf2_a.clear_fault();
  REQUIRE(kLdpc_sf2_a.verify_integrity() == true);

  // LDPC failure must be detected; output must be scrubbed.
  REQUIRE(status != FrameStatus::kOk);
  for (const auto byte : out) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-14 — Frame-assembly CFI: frame assembler milestone skip (FM-026)
// ---------------------------------------------------------------------------
TEST_CASE("[SW-SAF-01] [LSIS-AFS-606] SWIFI: frame_assembler_cfi_milestone_skip — LDPC LUT fault "
          "skips SB2 milestone",
          "[swifi][messages]") {
  // Inject a single bit flip into the LDPC SF2 matrix.
  kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

  // Prepare valid inputs.
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> out{};
  out.fill(0xFFU); // poison to verify scrub
  LdpcEncodeWorkspace workspace{};
  FrameBuildWorkspace fbw{};

  const auto status = frame_build(Fid{0U}, Toi{0U}, sb2, sb3, sb4, out, workspace, fbw);

  // Always restore matrix before any CHECK/REQUIRE.
  kLdpc_sf2_a.clear_fault();
  REQUIRE(kLdpc_sf2_a.verify_integrity() == true);

  // Frame build must fail; output must be scrubbed.
  REQUIRE(status != FrameStatus::kOk);
  for (const auto byte : out) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-23: frame_decode CFI skip — injected milestone skip triggers kFaultDetected
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: frame_decode_cfi_skip — skipped milestone fires kFaultDetected",
          "[swifi][messages]") {
  // Build a valid noiseless frame for decoding
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> tx_frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw2{};
  REQUIRE(frame_build(Fid{0}, Toi{42}, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws, fbw2) ==
          FrameStatus::kOk);

  std::array<float, kFrameLength> rx_symbols{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    rx_symbols[i] = (tx_frame[i] == 0U) ? 1.0F : -1.0F;
  }

  // Nominal decode must succeed
  {
    CaptureTelemetryHal<4> hal;
    FrameDecodeResult result{};
    auto ws = std::make_unique<FrameDecodeWorkspace>();
    REQUIRE(frame_decode(rx_symbols, result, *ws, hal) == FrameDecodeStatus::kOk);
  }

  // Inject CFI skip: subtract 1 from counter → terminal check fails
  {
    CaptureTelemetryHal<4> hal;
    FrameDecodeResult result{};
    auto ws = std::make_unique<FrameDecodeWorkspace>();
    ws->cfi_skip_inject = 1; // Skip 1 milestone
    const auto status = frame_decode(rx_symbols, result, *ws, hal);
    REQUIRE(status == FrameDecodeStatus::kFaultDetected);
    // Verify a fault packet was emitted
    REQUIRE(hal.count() >= 1);
  }
}

// ---------------------------------------------------------------------------
// FI-28 — Return-value injection: subframe_build_sb2 forced LDPC failure
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: subframe_build_sb2_forced_ldpc_fail — output scrubbed on injected failure",
          "[swifi][messages]") {
  std::array<uint8_t, kSb2DataBits> data{};
  std::array<uint8_t, kSb2Length> out{};
  out.fill(0xFFU); // poison to verify scrub
  LdpcEncodeWorkspace ws{};

  // Inject: force ldpc_encode to return kFaultDetected
  ws.fi_force_status = static_cast<uint8_t>(LdpcStatus::kFaultDetected);

  const auto status = subframe_build_sb2(data, out, ws);

  // Subframe builder must propagate the failure
  REQUIRE(status != FrameStatus::kOk);

  // Output must be scrubbed (all zero)
  for (const auto byte : out) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-29 — Return-value injection: frame_build forced LDPC failure
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: frame_build_forced_ldpc_fail — full frame scrubbed on injected failure",
          "[swifi][messages]") {
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  std::array<uint8_t, kFrameLength> out{};
  out.fill(0xFFU); // poison to verify scrub
  LdpcEncodeWorkspace ldpc_ws{};
  FrameBuildWorkspace fbw{};

  // Inject: force ldpc_encode to return kFaultDetected on the first LDPC call (SB2)
  ldpc_ws.fi_force_status = static_cast<uint8_t>(LdpcStatus::kFaultDetected);

  const auto status = frame_build(Fid{0U}, Toi{0U}, sb2, sb3, sb4, out, ldpc_ws, fbw);

  // Frame build must fail
  REQUIRE(status != FrameStatus::kOk);

  // Full frame output must be scrubbed
  for (const auto byte : out) {
    CHECK(byte == 0U);
  }

  // Staging must be scrubbed
  for (const auto byte : fbw.staging) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-30 — Return-value injection: frame_decode forced sync failure
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: frame_decode_forced_sync_fail — workspace scrubbed on injected sync failure",
          "[swifi][messages]") {
  // Build a valid noiseless frame
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> tx_frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid{0}, Toi{42}, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  std::array<float, kFrameLength> rx_symbols{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    rx_symbols[i] = (tx_frame[i] == 0U) ? 1.0F : -1.0F;
  }

  CaptureTelemetryHal<4> hal;
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();

  // Inject: force sync stage to return kSyncNotFound
  ws->fi_force_sync_fail = 1;

  const auto status = frame_decode(rx_symbols, result, *ws, hal);
  REQUIRE(status == FrameDecodeStatus::kSyncNotFound);

  // Data buffers must be scrubbed on non-kOk status
  for (const auto byte : ws->sb2_data) {
    CHECK(byte == 0U);
  }
  for (const auto byte : ws->sb3_data) {
    CHECK(byte == 0U);
  }
  for (const auto byte : ws->sb4_data) {
    CHECK(byte == 0U);
  }
}

// ---------------------------------------------------------------------------
// FI-31 — Return-value injection: frame_decode forced LDPC failure
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: frame_decode_forced_ldpc_fail — workspace scrubbed on injected LDPC failure",
          "[swifi][messages]") {
  // Build a valid noiseless frame
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  std::array<uint8_t, kFrameLength> tx_frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid{0}, Toi{42}, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  std::array<float, kFrameLength> rx_symbols{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    rx_symbols[i] = (tx_frame[i] == 0U) ? 1.0F : -1.0F;
  }

  CaptureTelemetryHal<4> hal;
  FrameDecodeResult result{};
  auto ws = std::make_unique<FrameDecodeWorkspace>();

  // Inject: force LDPC decode stage to return kFaultDetected
  ws->fi_force_ldpc_fail = 1;

  const auto status = frame_decode(rx_symbols, result, *ws, hal);
  REQUIRE(status == FrameDecodeStatus::kFaultDetected);

  // Data buffers must be scrubbed on kFaultDetected status
  for (const auto byte : ws->sb2_data) {
    CHECK(byte == 0U);
  }
  for (const auto byte : ws->sb3_data) {
    CHECK(byte == 0U);
  }
  for (const auto byte : ws->sb4_data) {
    CHECK(byte == 0U);
  }
}
