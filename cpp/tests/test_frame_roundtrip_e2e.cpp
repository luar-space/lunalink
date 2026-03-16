#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "lunalink/signal/frame.hpp"
#include "lunalink/signal/frame_decode.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>

using namespace lunalink::signal;

namespace {

/// Convert binary frame (0/1 uint8_t) to bipolar soft symbols (+1.0 / -1.0).
/// Bit 0 → +1.0, Bit 1 → -1.0 (BPSK mapping per LSIS Table 8).
void binary_to_bipolar(std::span<const uint8_t> bits,
                       std::span<float> out) {
    for (size_t i = 0; i < bits.size(); ++i) {
        out[i] = (bits[i] == 0U) ? 1.0F : -1.0F;
    }
}

/// Add AWGN noise to bipolar symbols at a given Eb/N0 (dB).
/// For BPSK: Es/N0 = Eb/N0 (since 1 bit per symbol).
/// sigma = sqrt(1 / (2 * 10^(EbN0_dB/10)))
void add_awgn(std::span<float> symbols, float eb_n0_db,
              std::mt19937& rng) {
    const auto eb_n0_lin = std::pow(10.0F, eb_n0_db / 10.0F);
    const auto sigma = std::sqrt(1.0F / (2.0F * eb_n0_lin));
    auto dist = std::normal_distribution<float>(0.0F, sigma);
    for (auto& s : symbols) {
        s += dist(rng);
    }
}

/// Fill a span with random bits (0 or 1).
void random_bits(std::span<uint8_t> out, std::mt19937& rng) {
    auto dist = std::uniform_int_distribution<uint32_t>(0, 1);
    for (auto& b : out) {
        b = static_cast<uint8_t>(dist(rng));
    }
}

} // namespace

TEST_CASE("E2E round-trip: TX → AWGN → RX → bit-exact recovery", "[e2e][gd5.3]") {
    std::mt19937 rng(42);

    // Generate random navigation data for all subframes
    std::array<uint8_t, kSb2DataBits> sb2_data{};
    std::array<uint8_t, kSb3DataBits> sb3_data{};
    std::array<uint8_t, kSb4DataBits> sb4_data{};
    random_bits(sb2_data, rng);
    random_bits(sb3_data, rng);
    random_bits(sb4_data, rng);

    const Fid fid{0};
    const Toi toi{42};

    // ── TX: Build frame ──────────────────────────────────────────────────
    std::array<uint8_t, kFrameLength> tx_frame{};
    LdpcEncodeWorkspace enc_ws{};
    const auto build_status = frame_build(
        fid, toi, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws);
    REQUIRE(build_status == FrameStatus::kOk);

    // ── Channel: binary → bipolar → AWGN at Eb/N0 = 3 dB ────────────────
    std::array<float, kFrameLength> rx_symbols{};
    binary_to_bipolar(tx_frame, rx_symbols);
    add_awgn(rx_symbols, 3.0F, rng);

    // ── RX: Decode ───────────────────────────────────────────────────────
    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto workspace = std::make_unique<FrameDecodeWorkspace>();

    const auto status = frame_decode(rx_symbols, result, *workspace, hal);

    // ── Verify ───────────────────────────────────────────────────────────
    REQUIRE(status == FrameDecodeStatus::kOk);
    CHECK(result.sync.detected);
    CHECK(result.sync.frame_offset == 0);
    CHECK(result.bch.status == BchStatus::kOk);
    CHECK(result.bch.fid == fid);
    CHECK(result.bch.toi == toi);

    CHECK(result.sb2.ldpc_converged);
    CHECK(result.sb2.crc_ok);
    CHECK(result.sb3.ldpc_converged);
    CHECK(result.sb3.crc_ok);
    CHECK(result.sb4.ldpc_converged);
    CHECK(result.sb4.crc_ok);

    // Bit-exact data recovery
    CHECK(std::equal(sb2_data.begin(), sb2_data.end(),
                     workspace->sb2_data.begin()));
    CHECK(std::equal(sb3_data.begin(), sb3_data.end(),
                     workspace->sb3_data.begin()));
    CHECK(std::equal(sb4_data.begin(), sb4_data.end(),
                     workspace->sb4_data.begin()));
}

