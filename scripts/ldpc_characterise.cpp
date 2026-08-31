// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// ldpc_characterise.cpp — Parallel LDPC decoder performance characterisation.
// Standalone tool, outputs structured JSON to stdout.
// Compile: cmake --build build/algo_card --target ldpc_characterise
// Run:     build/algo_card/ldpc_characterise > build/algo_card/sp_results.json

#include "lunalink/encoding/ldpc.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// Simulation helpers
// ---------------------------------------------------------------------------

struct BerResult {
  double fer;
  double ber;
  double ci_fer; // Wilson 95% CI half-width on FER
  size_t frame_errors;
  size_t total_frames;
  size_t total_errors;
  size_t total_bits;
  size_t not_converged;
};

double wilson_ci_hw(size_t errors, size_t trials) {
  constexpr double z = 1.96;
  const double n = static_cast<double>(trials);
  const double p = static_cast<double>(errors) / n;
  const double denom = 1.0 + z * z / n;
  return z * std::sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denom;
}

// Core BER simulation. Returns aggregate results across 3 seeds.
BerResult run_simulation(LdpcSubframe type,
                         double eb_no_db,
                         int frames_per_seed,
                         uint16_t k_nominal,
                         uint16_t n_output,
                         uint8_t max_iters,
                         int quantize_bits = 0,
                         bool all_zero_msg = false) {
  constexpr std::array<uint32_t, 3> seeds = {42, 137, 313};
  size_t total_bits = 0;
  size_t total_errors = 0;
  size_t frame_errors = 0;
  size_t total_frames = 0;
  size_t not_converged = 0;

  for (auto seed : seeds) {
    std::mt19937 gen(seed);
    const double code_rate = static_cast<double>(k_nominal) / static_cast<double>(n_output);
    const double eb_no_lin = std::pow(10.0, eb_no_db / 10.0);
    const double sigma = 1.0 / std::sqrt(2.0 * code_rate * eb_no_lin);
    const double sigma_sq = sigma * sigma;
    const double llr_scale = 2.0 / sigma_sq;
    std::normal_distribution<double> noise(0.0, sigma);

    std::vector<uint8_t> message(k_nominal);
    std::vector<uint8_t> encoded(n_output);
    std::vector<float> soft(n_output);
    std::vector<uint8_t> decoded(k_nominal);
    LdpcEncodeWorkspace enc_ws{};
    LdpcWorkspace ws{};

    for (int f = 0; f < frames_per_seed; ++f) {
      if (all_zero_msg) {
        std::fill(message.begin(), message.end(), static_cast<uint8_t>(0));
      } else {
        for (auto& b : message) {
          b = static_cast<uint8_t>(gen() % 2);
        }
      }
      auto enc_st = ldpc_encode(type, message, encoded, enc_ws);
      if (enc_st != LdpcStatus::kOk) {
        continue;
      }
      for (size_t i = 0; i < n_output; ++i) {
        double mod = (encoded[i] == 1) ? -1.0 : 1.0;
        double recv = mod + noise(gen);
        auto llr = static_cast<float>(recv * llr_scale);
        if (quantize_bits > 0) {
          // Peak-normalised quantisation
          auto scale_q = static_cast<float>((1 << (quantize_bits - 1)) - 1);
          // Approximate peak from sigma
          float peak = static_cast<float>(llr_scale * (1.0 + 4.0 * sigma));
          if (peak > 1e-6F) {
            float gain = scale_q / peak;
            llr = std::clamp(std::round(llr * gain), -scale_q, scale_q) / gain;
          }
        }
        soft[i] = llr;
      }

      auto status = ldpc_decode(type, soft, max_iters, decoded, ws);
      if (status == LdpcStatus::kNotConverged) {
        ++not_converged;
      }

      size_t errs = 0;
      for (size_t i = 0; i < k_nominal; ++i) {
        if (message[i] != decoded[i]) {
          ++errs;
        }
      }
      total_errors += errs;
      total_bits += k_nominal;
      if (errs > 0) {
        ++frame_errors;
      }
    }
    total_frames += static_cast<size_t>(frames_per_seed);
  }

  double fer = static_cast<double>(frame_errors) / static_cast<double>(total_frames);
  double ber = static_cast<double>(total_errors) / static_cast<double>(total_bits);
  double ci = wilson_ci_hw(frame_errors, total_frames);
  return {fer, ber, ci, frame_errors, total_frames, total_errors, total_bits, not_converged};
}

// Bit-error pattern simulation: returns per-frame error counts.
struct ErrorPatternResult {
  std::vector<size_t> errors_per_frame; // error count for each frame
  size_t total_frames;
  size_t frame_errors; // frames with any errors
  size_t max_errors;   // worst frame
};

