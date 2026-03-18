/**
 * @file test_swifi_messages.cpp
 * @brief SWIFI tests for messages subsystem: frame_build aliasing,
 *        subframe builder CFI, frame assembler CFI, frame_decode CFI.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <memory>

#include "lunalink/core/telemetry.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-24: Aliasing — overlapping workspace staging and output in frame_build_partial
// ---------------------------------------------------------------------------
TEST_CASE("[LSIS-AFS-601] SWIFI: aliasing_overlap_frame_build_partial — kFaultDetected before write") {
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
TEST_CASE("[LSIS-AFS-605] SWIFI: aliasing_overlap_frame_build — kFaultDetected before write") {
    alignas(16) std::array<uint8_t, sizeof(FrameBuildWorkspace) + kFrameLength> arena{};
    arena.fill(0xFFU);
    auto* ws = new (arena.data()) FrameBuildWorkspace{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) — intentional: constructing an overlapping span to test aliasing detection in frame_build
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
TEST_CASE("[LSIS-AFS-610] SWIFI: subframe_builder_cfi_milestone_skip — LDPC LUT fault skips milestone 3") {
    // Inject a single bit flip into the LDPC SF2 matrix (same as FI-05).
    kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

    // Prepare valid SB2 input data (1176 bits) and output buffer (2400 symbols).
    std::array<uint8_t, kSb2DataBits> data{};
    data.fill(0U);
    std::array<uint8_t, kSb2Length> out{};
    out.fill(0xFFU);  // poison to verify scrub
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
TEST_CASE("[SW-SAF-01] [LSIS-AFS-606] SWIFI: frame_assembler_cfi_milestone_skip — LDPC LUT fault skips SB2 milestone") {
    // Inject a single bit flip into the LDPC SF2 matrix.
    kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

    // Prepare valid inputs.
    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    std::array<uint8_t, kFrameLength> out{};
    out.fill(0xFFU);  // poison to verify scrub
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
TEST_CASE("SWIFI: frame_decode_cfi_skip — skipped milestone fires kFaultDetected") {
    // Build a valid noiseless frame for decoding
    std::array<uint8_t, kSb2DataBits> sb2_data{};
    std::array<uint8_t, kSb3DataBits> sb3_data{};
    std::array<uint8_t, kSb4DataBits> sb4_data{};
    std::array<uint8_t, kFrameLength> tx_frame{};
    LdpcEncodeWorkspace enc_ws{};
    FrameBuildWorkspace fbw2{};
    REQUIRE(frame_build(Fid{0}, Toi{42}, sb2_data, sb3_data, sb4_data,
                        tx_frame, enc_ws, fbw2) == FrameStatus::kOk);

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
        ws->cfi_skip_inject = 1;  // Skip 1 milestone
        const auto status = frame_decode(rx_symbols, result, *ws, hal);
        REQUIRE(status == FrameDecodeStatus::kFaultDetected);
        // Verify a fault packet was emitted
        REQUIRE(hal.count() >= 1);
    }
}
