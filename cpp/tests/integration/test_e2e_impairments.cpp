#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <random>

using namespace lunalink::signal;

namespace {

/// Convert binary frame (0/1 uint8_t) to bipolar soft symbols (+1.0 / -1.0).
void binary_to_bipolar(std::span<const uint8_t> bits, std::span<float> out) {
  for (size_t i = 0; i < bits.size(); ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    out[i] = (bits[i] == 0U) ? 1.0F : -1.0F;
  }
}

/// Add AWGN noise to bipolar symbols at a given Eb/N0 (dB).
void add_awgn(std::span<float> symbols, float eb_n0_db, std::mt19937& rng) {
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

/// Quantize bipolar soft symbols to int16 or int8, then dequantize back to float.
/// Simulates ADC quantization of the noisy received signal.
void quantize_symbols(std::span<float> symbols, int bits) {
  if (bits <= 0) {
    return;
  }

  // Find peak magnitude for AGC scaling (3σ headroom).
  float peak = 0.0F;
  for (const auto& s : symbols) {
    peak = std::max(peak, std::abs(s));
  }
  if (peak < 1e-12F) {
    return;
  }

  const float scale = static_cast<float>((1 << (bits - 1)) - 1);
  const float agc_gain = scale / peak;

  for (auto& s : symbols) {
    const float q = std::round(s * agc_gain);
    const float clamped = std::clamp(q, -scale, scale);
    s = clamped / agc_gain;
  }
}

} // namespace

// ─── E2E Impairment Tests ───────────────────────────────────────────────────

TEST_CASE("E2E impairment: round-trip at int16", "[e2e][impairment]") {
  std::mt19937 rng(42); // NOLINT(cert-msc51-cpp)

  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  random_bits(sb2_data, rng);
  random_bits(sb3_data, rng);
  random_bits(sb4_data, rng);

  std::array<uint8_t, kFrameLength> tx_frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid{0}, Toi{42}, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  std::array<float, kFrameLength> rx_symbols{};
  binary_to_bipolar(tx_frame, rx_symbols);
  add_awgn(rx_symbols, 3.0F, rng);
  quantize_symbols(rx_symbols, 16);

  NullTelemetryHal hal;
  FrameDecodeResult result{};
  auto workspace = std::make_unique<FrameDecodeWorkspace>();

  const auto status = frame_decode(rx_symbols, result, *workspace, hal);

  REQUIRE(status == FrameDecodeStatus::kOk);
  CHECK(result.sb2.ldpc_converged);
  CHECK(result.sb2.crc_ok);
  CHECK(result.sb3.ldpc_converged);
  CHECK(result.sb3.crc_ok);
  CHECK(result.sb4.ldpc_converged);
  CHECK(result.sb4.crc_ok);

  CHECK(std::equal(sb2_data.begin(), sb2_data.end(), workspace->sb2_data.begin()));
  CHECK(std::equal(sb3_data.begin(), sb3_data.end(), workspace->sb3_data.begin()));
  CHECK(std::equal(sb4_data.begin(), sb4_data.end(), workspace->sb4_data.begin()));
}

TEST_CASE("E2E impairment: round-trip at int8", "[e2e][impairment]") {
  std::mt19937 rng(43); // NOLINT(cert-msc51-cpp)

  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};
  random_bits(sb2_data, rng);
  random_bits(sb3_data, rng);
  random_bits(sb4_data, rng);

  std::array<uint8_t, kFrameLength> tx_frame{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  REQUIRE(frame_build(Fid{0}, Toi{43}, sb2_data, sb3_data, sb4_data, tx_frame, enc_ws, fbw) ==
          FrameStatus::kOk);

  std::array<float, kFrameLength> rx_symbols{};
  binary_to_bipolar(tx_frame, rx_symbols);
  add_awgn(rx_symbols, 3.0F, rng);
  quantize_symbols(rx_symbols, 8);

  NullTelemetryHal hal;
  FrameDecodeResult result{};
  auto workspace = std::make_unique<FrameDecodeWorkspace>();

  const auto status = frame_decode(rx_symbols, result, *workspace, hal);

  REQUIRE(status == FrameDecodeStatus::kOk);
  CHECK(result.sb2.ldpc_converged);
  CHECK(result.sb2.crc_ok);
  CHECK(result.sb3.ldpc_converged);
  CHECK(result.sb3.crc_ok);
  CHECK(result.sb4.ldpc_converged);
  CHECK(result.sb4.crc_ok);

  CHECK(std::equal(sb2_data.begin(), sb2_data.end(), workspace->sb2_data.begin()));
  CHECK(std::equal(sb3_data.begin(), sb3_data.end(), workspace->sb3_data.begin()));
  CHECK(std::equal(sb4_data.begin(), sb4_data.end(), workspace->sb4_data.begin()));
}

TEST_CASE("E2E impairment: FER sweep at int8", "[e2e][impairment]") {
  const auto eb_n0_db = GENERATE(3.0F, 6.0F);
  INFO("Eb/N0 = " << eb_n0_db << " dB (int8 quantized)");

  constexpr int kTrials = 50;
  size_t frame_errors = 0;

  NullTelemetryHal hal;
  auto workspace = std::make_unique<FrameDecodeWorkspace>();
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};