// ---------------------------------------------------------------------------
// Gateway 5: Frame-Level FER Sweep
//
// Validates that the full frame_decode() pipeline achieves the deliverable
// requirement "BER < 10^-4 at SNR > 0 dB" at the frame level.
//
// Runs Monte Carlo at multiple Eb/N0 points, recording:
//   - Frame success rate (all 3 subframes: LDPC converged + CRC pass)
//   - Per-subframe bit error rate
//
// At Eb/N0 = 3.0 dB (Es/N0 = 0 dB for rate 1/2), 100 frames must show
// zero frame errors to confirm FER < 10^-2 with statistical confidence.
// At Eb/N0 = 6.0 dB, 200 frames confirms zero-error operation.
// ---------------------------------------------------------------------------
TEST_CASE("Gateway 5: frame-level FER sweep", "[e2e][gd5.3][performance]") {
    struct SnrPoint {
        float  eb_n0_db;
        int    frames;
        double max_fer;     // Maximum allowable frame error rate
        const char* label;
    };

    // NOLINTBEGIN(cert-msc51-cpp)
    constexpr SnrPoint kPoints[] = {
        {1.5F,   50,  1.0,   "1.5 dB (waterfall)"},    // expect some errors
        {3.0F,  100,  0.0,   "3.0 dB (Es/N0=0 dB)"},  // spec operating point
        {6.0F,  200,  0.0,   "6.0 dB (margin)"},       // well above threshold
    };

    NullTelemetryHal hal;
    auto workspace = std::make_unique<FrameDecodeWorkspace>();
    LdpcEncodeWorkspace enc_ws{};

    for (const auto& pt : kPoints) {
        size_t frame_errors = 0;
        size_t total_bit_errors = 0;
        const size_t bits_per_frame = kSb2DataBits + kSb3DataBits + kSb4DataBits;

        for (int trial = 0; trial < pt.frames; ++trial) {
            std::mt19937 rng(static_cast<uint32_t>(1000 * pt.eb_n0_db) +
                             static_cast<uint32_t>(trial));

            std::array<uint8_t, kSb2DataBits> sb2{};
            std::array<uint8_t, kSb3DataBits> sb3{};
            std::array<uint8_t, kSb4DataBits> sb4{};
            random_bits(sb2, rng);
            random_bits(sb3, rng);
            random_bits(sb4, rng);

            std::array<uint8_t, kFrameLength> tx{};
            REQUIRE(frame_build(Fid{0}, Toi{static_cast<uint8_t>(trial % 100)},
                                sb2, sb3, sb4, tx, enc_ws) == FrameStatus::kOk);

            std::array<float, kFrameLength> rx{};
            binary_to_bipolar(tx, rx);
            add_awgn(rx, pt.eb_n0_db, rng);

            FrameDecodeResult result{};
            const auto status = frame_decode(rx, result, *workspace, hal);

            bool frame_ok = (status == FrameDecodeStatus::kOk) &&
                            result.sb2.ldpc_converged && result.sb2.crc_ok &&
                            result.sb3.ldpc_converged && result.sb3.crc_ok &&
                            result.sb4.ldpc_converged && result.sb4.crc_ok;

            if (!frame_ok) {
                frame_errors++;
            } else {
                // Count bit errors on successfully decoded frames
                for (size_t i = 0; i < kSb2DataBits; ++i) {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    if (sb2[i] != workspace->sb2_data[i]) { total_bit_errors++; }
                }
                for (size_t i = 0; i < kSb3DataBits; ++i) {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    if (sb3[i] != workspace->sb3_data[i]) { total_bit_errors++; }
                }
                for (size_t i = 0; i < kSb4DataBits; ++i) {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    if (sb4[i] != workspace->sb4_data[i]) { total_bit_errors++; }
                }
            }
        }

        const double fer = static_cast<double>(frame_errors) /
                           static_cast<double>(pt.frames);
        const size_t total_bits = static_cast<size_t>(pt.frames) * bits_per_frame;
        const double ber = static_cast<double>(total_bit_errors) /
                           static_cast<double>(total_bits);

        std::cout << "[FER sweep] Eb/N0=" << pt.eb_n0_db << " dB"
                  << "  FER=" << fer
                  << " (" << frame_errors << "/" << pt.frames << ")"
                  << "  BER=" << ber
                  << " (" << total_bit_errors << "/" << total_bits << ")"
                  << std::endl;

        CHECK(fer <= pt.max_fer);
    }
    // NOLINTEND(cert-msc51-cpp)
}

