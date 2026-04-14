#include "lunalink/core/telemetry.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <numeric>
#include <random>
#include <vector>

using namespace lunalink::signal;

namespace {

// G-10: wall-clock budget asserts for absolute WCET limits.
//
// The existing BENCHMARK tests in this file feed `scripts/performance_report.py`
// and the gh-pages benchmark history, which raises an alert at 120% of the
// prior nightly value — so *relative* regressions are already detected. What
// was missing before G-10: an *absolute* wall-clock ceiling that fails a
// build when a stage exceeds its spec budget, regardless of history.
//
// The TEST_CASEs below are tagged `[wcet][wcet-budget][slow]`:
//   - `[slow]` excludes them from `task test-cpp` (Debug build), so the
//     budgets — calibrated against Release builds — never run in Debug
//     where timings are 3-14× slower and would flake. (sync_detect, for
//     example, runs in 64 µs Release vs 899 µs Debug on the same host.)
//   - `[wcet]` is included by the nightly `wcet.yml` workflow, which
//     builds Release (`-DCMAKE_BUILD_TYPE=Release`) and already has the
//     perf regression machinery wired up. Adding these tests there gives
//     us the absolute-limit gate without a new CI workflow.
//   - `[wcet-budget]` lets a human run just the budgets with
//     `test_lunalink "[wcet-budget]"` to check their own changes before
//     pushing.
//
// Budgets are ~10× the observed mean on a 2024-vintage M-series MacBook
// Release build. If a budget flakes on a specific CI runner, bump the
// number in the test — do NOT loosen the iteration count below ~10,
// because averaging is what keeps the measurement stable.
template <typename F>
double measure_avg_us(F&& f, size_t iters) {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  for (size_t i = 0; i < iters; ++i) {
    (void)f();
  }
  const auto end = clock::now();
  const auto total_us =
      std::chrono::duration<double, std::micro>(end - start).count();
  return total_us / static_cast<double>(iters);
}

} // namespace

namespace {
// Helper: encode a test pattern and produce soft symbols for benchmarking.
void prepare_soft_symbols(LdpcSubframe type, std::span<float> soft, float snr_scale = 1.0f) {
  const bool is_sf2 = (type == LdpcSubframe::kSF2);
  const size_t k = is_sf2 ? 1200U : 870U;
  const size_t n = is_sf2 ? 2400U : 1740U;

  std::vector<uint8_t> msg(k);
  std::iota(msg.begin(), msg.end(), uint8_t{0});
  for (auto& b : msg)
    b %= 2;

  std::vector<uint8_t> encoded(n);
  LdpcEncodeWorkspace enc_ws{};
  (void)ldpc_encode(type, msg, encoded, enc_ws);

  for (size_t i = 0; i < n && i < soft.size(); ++i)
    soft[i] = (encoded[i] == 1 ? -snr_scale : snr_scale);
}
} // namespace

// ============================================================================
// G-10 — PR-time wall-clock budget asserts
// ============================================================================
//
// Budgets are ~10× the observed mean on a 2024-vintage M-series MacBook.
// Rationale: the tests must not flake on slower CI runners (ubuntu-latest
// ~2-4× slower than M-series) while still catching a 3× regression on
// current hardware. The observed means are in test comments so future
// auditors can see the drift if CI hardware changes.

TEST_CASE("WCET budget: BCH ML decode < 3 µs", "[wcet][wcet-budget][slow]") {
  // Observed: 238 ns (M-series Release build). Budget 3 µs = 12×.
  std::array<uint8_t, 52> bch_in{};
  const double avg_us = measure_avg_us([&]() { return bch_decode(bch_in); }, 1000);
  INFO("BCH ML decode avg: " << avg_us << " us");
  CHECK(avg_us < 3.0);
}

TEST_CASE("WCET budget: sync_detect (6000 symbols) < 700 µs", "[wcet][wcet-budget][slow]") {
  // Observed: 64 µs (M-series Release). Budget 700 µs ≈ 11×.
  NullTelemetryHal hal;
  SyncDetector det(hal);

  std::array<float, 6000> symbols{};
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    symbols[i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;  // NOLINT
  }

  SyncResult result{};
  const double avg_us = measure_avg_us(
      [&]() { return det.detect(std::span<const float>(symbols), result); }, 50);
  INFO("sync_detect (6000 symbols) avg: " << avg_us << " us");
  CHECK(avg_us < 700.0);
}