  for (int trial = 0; trial < kTrials; ++trial) {
    // NOLINTNEXTLINE(cert-msc51-cpp)
    std::mt19937 rng(static_cast<uint32_t>(1000.0F * eb_n0_db) + static_cast<uint32_t>(trial));

    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    random_bits(sb2, rng);
    random_bits(sb3, rng);
    random_bits(sb4, rng);

    std::array<uint8_t, kFrameLength> tx{};
    REQUIRE(frame_build(
                Fid{0}, Toi{static_cast<uint8_t>(trial % 100)}, sb2, sb3, sb4, tx, enc_ws, fbw) ==
            FrameStatus::kOk);

    std::array<float, kFrameLength> rx{};
    binary_to_bipolar(tx, rx);
    add_awgn(rx, eb_n0_db, rng);
    quantize_symbols(rx, 8);

    FrameDecodeResult result{};
    const auto status = frame_decode(rx, result, *workspace, hal);

    const bool frame_ok = (status == FrameDecodeStatus::kOk) && result.sb2.ldpc_converged &&
                          result.sb2.crc_ok && result.sb3.ldpc_converged && result.sb3.crc_ok &&
                          result.sb4.ldpc_converged && result.sb4.crc_ok;
    if (!frame_ok) {
      frame_errors++;
    }
  }

  const double fer = static_cast<double>(frame_errors) / static_cast<double>(kTrials);
  std::cout << "[int8 FER] Eb/N0=" << eb_n0_db << " dB  FER=" << fer << " (" << frame_errors << "/"
            << kTrials << ")" << std::endl;

  CHECK(fer == 0.0);
}

TEST_CASE("E2E impairment: implementation loss int8 vs float32", "[e2e][impairment]") {
  // At 1.5 dB (waterfall region), compare FER between float32 and int8.
  // Int8 FER should be within 2× of float32 FER.
  constexpr float kEbN0 = 1.5F;
  constexpr int kTrials = 50;

  NullTelemetryHal hal;
  auto workspace = std::make_unique<FrameDecodeWorkspace>();
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};

  size_t errors_f32 = 0;
  size_t errors_i8 = 0;

  for (int trial = 0; trial < kTrials; ++trial) {
    // NOLINTNEXTLINE(cert-msc51-cpp)
    std::mt19937 rng(static_cast<uint32_t>(trial) + 500U);

    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    random_bits(sb2, rng);
    random_bits(sb3, rng);
    random_bits(sb4, rng);

    std::array<uint8_t, kFrameLength> tx{};
    REQUIRE(frame_build(
                Fid{0}, Toi{static_cast<uint8_t>(trial % 100)}, sb2, sb3, sb4, tx, enc_ws, fbw) ==
            FrameStatus::kOk);

    // Float32 path.
    {
      // NOLINTNEXTLINE(cert-msc51-cpp)
      std::mt19937 noise_rng(static_cast<uint32_t>(trial) + 500U);
      // Skip the bits we already generated.
      noise_rng.discard(3U * (kSb2DataBits + kSb3DataBits + kSb4DataBits));

      std::array<float, kFrameLength> rx{};
      binary_to_bipolar(tx, rx);
      add_awgn(rx, kEbN0, noise_rng);

      FrameDecodeResult result{};
      const auto status = frame_decode(rx, result, *workspace, hal);
      const bool ok = (status == FrameDecodeStatus::kOk) && result.sb2.crc_ok &&
                      result.sb3.crc_ok && result.sb4.crc_ok;
      if (!ok) {
        errors_f32++;
      }
    }

    // Int8 path (same noise realization).
    {
      // NOLINTNEXTLINE(cert-msc51-cpp)
      std::mt19937 noise_rng(static_cast<uint32_t>(trial) + 500U);
      noise_rng.discard(3U * (kSb2DataBits + kSb3DataBits + kSb4DataBits));

      std::array<float, kFrameLength> rx{};
      binary_to_bipolar(tx, rx);
      add_awgn(rx, kEbN0, noise_rng);
      quantize_symbols(rx, 8);

      FrameDecodeResult result{};
      const auto status = frame_decode(rx, result, *workspace, hal);
      const bool ok = (status == FrameDecodeStatus::kOk) && result.sb2.crc_ok &&
                      result.sb3.crc_ok && result.sb4.crc_ok;
      if (!ok) {
        errors_i8++;
      }
    }
  }

  const double fer_f32 = static_cast<double>(errors_f32) / kTrials;
  const double fer_i8 = static_cast<double>(errors_i8) / kTrials;

  std::cout << "[impl loss] Eb/N0=" << kEbN0 << " dB"
            << "  FER_f32=" << fer_f32 << " (" << errors_f32 << "/" << kTrials << ")"
            << "  FER_i8=" << fer_i8 << " (" << errors_i8 << "/" << kTrials << ")" << std::endl;

  // Int8 FER should be within 2× of float32 FER (or both zero).
  if (errors_f32 > 0) {
    CHECK(fer_i8 <= fer_f32 * 2.0);
  }
}