TEST_CASE("E2E round-trip: FID=0 TOI=99 with random offset", "[e2e][gd5.3]") {
    std::mt19937 rng(99);

    std::array<uint8_t, kSb2DataBits> sb2_data{};
    std::array<uint8_t, kSb3DataBits> sb3_data{};
    std::array<uint8_t, kSb4DataBits> sb4_data{};
    random_bits(sb2_data, rng);
    random_bits(sb3_data, rng);
    random_bits(sb4_data, rng);

    const Fid fid{0};
    const Toi toi{99};

    std::array<uint8_t, kFrameLength> tx_frame{};
    LdpcEncodeWorkspace enc_ws{};
    REQUIRE(frame_build(fid, toi, sb2_data, sb3_data, sb4_data,
                        tx_frame, enc_ws) == FrameStatus::kOk);

    // Place frame at a random offset within a larger buffer
    constexpr size_t kPadding = 200;
    std::array<float, kFrameLength + kPadding> rx_buf{};
    // Fill padding with noise
    {
        auto noise_dist = std::normal_distribution<float>(0.0F, 0.5F);
        for (size_t i = 0; i < kPadding; ++i) {
            rx_buf[i] = noise_dist(rng);
        }
    }
    // Place bipolar frame after padding
    for (size_t i = 0; i < kFrameLength; ++i) {
        rx_buf[kPadding + i] = (tx_frame[i] == 0U) ? 1.0F : -1.0F;
    }
    // Add light noise (Eb/N0 = 6 dB)
    add_awgn(std::span<float>(rx_buf).subspan(kPadding, kFrameLength), 6.0F, rng);

    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto workspace = std::make_unique<FrameDecodeWorkspace>();

    const auto status = frame_decode(rx_buf, result, *workspace, hal);

    REQUIRE(status == FrameDecodeStatus::kOk);
    CHECK(result.sync.frame_offset == kPadding);
    CHECK(result.bch.fid == fid);
    CHECK(result.bch.toi == toi);

    CHECK(std::equal(sb2_data.begin(), sb2_data.end(),
                     workspace->sb2_data.begin()));
    CHECK(std::equal(sb3_data.begin(), sb3_data.end(),
                     workspace->sb3_data.begin()));
    CHECK(std::equal(sb4_data.begin(), sb4_data.end(),
                     workspace->sb4_data.begin()));
}

TEST_CASE("E2E: noiseless round-trip (Eb/N0 = infinity)", "[e2e][gd5.3]") {
    // Perfect channel: verify the pipeline works without any noise
    std::mt19937 rng(7);

    std::array<uint8_t, kSb2DataBits> sb2_data{};
    std::array<uint8_t, kSb3DataBits> sb3_data{};
    std::array<uint8_t, kSb4DataBits> sb4_data{};
    random_bits(sb2_data, rng);
    random_bits(sb3_data, rng);
    random_bits(sb4_data, rng);

    std::array<uint8_t, kFrameLength> tx_frame{};
    LdpcEncodeWorkspace enc_ws{};
    REQUIRE(frame_build(Fid{2}, Toi{50}, sb2_data, sb3_data, sb4_data,
                        tx_frame, enc_ws) == FrameStatus::kOk);

    std::array<float, kFrameLength> rx_symbols{};
    binary_to_bipolar(tx_frame, rx_symbols);
    // No noise added

    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto workspace = std::make_unique<FrameDecodeWorkspace>();

    REQUIRE(frame_decode(rx_symbols, result, *workspace, hal) == FrameDecodeStatus::kOk);
    CHECK(result.bch.fid == Fid{2});
    CHECK(result.bch.toi == Toi{50});
    CHECK(result.sb2.ldpc_converged);
    CHECK(result.sb2.crc_ok);
    CHECK(result.sb3.ldpc_converged);
    CHECK(result.sb3.crc_ok);
    CHECK(result.sb4.ldpc_converged);
    CHECK(result.sb4.crc_ok);

    CHECK(std::equal(sb2_data.begin(), sb2_data.end(),
                     workspace->sb2_data.begin()));
    CHECK(std::equal(sb3_data.begin(), sb3_data.end(),
                     workspace->sb3_data.begin()));
    CHECK(std::equal(sb4_data.begin(), sb4_data.end(),
                     workspace->sb4_data.begin()));
}

