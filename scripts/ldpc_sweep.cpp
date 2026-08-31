// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// ldpc_sweep.cpp — Parallel 2D α×β sweep for Adjusted Min-Sum LDPC decoder.
// Standalone tool, not a Catch2 test. Compile with:
//   cmake --build build/ber --target ldpc_sweep
// Run:
//   ./build/ber/ldpc_sweep

#include "lunalink/encoding/ldpc.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

using namespace lunalink::signal;

struct SweepResult {
  int16_t alpha_num;
  int16_t alpha_den;
  int16_t beta;
  double fer;
  double ber;
  double ci;
  size_t total_frames;
  size_t frame_errors;
};

// Wilson score 95% CI half-width.
double wilson_ci_hw(size_t errors, size_t trials) {
  constexpr double z = 1.96;
  const double n = static_cast<double>(trials);
  const double p = static_cast<double>(errors) / n;
  const double denom = 1.0 + z * z / n;
  return z * std::sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denom;
}

SweepResult run_point(LdpcSubframe type, double eb_no_db,
                      int frames_per_seed, uint16_t k, uint16_t n,
                      uint8_t max_iters, int16_t alpha_num, int16_t alpha_den,
                      int16_t beta) {
  constexpr std::array<uint32_t, 3> seeds = {42, 137, 313};
  size_t total_bits = 0;
  size_t total_errors = 0;
  size_t frame_errors = 0;
  size_t total_frames = 0;

  for (auto seed : seeds) {
    std::mt19937 gen(seed);
    const double code_rate = static_cast<double>(k) / static_cast<double>(n);
    const double eb_no_lin = std::pow(10.0, eb_no_db / 10.0);
    const double sigma = 1.0 / std::sqrt(2.0 * code_rate * eb_no_lin);
    const double sigma_sq = sigma * sigma;
    const double llr_scale = 2.0 / sigma_sq;
    std::normal_distribution<double> noise(0.0, sigma);

    std::vector<uint8_t> message(k);
    std::vector<uint8_t> encoded(n);
    std::vector<float> soft(n);
    std::vector<uint8_t> decoded(k);
    LdpcEncodeWorkspace enc_ws{};
    LdpcWorkspace ws{};
    ws.alpha.numerator = alpha_num;
    ws.alpha.denominator = alpha_den;
    ws.beta = beta;

    for (int f = 0; f < frames_per_seed; ++f) {
      for (auto& b : message) {
        b = static_cast<uint8_t>(gen() % 2);
      }
      ldpc_encode(type, message, encoded, enc_ws);
      for (size_t i = 0; i < n; ++i) {
        double mod = (encoded[i] == 1) ? -1.0 : 1.0;
        soft[i] = static_cast<float>((mod + noise(gen)) * llr_scale);
      }
      ldpc_decode(type, soft, max_iters, decoded, ws);
      size_t errs = 0;
      for (size_t i = 0; i < k; ++i) {
        if (message[i] != decoded[i]) ++errs;
      }
      total_errors += errs;
      total_bits += k;
      if (errs > 0) ++frame_errors;
    }
    total_frames += static_cast<size_t>(frames_per_seed);
  }

  double fer = static_cast<double>(frame_errors) / static_cast<double>(total_frames);
  double ber = static_cast<double>(total_errors) / static_cast<double>(total_bits);
  double ci = wilson_ci_hw(frame_errors, total_frames);
  return {alpha_num, alpha_den, beta, fer, ber, ci, total_frames, frame_errors};
}

int main() {
  constexpr uint8_t kIter = 50;
  constexpr int kFramesPerSeed = 5000;
  constexpr double kEbN0 = 1.2;

  struct Alpha { int16_t num; int16_t den; };
  // Focused on the winning region from coarse sweep
  // NOLINTNEXTLINE
  const Alpha alphas[] = {
      {5, 7}, {3, 4}, {4, 5}, {5, 6}, {7, 8},
  };
  // Fine β grid around the 16-48 sweet spot (Q8.7)
  // NOLINTNEXTLINE
  const int16_t betas[] = {0, 8, 12, 16, 20, 24, 28, 32, 40, 48, 64};

  // Build work items
  struct WorkItem { Alpha alpha; int16_t beta; };
  std::vector<WorkItem> items;
  for (const auto& a : alphas) {
    for (const auto b : betas) {
      items.push_back({a, b});
    }
  }

  std::vector<SweepResult> results(items.size());
  std::atomic<size_t> next_item{0};
  std::mutex print_mtx;

  auto worker = [&]() {
    while (true) {
      size_t idx = next_item.fetch_add(1);
      if (idx >= items.size()) break;
      const auto& item = items[idx];
      results[idx] = run_point(LdpcSubframe::kSF2, kEbN0, kFramesPerSeed,
                               1200, 2400, kIter,
                               item.alpha.num, item.alpha.den, item.beta);
      std::lock_guard<std::mutex> lock(print_mtx);
      const auto& r = results[idx];
      double af = static_cast<double>(r.alpha_num) / static_cast<double>(r.alpha_den);
      double bf = static_cast<double>(r.beta) / 128.0;
      std::printf("[%3zu/%zu] α=%.3f β=%.3f  FER=%.5f ± %.5f  BER=%.6f  [%zu frames]\n",
                  idx + 1, items.size(), af, bf,
                  r.fer, r.ci, r.ber, r.total_frames);
    }
  };

  unsigned n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0) n_threads = 4;
  std::printf("Running %zu combinations on %u threads (%d frames x 3 seeds each)\n",
              items.size(), n_threads, kFramesPerSeed);

  std::vector<std::thread> threads;
  for (unsigned t = 0; t < n_threads; ++t) {
    threads.emplace_back(worker);
  }
  for (auto& t : threads) t.join();

  // Find best
  size_t best_idx = 0;
  for (size_t i = 1; i < results.size(); ++i) {
    if (results[i].fer < results[best_idx].fer) best_idx = i;
  }
  const auto& best = results[best_idx];
  double af = static_cast<double>(best.alpha_num) / static_cast<double>(best.alpha_den);
  double bf = static_cast<double>(best.beta) / 128.0;

  std::printf("\n=== BEST ===\n");
  std::printf("α=%d/%d (%.4f)  β=%d (%.4f)  FER=%.5f ± %.5f  [%zu frames]\n",
              best.alpha_num, best.alpha_den, af, best.beta, bf,
              best.fer, best.ci, best.total_frames);

  // Print sorted table
  std::printf("\n=== FULL TABLE (sorted by FER) ===\n");
  std::vector<size_t> order(results.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return results[a].fer < results[b].fer;
  });
  for (auto i : order) {
    const auto& r = results[i];
    double a = static_cast<double>(r.alpha_num) / static_cast<double>(r.alpha_den);
    double b = static_cast<double>(r.beta) / 128.0;
    std::printf("α=%.3f β=%.3f  FER=%.5f ± %.5f  BER=%.6f\n",
                a, b, r.fer, r.ci, r.ber);
  }

  return 0;
}