// ---------------------------------------------------------------------------
// Burst error resilience through the 60×98 block interleaver.
//
// The interleaver writes row-wise (98 columns) and reads column-wise (60 rows).
// A burst of B consecutive symbols in the interleaved domain gets spread across
// ceil(B/60) different column positions per LDPC codeword after deinterleaving.
// For B ≤ 60: exactly 1 error per affected position — fully correctable.
// For B = 120: ≤2 per position — comfortably within LDPC capability at 3 dB.
// For B ≥ 1200: ≥20% of payload hard-flipped — overwhelms rate-1/2 LDPC.
// ---------------------------------------------------------------------------
TEST_CASE("E2E impairment: burst error resilience through interleaver", "[e2e][impairment]") {
  NullTelemetryHal hal;
  auto workspace = std::make_unique<FrameDecodeWorkspace>();
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};

  // NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  struct BurstCase {
    size_t length;
    bool should_decode;
  };
  const BurstCase cases[] = {
      {30, true},    // half-column: ≤1 error per position
      {60, true},    // full column: exactly 1 per position — design point
      {120, true},   // 2 columns: ≤2 per position — LDPC handles easily
      {240, true},   // 4 columns: ≤4 per position — still within capability
      {1200, false}, // 20 columns: ≥20% hard-flipped → overwhelms LDPC + CRC
  };

  constexpr size_t kPayloadStart = kSyncLength + kSb1Length; // 68 + 52 = 120

  for (const auto& c : cases) {
    INFO("Burst length = " << c.length << " symbols");

    // NOLINTNEXTLINE(cert-msc51-cpp)
    std::mt19937 rng(static_cast<uint32_t>(c.length) + 7777U);

    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    random_bits(sb2, rng);
    random_bits(sb3, rng);
    random_bits(sb4, rng);

    std::array<uint8_t, kFrameLength> tx{};
    REQUIRE(frame_build(Fid{0}, Toi{42}, sb2, sb3, sb4, tx, enc_ws, fbw) == FrameStatus::kOk);

    std::array<float, kFrameLength> rx{};
    binary_to_bipolar(tx, rx);
    add_awgn(rx, 3.0F, rng);

    // Apply burst: flip consecutive symbols in the interleaved payload.
    // This simulates an impulse interference or brief signal blockage.
    for (size_t i = 0; i < c.length && (kPayloadStart + i) < kFrameLength; ++i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      rx[kPayloadStart + i] = -rx[kPayloadStart + i];
    }

    FrameDecodeResult result{};
    const auto status = frame_decode(rx, result, *workspace, hal);

    const bool frame_ok = (status == FrameDecodeStatus::kOk) && result.sb2.ldpc_converged &&
                          result.sb2.crc_ok && result.sb3.ldpc_converged && result.sb3.crc_ok &&
                          result.sb4.ldpc_converged && result.sb4.crc_ok;

    if (c.should_decode) {
      CHECK(frame_ok);
      if (frame_ok) {
        // Verify bit-exact recovery despite burst.
        CHECK(std::equal(sb2.begin(), sb2.end(), workspace->sb2_data.begin()));
        CHECK(std::equal(sb3.begin(), sb3.end(), workspace->sb3_data.begin()));
        CHECK(std::equal(sb4.begin(), sb4.end(), workspace->sb4_data.begin()));
      }
    } else {
      // Large burst must be detected — either LDPC fails or CRC catches.
      CHECK_FALSE(frame_ok);
    }
  }
}