TEST_CASE("WCET budget: LDPC decode SF2 (50 iter) < 4 ms", "[wcet][wcet-budget][slow]") {
  // Observed: 386 µs (M-series Release, clean signal, NMS). Budget 4 ms ≈ 10×.
  std::array<uint8_t, 1200> msg_sf2{};
  std::array<uint8_t, 2400> encoded_sf2{};
  std::array<float, 2400> soft_sf2{};
  std::array<uint8_t, 1200> decoded_sf2{};
  LdpcEncodeWorkspace enc_ws{};
  (void)ldpc_encode(LdpcSubframe::kSF2, msg_sf2, encoded_sf2, enc_ws);
  for (size_t i = 0; i < 2400; ++i) {
    soft_sf2[i] = (encoded_sf2[i] == 1U ? -1.0F : 1.0F);  // NOLINT
  }

  LdpcWorkspace ws{};
  const double avg_us = measure_avg_us(
      [&]() { return ldpc_decode(LdpcSubframe::kSF2, soft_sf2, 50, decoded_sf2, ws); }, 20);
  INFO("LDPC decode SF2 (50 iter) avg: " << (avg_us / 1000.0) << " ms");
  CHECK(avg_us < 4000.0);
}

TEST_CASE("WCET budget: LDPC decode SF3 (50 iter) < 5 ms", "[wcet][wcet-budget][slow]") {
  // Observed: 497 µs (M-series Release, clean signal). Budget 5 ms ≈ 10×.
  std::array<uint8_t, 870> msg_sf3{};
  std::array<uint8_t, 1740> encoded_sf3{};
  std::array<float, 1740> soft_sf3{};
  std::array<uint8_t, 870> decoded_sf3{};
  LdpcEncodeWorkspace enc_ws{};
  (void)ldpc_encode(LdpcSubframe::kSF3, msg_sf3, encoded_sf3, enc_ws);
  for (size_t i = 0; i < 1740; ++i) {
    soft_sf3[i] = (encoded_sf3[i] == 1U ? -1.0F : 1.0F);  // NOLINT
  }

  LdpcWorkspace ws{};
  const double avg_us = measure_avg_us(
      [&]() { return ldpc_decode(LdpcSubframe::kSF3, soft_sf3, 50, decoded_sf3, ws); }, 20);
  INFO("LDPC decode SF3 (50 iter) avg: " << (avg_us / 1000.0) << " ms");
  CHECK(avg_us < 5000.0);
}

TEST_CASE("WCET budget: frame_decode (pseudo-random payload) < 20 ms", "[wcet][wcet-budget][slow]") {
  // Observed: 1.75 ms (M-series Release, pseudo-random payload). Budget 20 ms ≈ 11×.
  // This is the end-to-end wall-clock that matters for the 1 s frame cadence:
  // 20 ms gives 50× real-time headroom on the worst-case class.
  std::mt19937 rng(42);  // NOLINT(cert-msc51-cpp) — deterministic seed
  auto dist = std::uniform_int_distribution<uint32_t>(0, 1);
  std::array<uint8_t, kSb2DataBits> sb2{};
  std::array<uint8_t, kSb3DataBits> sb3{};
  std::array<uint8_t, kSb4DataBits> sb4{};
  for (auto& b : sb2) {
    b = static_cast<uint8_t>(dist(rng));
  }
  for (auto& b : sb3) {
    b = static_cast<uint8_t>(dist(rng));
  }
  for (auto& b : sb4) {
    b = static_cast<uint8_t>(dist(rng));
  }

  std::array<uint8_t, kFrameLength> tx{};
  LdpcEncodeWorkspace enc_ws{};
  FrameBuildWorkspace fbw{};
  (void)frame_build(Fid{0}, Toi{42}, sb2, sb3, sb4, tx, enc_ws, fbw);

  std::array<float, kFrameLength> rx{};
  for (size_t i = 0; i < kFrameLength; ++i) {
    rx[i] = (tx[i] == 0U) ? 1.0F : -1.0F;  // NOLINT
  }

  NullTelemetryHal hal;
  auto workspace = std::make_unique<FrameDecodeWorkspace>();
  FrameDecodeResult result{};

  const double avg_us = measure_avg_us(
      [&]() { return frame_decode(rx, result, *workspace, hal); }, 10);
  INFO("frame_decode (pseudo-random) avg: " << (avg_us / 1000.0) << " ms");
  CHECK(avg_us < 20000.0);
}

// ============================================================================
// Existing BENCHMARK-based profile tests (feed the nightly WCET report and
// the gh-pages historical regression alert at 120%)
// ============================================================================

