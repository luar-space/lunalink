// ---------------------------------------------------------------------------
// BCH(51,8) Decoder Performance Characterisation — BPSK AWGN
//
// Compares hard-decision ML (bch_decode) vs soft-decision ML
// (bch_decode_soft) over a sweep of Eb/N0 values.
//
// Code parameters:
//   - k = 9 information bits (2-bit FID + 7-bit TOI)
//   - n = 52 coded bits (1 raw MSB + 51 LFSR bits)
//   - Code rate R = 9/52 ≈ 0.173
//   - Minimum distance d_min ≥ 5 (t=2 BCH)
//   - Codebook size: 400 entries (4 FIDs × 100 TOIs)
//
// System operating point:
//   The SB1 header is transmitted at the same symbol energy as the LDPC
//   payload. LSIS spec: SNR > 0 dB (Es/N0 > 0 dB).
//   For BCH with R = 9/52: Eb/N0_BCH = Es/N0 + 10·log10(52/9) ≈ Es/N0 + 7.6 dB
//   → At the system operating point Es/N0 = 0 dB: Eb/N0_BCH ≈ 7.6 dB.
//
// All tests tagged [channel][ber] — excluded from default ctest runs.
// Run explicitly with: task ber
// ---------------------------------------------------------------------------
#include "lunalink/encoding/bch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <random>

using namespace lunalink::signal;

namespace {

constexpr double kBchRate = 9.0 / 52.0; // code rate R = k/n

struct BchSimResult {
  size_t total_frames;
  size_t hard_errors;
  size_t soft_errors;
  double hard_fer;
  double soft_fer;
};

// Run a single Eb/N0 point comparing hard ML vs soft ML.
//
// Hard path:  AWGN → threshold at zero → bch_decode (Hamming-distance ML)
// Soft path:  AWGN → channel LLR = 2y/σ² → bch_decode_soft (inner-product ML)
//
// A frame error = decoded FID or TOI ≠ transmitted value.
// Both decoders always output the ML-best codeword regardless of status code.
BchSimResult run_bch_sim(double eb_no_db, int frames, uint32_t seed) {
  const double eb_no_lin = std::pow(10.0, eb_no_db / 10.0);
  const double sigma = 1.0 / std::sqrt(2.0 * kBchRate * eb_no_lin);
  const double sigma_sq = sigma * sigma;

  std::mt19937 gen(seed);
  std::uniform_int_distribution<uint8_t> fid_dist(0U, 3U);
  std::uniform_int_distribution<uint8_t> toi_dist(0U, 99U);
  std::normal_distribution<double> noise(0.0, sigma);

  std::array<uint8_t, kBchCodewordLength> codeword{};
  std::array<uint8_t, kBchCodewordLength> hard_rx{};
  std::array<float, kBchCodewordLength> soft_rx{};

  size_t hard_errors = 0;
  size_t soft_errors = 0;

  for (int f = 0; f < frames; ++f) {
    const uint8_t fid = fid_dist(gen);
    const uint8_t toi = toi_dist(gen);

    REQUIRE(bch_encode(Fid(fid), Toi(toi), codeword) == BchStatus::kOk);

    // Transmit over BPSK AWGN: bit=0 → +1, bit=1 → −1
    for (uint32_t i = 0; i < kBchCodewordLength; ++i) {
      const double s = (codeword[i] == 0U) ? 1.0 : -1.0;
      const double r = s + noise(gen);

      hard_rx[i] = (r < 0.0) ? 1U : 0U;                    // hard decision
      soft_rx[i] = static_cast<float>(2.0 * r / sigma_sq); // channel LLR
    }

    const auto hard_res = bch_decode(hard_rx);
    const auto soft_res = bch_decode_soft(soft_rx);

    if (hard_res.fid.value() != fid || hard_res.toi.value() != toi) {
      ++hard_errors;
    }
    if (soft_res.fid.value() != fid || soft_res.toi.value() != toi) {
      ++soft_errors;
    }
  }

  const double n = static_cast<double>(frames);
  return {static_cast<size_t>(frames),
          hard_errors,
          soft_errors,
          static_cast<double>(hard_errors) / n,
          static_cast<double>(soft_errors) / n};
}

void print_bch_result(double eb_no_db, const BchSimResult& r) {
  std::cout << "[BCH BER] Eb/N0=" << eb_no_db << " dB"
            << "  hard FER=" << r.hard_fer << " (" << r.hard_errors << "/" << r.total_frames << ")"
            << "  soft FER=" << r.soft_fer << " (" << r.soft_errors << "/" << r.total_frames << ")"
            << std::endl;
}

} // namespace

