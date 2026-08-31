// FFT microbenchmark — A1.V2 of the FFT acceleration plan.
//
// See docs/shaping/cislunar_nav/fft-acceleration.md.
//
// Tagged [fft_bench] so it doesn't run with the default test suite (Debug)
// and only fires under `task bench-fft` (Release). Emits CSV rows for the
// (size × backend × mode) grid into a file named by the
// LUNALINK_BENCH_OUTPUT_CSV env var (default benchmarks/latest/fft_bench.csv).
//
// Each row is one independent measurement; the harness writes header on
// first row and appends thereafter.

#include "lunalink/signal/fft.hpp"

#include "performance/bench_util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace ls = lunalink::signal;
using ls::Fft;

namespace {

// ─── Configuration ──────────────────────────────────────────────────────────

constexpr size_t kWarmupIters = 32;
constexpr size_t kMeasureIters = 256;
constexpr size_t kSamples = 5;      // independent timing samples for stdev
constexpr size_t kThreadCount = 10; // matches campaign ThreadPoolExecutor

const char* variant_id() {
  const char* v = std::getenv("LUNALINK_BENCH_VARIANT");
  return (v && *v) ? v : "baseline";
}

const char* output_csv_path() {
  const char* p = std::getenv("LUNALINK_BENCH_OUTPUT_CSV");
  return (p && *p) ? p : "benchmarks/latest/fft_bench.csv";
}

// ─── CSV writer (header on first call per process) ──────────────────────────
// Schema is fft_bench-specific; the row writer stays local.  Header
// truncate-and-write uses the shared ``write_csv_header_once`` helper.

class CsvWriter {
public:
  explicit CsvWriter(const std::string& path) : path_(path) {}

  void write_row(uint32_t size,
                 const char* backend,
                 const char* mode,
                 const char* variant,
                 double ns_per_fft,
                 double stdev_ns,
                 double gflops) {
    lunalink::bench_util::write_csv_header_once(
        path_, "size,backend,mode,variant,ns_per_fft,stdev_ns,gflops", header_written_);
    std::ofstream f(path_, std::ios::app);
    f << size << "," << backend << "," << mode << "," << variant << "," << ns_per_fft << ","
      << stdev_ns << "," << gflops << "\n";
  }

private:
  std::string path_;
  bool header_written_ = false;
};

// ─── Timing primitives ──────────────────────────────────────────────────────

template <uint32_t N>
void fill_deterministic(std::span<std::complex<float>, N> buf, uint32_t seed) {
  // Centred-uniform fill via the shared LCG.  Values map [0,1) → [-1,1).
  lunalink::bench_util::Lcg rng(seed);
  for (uint32_t i = 0; i < N; ++i) {
    const float re = 2.0F * rng.uniform() - 1.0F;
    const float im = 2.0F * rng.uniform() - 1.0F;
    buf[i] = {re, im};
  }
}

template <uint32_t N> double measure_single_ns(size_t iters) {
  Fft<N> fft;
  std::vector<std::complex<float>> buf(N);
  std::span<std::complex<float>, N> view(buf.data(), N);
  fill_deterministic<N>(view, 0xDEADBEEFU);

  // Warmup
  for (size_t i = 0; i < kWarmupIters; ++i) {
    fft.forward(view);
  }

  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  for (size_t i = 0; i < iters; ++i) {
    fft.forward(view);
  }
  const auto dur = clock::now() - start;
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
  return static_cast<double>(ns) / static_cast<double>(iters);
}


template <uint32_t N> double measure_threaded_ns(size_t iters, size_t n_threads) {
  // Each thread owns its own Fft + buffer to mirror the campaign's
  // thread-local correlator state.
  std::atomic<bool> go{false};
  std::vector<std::thread> threads;
  threads.reserve(n_threads);

  using clock = std::chrono::steady_clock;
  std::atomic<int64_t> total_ns{0};

  for (size_t t = 0; t < n_threads; ++t) {
    threads.emplace_back([t, iters, &go, &total_ns] {
      Fft<N> fft;
      std::vector<std::complex<float>> buf(N);
      std::span<std::complex<float>, N> view(buf.data(), N);
      fill_deterministic<N>(view, 0xDEADBEEFU + static_cast<uint32_t>(t));

      // Warmup
      for (size_t i = 0; i < kWarmupIters; ++i) {
        fft.forward(view);
      }

      // Spin-wait for the start signal so all threads time the same window.
      while (!go.load(std::memory_order_acquire)) {}

      const auto start = clock::now();
      for (size_t i = 0; i < iters; ++i) {
        fft.forward(view);
      }
      const auto dur = clock::now() - start;
      total_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count(),
                         std::memory_order_relaxed);
    });
  }

  go.store(true, std::memory_order_release);
  for (auto& th : threads) {
    th.join();
  }

  // Mean per-thread time — represents per-FFT cost under contention.
  return static_cast<double>(total_ns.load()) / static_cast<double>(iters * n_threads);
}

// ─── Sampler with stdev ─────────────────────────────────────────────────────

struct Sample {
  double mean_ns;
  double stdev_ns;
};