// ---------------------------------------------------------------------------
// CRC validation catches corrupted frames
//
// Deliverables.pdf Gateway 5 Validation Requirement:
//   "CRC validation catches corrupted frames"
//
// Build a valid frame, decode it successfully, then corrupt individual
// subframe payload symbols and verify that CRC rejects the corrupted data.
// ---------------------------------------------------------------------------
TEST_CASE("E2E: CRC rejects corrupted subframe payload", "[e2e][gd5.3]") {
    std::mt19937 rng(123);  // NOLINT(cert-msc51-cpp)

    std::array<uint8_t, kSb2DataBits> sb2_data{};
    std::array<uint8_t, kSb3DataBits> sb3_data{};
    std::array<uint8_t, kSb4DataBits> sb4_data{};
    random_bits(sb2_data, rng);
    random_bits(sb3_data, rng);
    random_bits(sb4_data, rng);

    std::array<uint8_t, kFrameLength> tx_frame{};
    LdpcEncodeWorkspace enc_ws{};
    REQUIRE(frame_build(Fid{1}, Toi{77}, sb2_data, sb3_data, sb4_data,
                        tx_frame, enc_ws) == FrameStatus::kOk);

    // Convert to bipolar (noiseless)
    std::array<float, kFrameLength> rx_clean{};
    binary_to_bipolar(tx_frame, rx_clean);

    // Sanity: clean frame decodes successfully
    {
        NullTelemetryHal hal;
        FrameDecodeResult result{};
        auto ws = std::make_unique<FrameDecodeWorkspace>();
        REQUIRE(frame_decode(rx_clean, result, *ws, hal) == FrameDecodeStatus::kOk);
        CHECK(result.sb2.crc_ok);
        CHECK(result.sb3.crc_ok);
        CHECK(result.sb4.crc_ok);
    }

    // Corrupt the interleaved payload region: flip a block of symbols
    // Payload starts at offset kSyncLength + kSb1Length = 68 + 52 = 120
    // Flipping enough symbols in the payload will cause LDPC to either
    // not converge or produce residual errors that CRC catches.
    constexpr size_t kPayloadStart = kSyncLength + kSb1Length;
    auto rx_corrupt = rx_clean;

    // Flip 3000 of 5880 payload symbols (>50% — overwhelms rate-1/2 LDPC)
    for (size_t i = 0; i < 3000; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        rx_corrupt[kPayloadStart + i] = -rx_corrupt[kPayloadStart + i];
    }

    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto ws = std::make_unique<FrameDecodeWorkspace>();
    const auto status = frame_decode(rx_corrupt, result, *ws, hal);

    // The pipeline must NOT report full success: either LDPC fails to
    // converge or CRC catches the residual errors.
    bool any_subframe_rejected =
        !result.sb2.crc_ok || !result.sb3.crc_ok || !result.sb4.crc_ok;
    bool pipeline_detected_corruption =
        (status != FrameDecodeStatus::kOk) || any_subframe_rejected;

    CHECK(pipeline_detected_corruption);

    // At least one subframe's CRC or LDPC convergence must have failed
    std::cout << "[CRC rejection] status="
              << static_cast<int>(status)
              << "  sb2: ldpc=" << result.sb2.ldpc_converged
              << " crc=" << result.sb2.crc_ok
              << "  sb3: ldpc=" << result.sb3.ldpc_converged
              << " crc=" << result.sb3.crc_ok
              << "  sb4: ldpc=" << result.sb4.ldpc_converged
              << " crc=" << result.sb4.crc_ok
              << std::endl;
}

// ---------------------------------------------------------------------------
// Decode latency < 1 second
//
// Deliverables.pdf Gateway 5 Success Criterion:
//   "Decode frames in < 1 second"
//
// Time a single frame_decode() call and assert it completes within the
// 1-second budget.  Uses wall-clock timing (not a Catch2 BENCHMARK) so
// the assertion is enforced in the regular test suite, not just in
// performance mode.
// ---------------------------------------------------------------------------
TEST_CASE("E2E: frame_decode completes in < 1 second", "[e2e][gd5.3]") {
    std::mt19937 rng(55);  // NOLINT(cert-msc51-cpp)

    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    random_bits(sb2, rng);
    random_bits(sb3, rng);
    random_bits(sb4, rng);

    std::array<uint8_t, kFrameLength> tx{};
    LdpcEncodeWorkspace enc_ws{};
    REQUIRE(frame_build(Fid{0}, Toi{1}, sb2, sb3, sb4, tx, enc_ws)
            == FrameStatus::kOk);

    // Noiseless bipolar symbols (worst-case for LDPC: instant convergence,
    // but we want to measure the full pipeline including sync search)
    std::array<float, kFrameLength> rx{};
    binary_to_bipolar(tx, rx);

    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto workspace = std::make_unique<FrameDecodeWorkspace>();

    const auto t0 = std::chrono::steady_clock::now();
    const auto status = frame_decode(rx, result, *workspace, hal);
    const auto t1 = std::chrono::steady_clock::now();

    REQUIRE(status == FrameDecodeStatus::kOk);

    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "[latency] frame_decode: " << elapsed_ms << " ms" << std::endl;

    CHECK(elapsed_ms < 1000);
}