TEST_CASE("Gateway 2: WCET Analysis", "[wcet][slow]") {
  std::array<uint8_t, 1200> msg_sf2{};
  std::array<uint8_t, 2400> encoded_sf2{};
  std::array<float, 2400> soft_sf2{};
  std::array<uint8_t, 1200> decoded_sf2{};

  LdpcEncodeWorkspace enc_ws{};

  BENCHMARK("LDPC encode SF2") {
    return ldpc_encode(LdpcSubframe::kSF2, msg_sf2, encoded_sf2, enc_ws);
  };

  soft_sf2.fill(1.0f);
  LdpcWorkspace ws{};

  BENCHMARK("LDPC decode SF2 (50 iter)") {
    return ldpc_decode(LdpcSubframe::kSF2, soft_sf2, 50, decoded_sf2, ws);
  };

  std::array<uint8_t, 52> bch_in{};

  BENCHMARK("BCH ML decode") {
    return bch_decode(bch_in);
  };
}

TEST_CASE("Gateway 5: Sync Detector WCET", "[wcet][slow]") {
  NullTelemetryHal hal;
  SyncDetector det(hal);

  // Build a 6000-symbol stream with sync at offset 0.
  std::array<float, 6000> symbols{};
  for (uint16_t i = 0; i < kSyncLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    symbols[i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;
  }

  SyncResult result{};

  BENCHMARK("sync_detect (6000 symbols)") {
    return det.detect(std::span<const float>(symbols), result);
  };
}

TEST_CASE("Gateway 2: LDPC Decode Throughput", "[wcet][slow]") {
  SECTION("SF2 — alpha sweep throughput") {
    std::array<float, 2400> soft{};
    prepare_soft_symbols(LdpcSubframe::kSF2, soft);

    {
      std::array<uint8_t, 1200> decoded{};
      LdpcWorkspace ws{};

      BENCHMARK("LDPC decode SF2 sum-product") {
        return ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws);
      };
    }
  }

  SECTION("SF3 — baseline throughput") {
    std::array<float, 1740> soft{};
    prepare_soft_symbols(LdpcSubframe::kSF3, soft);

    std::array<uint8_t, 870> decoded{};
    LdpcWorkspace ws{};

    BENCHMARK("LDPC decode SF3 alpha=0.75") {
      return ldpc_decode(LdpcSubframe::kSF3, soft, 50, decoded, ws);
    };
  }
}

TEST_CASE("Gateway 5: frame_decode WCET", "[wcet][slow]") {
  // Multi-class WCET benchmark per MA §2: all-zeros, all-ones, pseudo-random.
  // Timing spread across input classes detects data-dependent branches.

  auto build_rx = [](std::span<uint8_t, kSb2DataBits> sb2,
                     std::span<uint8_t, kSb3DataBits> sb3,
                     std::span<uint8_t, kSb4DataBits> sb4,
                     std::span<float> out) {
    std::array<uint8_t, kFrameLength> tx{};
    LdpcEncodeWorkspace enc_ws{};
    FrameBuildWorkspace fbw{};
    (void)frame_build(Fid{0}, Toi{42}, sb2, sb3, sb4, tx, enc_ws, fbw);
    for (size_t i = 0; i < kFrameLength; ++i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      out[i] = (tx[i] == 0U) ? 1.0F : -1.0F;
    }
  };

  NullTelemetryHal hal;
  auto workspace = std::make_unique<FrameDecodeWorkspace>();
  FrameDecodeResult result{};

  // Class 1: all-zeros nav data
  {
    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    std::array<float, kFrameLength> rx{};
    build_rx(sb2, sb3, sb4, rx);
    BENCHMARK("frame_decode (all-zeros)") {
      return frame_decode(rx, result, *workspace, hal);
    };
  }

  // Class 2: all-ones nav data
  {
    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    for (auto& b : sb2) {
      b = 1;
    }
    for (auto& b : sb3) {
      b = 1;
    }
    for (auto& b : sb4) {
      b = 1;
    }
    std::array<float, kFrameLength> rx{};
    build_rx(sb2, sb3, sb4, rx);
    BENCHMARK("frame_decode (all-ones)") {
      return frame_decode(rx, result, *workspace, hal);
    };
  }

  // Class 3: pseudo-random nav data
  {
    std::mt19937 rng(42); // NOLINT(cert-msc51-cpp) deterministic seed for reproducible benchmark
    auto dist = std::uniform_int_distribution<uint32_t>(0, 1);
    std::array<uint8_t, kSb2DataBits> sb2{};
    std::array<uint8_t, kSb3DataBits> sb3{};
    std::array<uint8_t, kSb4DataBits> sb4{};
    for (auto& b : sb2) {
      b = static_cast<uint8_t>(dist(rng));
    }
    for (auto& b : sb3) {
      b = static_cast<uint8_t>(dist(rng));
    }
    for (auto& b : sb4) {
      b = static_cast<uint8_t>(dist(rng));
    }
    std::array<float, kFrameLength> rx{};
    build_rx(sb2, sb3, sb4, rx);
    BENCHMARK("frame_decode (pseudo-random)") {
      return frame_decode(rx, result, *workspace, hal);
    };
  }
}