// ---------------------------------------------------------------------------
// Zone 1 — Noise floor (2.0 dB)
//
// At Eb/N0 = 2.0 dB: σ ≈ 1.35, raw BER ≈ 23%, ~12 errors/codeword on average.
// Even exhaustive ML cannot always recover from this level of corruption.
// Both decoders fail on many frames — proves the channel simulation is real.
//
// Expected: hard FER ≈ 15%, soft FER ≈ 2%.
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: noise floor at 2.0 dB", "[channel][ber]") {
  constexpr int kFrames = 1000;

  auto r = run_bch_sim(2.0, kFrames, 42U);
  print_bch_result(2.0, r);

  CHECK(r.hard_fer > 0.05);       // hard: substantial failure rate
  CHECK(r.soft_fer > 0.005);      // soft: also failing — channel is genuinely noisy
  CHECK(r.hard_fer > r.soft_fer); // soft ML already outperforms at noise floor
}

// ---------------------------------------------------------------------------
// Zone 2 — Waterfall sweep (3.0 – 6.0 dB)
//
// 3,000 frames per Eb/N0 point.  Key results (representative, seed=77):
//
//   Eb/N0   hard FER     soft FER    gain
//   3.0 dB  ~6.0%        ~0.9%       ~8x
//   4.0 dB  ~1.9%        ~0.1%       ~19x
//   5.0 dB  ~0.5%         0          soft error-free, hard still failing
//   6.0 dB  ~0.1%         0          hard residual errors remain
//
// Soft ML reaches effective error-free operation ~2 dB before hard ML.
// This 2 dB gain translates directly into link-budget margin for SB1.
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: waterfall sweep 3-6 dB", "[channel][ber]") {
  constexpr int kFrames = 3000;

  for (double eb_no : {3.0, 4.0, 5.0, 6.0}) {
    auto r = run_bch_sim(eb_no, kFrames, 77U);
    print_bch_result(eb_no, r);

    // Soft ML must never be worse than hard ML
    CHECK(r.soft_fer <= r.hard_fer);
  }

  // At 5.0 dB: hard still has residual errors; soft is near-zero.
  // Soft decoder reaches negligible FER ~2 dB before hard ML.
  // Statistical bound: ≤2 errors in 3000 frames (FER < 1e-3).
  auto r5 = run_bch_sim(5.0, kFrames, 137U);
  CHECK(r5.hard_fer > 0.0);
  CHECK(r5.soft_fer < 1e-3);
}

// ---------------------------------------------------------------------------
// Zone 3 — System operating point (7.6 dB = Es/N0 0 dB for BCH R=9/52)
//
// In the LSIS-AFS system, SB1 and the LDPC payload share the same channel:
//
//   Code         Rate       Eb/N0 at Es/N0 = 0 dB
//   LDPC SF2     1/2        3.0 dB    (validated in test_ldpc_ber.cpp)
//   BCH SB1      9/52       7.6 dB    (validated here)
//
// The BCH has ~4.6 dB more Eb/N0 at the same physical SNR, giving it
// substantial additional margin over the LDPC operating point.
//
// At 7.6 dB, 10,000 frames: both decoders should have negligible FER.
// Statistical bound: ≤4 errors in 10,000 frames (FER < 5×10⁻⁴).
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: operating point 7.6 dB", "[channel][ber]") {
  constexpr int kFrames = 10000;

  auto r = run_bch_sim(7.6, kFrames, 99U);
  print_bch_result(7.6, r);

  CHECK(r.hard_fer < 5e-4);
  CHECK(r.soft_fer < 5e-4);
}