ErrorPatternResult run_error_pattern(LdpcSubframe type,
                                     double eb_no_db,
                                     int frames,
                                     uint16_t k_nominal,
                                     uint16_t n_output,
                                     uint8_t max_iters) {
  std::mt19937 gen(42);
  const double code_rate = static_cast<double>(k_nominal) / static_cast<double>(n_output);
  const double eb_no_lin = std::pow(10.0, eb_no_db / 10.0);
  const double sigma = 1.0 / std::sqrt(2.0 * code_rate * eb_no_lin);
  const double llr_scale = 2.0 / (sigma * sigma);
  std::normal_distribution<double> noise(0.0, sigma);

  std::vector<uint8_t> message(k_nominal);
  std::vector<uint8_t> encoded(n_output);
  std::vector<float> soft(n_output);
  std::vector<uint8_t> decoded(k_nominal);
  LdpcEncodeWorkspace enc_ws{};
  LdpcWorkspace ws{};

  ErrorPatternResult result;
  result.total_frames = static_cast<size_t>(frames);
  result.frame_errors = 0;
  result.max_errors = 0;

  for (int f = 0; f < frames; ++f) {
    for (auto& b : message) {
      b = static_cast<uint8_t>(gen() % 2);
    }
    auto enc_st = ldpc_encode(type, message, encoded, enc_ws);
    if (enc_st != LdpcStatus::kOk) {
      continue;
    }
    for (size_t i = 0; i < n_output; ++i) {
      double mod = (encoded[i] == 1) ? -1.0 : 1.0;
      soft[i] = static_cast<float>((mod + noise(gen)) * llr_scale);
    }
    (void)ldpc_decode(type, soft, max_iters, decoded, ws);

    size_t errs = 0;
    for (size_t i = 0; i < k_nominal; ++i) {
      if (message[i] != decoded[i]) {
        ++errs;
      }
    }
    if (errs > 0) {
      result.errors_per_frame.push_back(errs);
      ++result.frame_errors;
      if (errs > result.max_errors) {
        result.max_errors = errs;
      }
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Work item system
// ---------------------------------------------------------------------------

enum class SimType {
  kWaterfall,
  kErrorFloor,
  kConvergence,
  kLossBudget,
  kSymmetry,
  kQuantisation,
  kErrorPattern,
  kSaturation,
};

struct WorkItem {
  SimType type;
  LdpcSubframe subframe;
  double eb_no_db;
  int frames_per_seed;
  uint16_t k;
  uint16_t n;
  uint8_t max_iters;
  int quantize_bits;
  bool all_zero;
  std::string label;
};

struct WorkResult {
  std::string label;
  SimType type;
  BerResult ber;
  ErrorPatternResult patterns; // only for kErrorPattern
};

// ---------------------------------------------------------------------------
// JSON output helpers
// ---------------------------------------------------------------------------

void json_ber_point(const char* indent, const BerResult& r, double eb_no_db, bool last) {
  std::printf("%s{\"eb_n0_db\": %.2f, \"fer\": %.8f, \"ber\": %.10f, "
              "\"ci_fer\": %.8f, \"frames\": %zu, \"frame_errors\": %zu, "
              "\"bit_errors\": %zu, \"total_bits\": %zu, \"not_converged\": %zu}%s\n",
              indent, eb_no_db, r.fer, r.ber, r.ci_fer,
              r.total_frames, r.frame_errors,
              r.total_errors, r.total_bits, r.not_converged,
              last ? "" : ",");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  constexpr uint8_t kIter = 50;

  // --- Build work items ---
  std::vector<WorkItem> items;

  // 1. SF2 high-res waterfall (11 points)
  for (double snr : {0.2, 0.4, 0.6, 0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 2.0, 3.0}) {
    char label[64];
    std::snprintf(label, sizeof(label), "waterfall_sf2_%.1f", snr);
    items.push_back({SimType::kWaterfall, LdpcSubframe::kSF2, snr, 5000,
                     1200, 2400, kIter, 0, false, label});
  }

  // 2. SF3 high-res waterfall
  for (double snr : {0.2, 0.4, 0.6, 0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 2.0, 3.0}) {
    char label[64];
    std::snprintf(label, sizeof(label), "waterfall_sf3_%.1f", snr);
    items.push_back({SimType::kWaterfall, LdpcSubframe::kSF3, snr, 5000,
                     870, 1740, kIter, 0, false, label});
  }

  // 3. Error floor probe SF2
  for (double snr : {2.5, 3.0}) {
    char label[64];
    std::snprintf(label, sizeof(label), "floor_sf2_%.1f", snr);
    items.push_back({SimType::kErrorFloor, LdpcSubframe::kSF2, snr, 20000,
                     1200, 2400, kIter, 0, false, label});
  }

  // 4. Error floor probe SF3
  for (double snr : {2.5, 3.0}) {
    char label[64];
    std::snprintf(label, sizeof(label), "floor_sf3_%.1f", snr);
    items.push_back({SimType::kErrorFloor, LdpcSubframe::kSF3, snr, 20000,
                     870, 1740, kIter, 0, false, label});
  }

  // 5. Convergence CDF — sweep max_iters at 1.5 dB
  for (uint8_t it : {1, 2, 3, 5, 7, 10, 15, 20, 25, 30, 40, 50}) {
    char label[64];
    std::snprintf(label, sizeof(label), "convergence_%d", static_cast<int>(it));
    items.push_back({SimType::kConvergence, LdpcSubframe::kSF2, 1.5, 2000,
                     1200, 2400, it, 0, false, label});
  }

  // 6. Implementation loss budget at 1.2 dB
  // a) Baseline (float LLRs)
  items.push_back({SimType::kLossBudget, LdpcSubframe::kSF2, 1.2, 3000,
                   1200, 2400, kIter, 0, false, "loss_baseline"});
  // b) int8 quantised LLRs
  items.push_back({SimType::kLossBudget, LdpcSubframe::kSF2, 1.2, 3000,
                   1200, 2400, kIter, 8, false, "loss_int8"});
  // c) int4 quantised LLRs
  items.push_back({SimType::kLossBudget, LdpcSubframe::kSF2, 1.2, 3000,
                   1200, 2400, kIter, 4, false, "loss_int4"});
  // d) Reduced iterations (15)
  items.push_back({SimType::kLossBudget, LdpcSubframe::kSF2, 1.2, 3000,
                   1200, 2400, 15, 0, false, "loss_15iter"});

  // 7. All-zero codeword symmetry at 1.2 dB
  items.push_back({SimType::kSymmetry, LdpcSubframe::kSF2, 1.2, 5000,
                   1200, 2400, kIter, 0, false, "symmetry_random"});
  items.push_back({SimType::kSymmetry, LdpcSubframe::kSF2, 1.2, 5000,
                   1200, 2400, kIter, 0, true, "symmetry_allzero"});

  // 8. Quantisation sweep (additional points for the plot)
  for (int bits : {3, 4, 5, 6, 8}) {
    char label[64];
    std::snprintf(label, sizeof(label), "quant_%dbit", bits);
    items.push_back({SimType::kQuantisation, LdpcSubframe::kSF2, 1.2, 3000,
                     1200, 2400, kIter, bits, false, label});
  }
  items.push_back({SimType::kQuantisation, LdpcSubframe::kSF2, 1.2, 3000,
                   1200, 2400, kIter, 0, false, "quant_float"});

  // 9 & 10: Error pattern and saturation (single-threaded, handled separately)

  // --- Run parallel simulations ---
  std::vector<WorkResult> results(items.size());
  std::atomic<size_t> next_item{0};
  std::mutex print_mtx;
  auto t_start = std::chrono::steady_clock::now();

  auto worker = [&]() {
    while (true) {
      size_t idx = next_item.fetch_add(1);
      if (idx >= items.size()) {
        break;
      }
      const auto& item = items[idx];
      auto r = run_simulation(item.subframe, item.eb_no_db,
                              item.frames_per_seed, item.k, item.n,
                              item.max_iters, item.quantize_bits, item.all_zero);
      results[idx] = {item.label, item.type, r, {}};

      std::lock_guard<std::mutex> lock(print_mtx);
      std::fprintf(stderr, "[%3zu/%zu] %-30s Eb/N0=%4.1f dB  FER=%.5f ± %.5f\n",
                   idx + 1, items.size(), item.label.c_str(),
                   item.eb_no_db, r.fer, r.ci_fer);
    }
  };

  unsigned n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0) {
    n_threads = 4;
  }
  std::fprintf(stderr, "Running %zu work items on %u threads...\n",
               items.size(), n_threads);

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) {
    threads.emplace_back(worker);
  }
  for (auto& t : threads) {
    t.join();
  }

  // 9. Error pattern analysis at multiple SNR points (single-threaded — needs per-frame data)
  // 1.0 dB: deep waterfall (FER ~5%), 1.2 dB: mid-cliff (FER ~1%), 1.4 dB: near error-free
  struct PatternPoint { double eb_n0; int frames; ErrorPatternResult result; };
  std::vector<PatternPoint> pattern_points = {{1.0, 5000, {}}, {1.2, 10000, {}}, {1.4, 20000, {}}};
  for (auto& pp : pattern_points) {
    std::fprintf(stderr, "Running error pattern analysis at %.1f dB...\n", pp.eb_n0);
    pp.result = run_error_pattern(LdpcSubframe::kSF2, pp.eb_n0, pp.frames, 1200, 2400, kIter);
  }

  // 10. Saturation stress
  std::fprintf(stderr, "Running saturation stress tests...\n");
  auto sat_low = run_simulation(LdpcSubframe::kSF2, 0.1, 1000, 1200, 2400, kIter);
  auto sat_high = run_simulation(LdpcSubframe::kSF2, 10.0, 1000, 1200, 2400, kIter);

  auto t_end = std::chrono::steady_clock::now();
  double elapsed_s = std::chrono::duration<double>(t_end - t_start).count();
  std::fprintf(stderr, "Done in %.1f seconds.\n", elapsed_s);

  // --- Output JSON ---
  std::printf("{\n");
  std::printf("  \"decoder\": \"sum-product\",\n");
  std::printf("  \"algorithm\": \"Layered Sum-Product BP (phi-transform)\",\n");
  std::printf("  \"arithmetic\": \"float64\",\n");
  std::printf("  \"max_iterations\": %d,\n", static_cast<int>(kIter));
  std::printf("  \"early_termination\": \"syndrome check every iteration\",\n");
  std::printf("  \"threads\": %u,\n", n_threads);
  std::printf("  \"elapsed_seconds\": %.1f,\n", elapsed_s);

  // Helper: collect indices matching a label prefix
  auto collect = [&](const char* prefix) {
    std::vector<size_t> idxs;
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].label.rfind(prefix, 0) == 0) {
        idxs.push_back(i);
      }
    }
    return idxs;
  };

  auto print_ber_array = [&](const char* key, const char* prefix) {
    auto idxs = collect(prefix);
    std::printf("  \"%s\": [\n", key);
    for (size_t j = 0; j < idxs.size(); ++j) {
      size_t i = idxs[j];
      json_ber_point("    ", results[i].ber, items[i].eb_no_db, j + 1 == idxs.size());
    }
    std::printf("  ],\n");
  };

  print_ber_array("waterfall_sf2", "waterfall_sf2_");
  print_ber_array("waterfall_sf3", "waterfall_sf3_");

  // Error floor
  std::printf("  \"error_floor\": {\n");
  for (size_t i = 0; i < results.size(); ++i) {
    if (results[i].label.rfind("floor_", 0) == 0) {
      std::printf("    \"%s\": ", results[i].label.c_str());
      const auto& r = results[i].ber;
      std::printf("{\"fer\": %.10f, \"ber\": %.12f, \"ci_fer\": %.10f, "
                  "\"frames\": %zu, \"frame_errors\": %zu, \"total_bits\": %zu, "
                  "\"ci_fer_upper\": %.10f}",
                  r.fer, r.ber, r.ci_fer,
                  r.total_frames, r.frame_errors, r.total_bits,
                  (r.frame_errors == 0) ? wilson_ci_hw(0, r.total_frames) : r.fer + r.ci_fer);
      // Check if last floor item
      bool is_last = true;
      for (size_t j = i + 1; j < results.size(); ++j) {
        if (results[j].label.rfind("floor_", 0) == 0) {
          is_last = false;
          break;
        }
      }
      std::printf("%s\n", is_last ? "" : ",");
    }
  }
  std::printf("  },\n");

  // Convergence CDF
  {
    auto idxs = collect("convergence_");
    std::printf("  \"convergence_cdf\": [\n");
    for (size_t j = 0; j < idxs.size(); ++j) {
      size_t i = idxs[j];
      std::printf("    {\"max_iters\": %d, \"fer\": %.8f, \"ci_fer\": %.8f, "
                  "\"not_converged\": %zu, \"frames\": %zu}%s\n",
                  static_cast<int>(items[i].max_iters),
                  results[i].ber.fer, results[i].ber.ci_fer,
                  results[i].ber.not_converged, results[i].ber.total_frames,
                  (j + 1 < idxs.size()) ? "," : "");
    }
    std::printf("  ],\n");
  }

  // Loss budget
  std::printf("  \"loss_budget\": {\n");
  const char* loss_labels[] = {"loss_baseline", "loss_int8", "loss_int4", "loss_15iter"};
  for (int li = 0; li < 4; ++li) {
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].label == loss_labels[li]) {
        const auto& r = results[i].ber;
        std::printf("    \"%s\": {\"fer\": %.8f, \"ber\": %.10f, \"ci_fer\": %.8f, "
                    "\"frames\": %zu, \"frame_errors\": %zu}%s\n",
                    loss_labels[li], r.fer, r.ber, r.ci_fer,
                    r.total_frames, r.frame_errors,
                    (li < 3) ? "," : "");
      }
    }
  }
  std::printf("  },\n");

  // Symmetry
  std::printf("  \"symmetry\": {\n");
  for (size_t i = 0; i < results.size(); ++i) {
    if (results[i].label == "symmetry_random") {
      std::printf("    \"random\": {\"fer\": %.8f, \"ci_fer\": %.8f, \"frames\": %zu},\n",
                  results[i].ber.fer, results[i].ber.ci_fer, results[i].ber.total_frames);
    }
    if (results[i].label == "symmetry_allzero") {
      std::printf("    \"allzero\": {\"fer\": %.8f, \"ci_fer\": %.8f, \"frames\": %zu}\n",
                  results[i].ber.fer, results[i].ber.ci_fer, results[i].ber.total_frames);
    }
  }
  std::printf("  },\n");

  // Quantisation sweep
  {
    auto idxs = collect("quant_");
    std::printf("  \"quantisation\": [\n");
    for (size_t j = 0; j < idxs.size(); ++j) {
      size_t i = idxs[j];
      const char* qlabel = results[i].label.c_str() + 6; // skip "quant_"
      std::printf("    {\"config\": \"%s\", \"fer\": %.8f, \"ber\": %.10f, "
                  "\"ci_fer\": %.8f, \"frames\": %zu}%s\n",
                  qlabel, results[i].ber.fer, results[i].ber.ber,
                  results[i].ber.ci_fer, results[i].ber.total_frames,
                  (j + 1 < idxs.size()) ? "," : "");
    }
    std::printf("  ],\n");
  }

  // Error patterns (multiple SNR points)
  std::printf("  \"error_patterns\": [\n");
  for (size_t pi = 0; pi < pattern_points.size(); ++pi) {
    const auto& pp = pattern_points[pi];
    const auto& pat = pp.result;
    std::printf("    {\"eb_n0_db\": %.1f, \"total_frames\": %zu, "
                "\"frame_errors\": %zu, \"max_bit_errors\": %zu, "
                "\"k_nominal\": 1200, ",
                pp.eb_n0, pat.total_frames, pat.frame_errors, pat.max_errors);
    // Histogram bins
    size_t bins[5] = {0, 0, 0, 0, 0};
    for (auto e : pat.errors_per_frame) {
      if (e <= 5) {
        ++bins[0];
      } else if (e <= 20) {
        ++bins[1];
      } else if (e <= 100) {
        ++bins[2];
      } else if (e <= 300) {
        ++bins[3];
      } else {
        ++bins[4];
      }
    }
    std::printf("\"histogram\": ["
                "{\"range\": \"1-5\", \"count\": %zu}, "
                "{\"range\": \"6-20\", \"count\": %zu}, "
                "{\"range\": \"21-100\", \"count\": %zu}, "
                "{\"range\": \"101-300\", \"count\": %zu}, "
                "{\"range\": \"301+\", \"count\": %zu}], ",
                bins[0], bins[1], bins[2], bins[3], bins[4]);
    // Raw errors
    std::printf("\"raw_errors\": [");
    for (size_t i = 0; i < pat.errors_per_frame.size(); ++i) {
      if (i > 0) {
        std::printf(", ");
      }
      std::printf("%zu", pat.errors_per_frame[i]);
    }
    std::printf("]}%s\n", (pi + 1 < pattern_points.size()) ? "," : "");
  }
  std::printf("  ],\n");

  // Saturation stress
  std::printf("  \"saturation_stress\": {\n");
  std::printf("    \"low_snr\": {\"eb_n0_db\": 0.1, \"fer\": %.8f, \"ber\": %.10f, "
              "\"frames\": %zu, \"not_converged\": %zu},\n",
              sat_low.fer, sat_low.ber, sat_low.total_frames, sat_low.not_converged);
  std::printf("    \"high_snr\": {\"eb_n0_db\": 10.0, \"fer\": %.8f, \"ber\": %.10f, "
              "\"frames\": %zu, \"not_converged\": %zu}\n",
              sat_high.fer, sat_high.ber, sat_high.total_frames, sat_high.not_converged);
  std::printf("  }\n");

  std::printf("}\n");
  return 0;
}