template <typename F> Sample sample_with_stdev(F&& fn) {
  std::vector<double> samples;
  samples.reserve(kSamples);
  for (size_t i = 0; i < kSamples; ++i) {
    samples.push_back(fn());
  }
  double sum = 0.0;
  for (double s : samples)
    sum += s;
  const double mean = sum / static_cast<double>(kSamples);
  double var_sum = 0.0;
  for (double s : samples)
    var_sum += (s - mean) * (s - mean);
  const double stdev = std::sqrt(var_sum / static_cast<double>(kSamples));
  return {mean, stdev};
}

double gflops_for(uint32_t n, double ns_per_fft) {
  // 5N log2(N) flops is the standard radix-2 FFT count.
  const double log2n = std::log2(static_cast<double>(n));
  const double flops = 5.0 * static_cast<double>(n) * log2n;
  const double seconds = ns_per_fft * 1e-9;
  return (flops / seconds) * 1e-9;
}

template <uint32_t N> void bench_size(CsvWriter& csv) {
  // Apple builds use vDSP_DFT_Execute since A2; non-Apple uses the scalar
  // fallback.  Backend label is informational only.
#ifdef __APPLE__
  const char* backend = "vdsp_dft";
#else
  const char* backend = "scalar";
#endif

  const auto single = sample_with_stdev([] { return measure_single_ns<N>(kMeasureIters); });
  csv.write_row(N,
                backend,
                "single",
                variant_id(),
                single.mean_ns,
                single.stdev_ns,
                gflops_for(N, single.mean_ns));

  const auto threaded =
      sample_with_stdev([] { return measure_threaded_ns<N>(kMeasureIters, kThreadCount); });
  csv.write_row(N,
                backend,
                "thr10",
                variant_id(),
                threaded.mean_ns,
                threaded.stdev_ns,
                gflops_for(N, threaded.mean_ns));

  std::printf("  %5u  single=%9.0f ns/fft (±%5.0f, %4.1f GFLOPS)  "
              "thr10=%9.0f ns/fft (±%5.0f, %4.1f GFLOPS)\n",
              N,
              single.mean_ns,
              single.stdev_ns,
              gflops_for(N, single.mean_ns),
              threaded.mean_ns,
              threaded.stdev_ns,
              gflops_for(N, threaded.mean_ns));
}

} // namespace

// One-shot correctness sanity check before any timing — catches a
// catastrophically broken FFT silently masquerading as "10× faster".
// Forward FFT of an impulse should be all-ones in magnitude (DFT of
// δ[n] = 1).  Inverse FFT of all-ones should reconstruct the impulse.
template <uint32_t N> void verify_impulse_response() {
  Fft<N> fft;
  std::vector<std::complex<float>> buf(N);
  std::span<std::complex<float>, N> view(buf.data(), N);
  std::fill(buf.begin(), buf.end(), std::complex<float>{0.0F, 0.0F});
  buf[0] = {1.0F, 0.0F};
  fft.forward(view);
  for (uint32_t i = 0; i < N; ++i) {
    REQUIRE(std::abs(std::abs(buf[i]) - 1.0F) < 1e-3F);
  }
  fft.inverse(view);
  REQUIRE(std::abs(buf[0] - std::complex<float>{1.0F, 0.0F}) < 1e-3F);
  for (uint32_t i = 1; i < N; ++i) {
    REQUIRE(std::abs(buf[i]) < 1e-3F);
  }
}

// Tagged ``[slow]`` so the default ``~[slow]`` filter used by ``task
// test-cpp`` and ``task sanitize`` excludes it — sanitiser-instrumented
// timings are bogus, and the bench's own ``task bench-fft`` invokes it
// explicitly via the ``[fft_bench]`` filter regardless of ``[slow]``.
// Same convention as ``[wcet][slow]`` and ``[ber][slow]``.
TEST_CASE("FFT microbench: size × backend × mode grid", "[fft_bench][slow]") {
  // Sanity-check at every size we then time.  test_fft_sizes.cpp
  // already covers correctness for the default Tier-2 build, but if
  // a future slice introduces per-size dispatch (e.g. an A5 GPU
  // backend above N=8192), a broken backend at N=16384 would escape
  // a single-size spot-check.  Six impulse-checks total cost <1 ms.
  verify_impulse_response<2048>();
  verify_impulse_response<4096>();
  verify_impulse_response<8192>();
  verify_impulse_response<16384>();
  verify_impulse_response<32768>();
  verify_impulse_response<65536>();

  CsvWriter csv(output_csv_path());
  std::printf("FFT microbench (variant=%s, output=%s)\n", variant_id(), output_csv_path());
  std::printf("  size  single                                 thr10 (10 concurrent threads)\n");

  bench_size<2048>(csv);
  bench_size<4096>(csv);
  bench_size<8192>(csv);
  bench_size<16384>(csv);
  bench_size<32768>(csv);
  bench_size<65536>(csv);

  // No assertion — bench is for measurement, not pass/fail.
  // The regression gate lives at scripts/bench_regression_check.py (A1.V4).
  SUCCEED("Bench rows written to " << output_csv_path());
}
