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

// Wilson score 95% CI half-width — handles p=0 and p=1 gracefully.
double wilson_ci_hw(size_t errors, size_t trials) {
  constexpr double z = 1.96;
  const double n = static_cast<double>(trials);
  const double p = static_cast<double>(errors) / n;
  const double denom = 1.0 + z * z / n;
  return z * std::sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denom;
}

struct BchMultiSeedResult {
  double hard_fer_mean;
  double soft_fer_mean;
  double hard_fer_std;
  double soft_fer_std;
  size_t total_frames;
  size_t total_hard_errors;
  size_t total_soft_errors;
};

BchMultiSeedResult run_bch_multi_seed(double eb_no_db, int frames_per_seed) {
  constexpr size_t kNumSeeds = 3;
  constexpr std::array<uint32_t, kNumSeeds> seeds = {42, 137, 313};
  std::array<double, kNumSeeds> hard_fers{};
  std::array<double, kNumSeeds> soft_fers{};
  size_t total_he = 0;
  size_t total_se = 0;
  size_t total_f = 0;

  for (size_t s = 0; s < kNumSeeds; ++s) {
    auto r = run_bch_sim(eb_no_db, frames_per_seed, seeds.at(s));
    hard_fers.at(s) = r.hard_fer;
    soft_fers.at(s) = r.soft_fer;
    total_he += r.hard_errors;
    total_se += r.soft_errors;
    total_f += r.total_frames;
  }

  double hf_sum = 0;
  double sf_sum = 0;
  for (size_t s = 0; s < kNumSeeds; ++s) {
    hf_sum += hard_fers.at(s);
    sf_sum += soft_fers.at(s);
  }
  const double hf_mean = hf_sum / static_cast<double>(kNumSeeds);
  const double sf_mean = sf_sum / static_cast<double>(kNumSeeds);

  double hf_var = 0;
  double sf_var = 0;
  for (size_t s = 0; s < kNumSeeds; ++s) {
    hf_var += (hard_fers.at(s) - hf_mean) * (hard_fers.at(s) - hf_mean);
    sf_var += (soft_fers.at(s) - sf_mean) * (soft_fers.at(s) - sf_mean);
  }

  return {hf_mean,
          sf_mean,
          std::sqrt(hf_var / static_cast<double>(kNumSeeds)),
          std::sqrt(sf_var / static_cast<double>(kNumSeeds)),
          total_f,
          total_he,
          total_se};
}

void print_bch_ci_result(double eb_no_db, const BchMultiSeedResult& r, int frames_per_seed) {
  double hci = wilson_ci_hw(r.total_hard_errors, r.total_frames);
  double sci = wilson_ci_hw(r.total_soft_errors, r.total_frames);
  std::cout << "[BCH BER] Eb/N0=" << eb_no_db << " dB"
            << "  hard FER=" << r.hard_fer_mean << " +/- " << hci
            << "  soft FER=" << r.soft_fer_mean << " +/- " << sci << " (95% CI)"
            << "  [" << r.total_frames << " frames, 3 seeds x " << frames_per_seed << "]"
            << std::endl;
}

} // namespace

// ---------------------------------------------------------------------------
// Zone 1 — Noise floor (2.0 dB)
// 3000 frames × 3 seeds = 9000 total.
//
// At Eb/N0 = 2.0 dB: σ ≈ 1.35, raw BER ≈ 23%, ~12 errors/codeword on average.
// Even exhaustive ML cannot always recover from this level of corruption.
// Both decoders fail on many frames — proves the channel simulation is real.
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: noise floor at 2.0 dB", "[channel][ber][reference][slow]") {
  constexpr int kFramesPerSeed = 3000;

  auto r = run_bch_multi_seed(2.0, kFramesPerSeed);
  print_bch_ci_result(2.0, r, kFramesPerSeed);

  CHECK(r.hard_fer_mean > 0.05);            // hard: substantial failure rate
  CHECK(r.soft_fer_mean > 0.005);           // soft: also failing — channel is genuinely noisy
  CHECK(r.hard_fer_mean > r.soft_fer_mean); // soft ML outperforms at noise floor
}

// ---------------------------------------------------------------------------
// Zone 2 — Waterfall sweep (3.0 – 6.0 dB)
// 3000 frames × 3 seeds per Eb/N0 point = 9000 total per point.
//
// Soft ML reaches effective error-free operation ~1.6 dB before hard ML.
// This gain translates directly into link-budget margin for SB1.
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: waterfall sweep 3-6 dB", "[channel][ber][reference][slow]") {
  constexpr int kFramesPerSeed = 3000;

  double prev_hard_fer = 1.0;
  double prev_soft_fer = 1.0;

  // NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  const double points[] = {3.0, 4.0, 5.0, 6.0};

  for (const double eb_no : points) {
    auto r = run_bch_multi_seed(eb_no, kFramesPerSeed);
    print_bch_ci_result(eb_no, r, kFramesPerSeed);

    // Soft ML must never be worse than hard ML
    CHECK(r.soft_fer_mean <= r.hard_fer_mean);

    // CI-based monotonicity for both decoders
    double hci = wilson_ci_hw(r.total_hard_errors, r.total_frames);
    double sci = wilson_ci_hw(r.total_soft_errors, r.total_frames);
    CHECK(r.hard_fer_mean <= prev_hard_fer + hci);
    CHECK(r.soft_fer_mean <= prev_soft_fer + sci);
    prev_hard_fer = r.hard_fer_mean;
    prev_soft_fer = r.soft_fer_mean;
  }

  // At 6.0 dB: soft should be error-free, hard should have residual errors
  auto r6 = run_bch_multi_seed(6.0, kFramesPerSeed);
  double soft_ci = wilson_ci_hw(r6.total_soft_errors, r6.total_frames);
  std::cout << "[BCH BER] 6.0 dB soft upper bound: " << soft_ci << " (95% CI, " << r6.total_frames
            << " frames)" << std::endl;
  CHECK(r6.hard_fer_mean > 0.0);  // hard still has residual errors
  CHECK(r6.soft_fer_mean < 1e-3); // soft near error-free
}

// ---------------------------------------------------------------------------
// Zone 3 — System operating point (7.6 dB = Es/N0 0 dB for BCH R=9/52)
// 10000 frames × 3 seeds = 30000 total.
//
// At 7.6 dB both decoders should have negligible FER.
// Upper bound on true FER: < 0.013% (Wilson 95% CI with 0 errors in 30k).
// ---------------------------------------------------------------------------
TEST_CASE("BCH hard vs soft: operating point 7.6 dB", "[channel][ber][reference][slow]") {
  constexpr int kFramesPerSeed = 10000;

  auto r = run_bch_multi_seed(7.6, kFramesPerSeed);
  print_bch_ci_result(7.6, r, kFramesPerSeed);

  double hci = wilson_ci_hw(r.total_hard_errors, r.total_frames);
  double sci = wilson_ci_hw(r.total_soft_errors, r.total_frames);
  std::cout << "[BCH BER] 7.6 dB hard upper bound: " << hci << "  soft upper bound: " << sci
            << " (95% CI, " << r.total_frames << " frames)" << std::endl;

  CHECK(r.hard_fer_mean < 5e-4);
  CHECK(r.soft_fer_mean < 5e-4);
}
