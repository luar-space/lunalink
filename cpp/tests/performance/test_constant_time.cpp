// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/signal/modulator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using namespace lunalink::signal;

namespace {

// Constant-time acceptance for the input-independent primitives.
//
// Scope — this gate applies ONLY to functions whose control flow provably does
// not depend on data values:
//
//   * modulate_bpsk_generic — branchless arithmetic mapping, one pass
//   * crc24_calculate       — fixed shift/XOR schedule, one pass
//   * block_interleave      — pure index permutation, one pass
//   * bch_decode (hard)     — exhaustive ML correlation over all 512 codewords
//
// It deliberately does NOT apply to ldpc_decode or frame_decode.  Those
// early-terminate on a satisfied syndrome (ldpc.cpp: the `!result.converged`
// loop guard and the in-loop `check_syndrome` break), so their runtime is
// data-dependent BY DESIGN — an all-zeros payload converges in fewer
// iterations than a pseudo-random one.  A spread gate there would forbid a
// correct optimisation.  Their worst case is bounded instead by the forced
// full-iteration budgets in test_wcet.cpp ("LDPC decode SF2 (50 iter)
// < 4 ms"), which is the number that matters for WCET.
//
// Tolerance derivation — measured, not asserted.  Across 8 fresh Release-build
// process runs on a 2024-vintage M-series host, the worst class-to-class spread
// over all four primitives was 6.7% (modulate_bpsk_i 4.1%, crc24_calculate
// 5.4%, block_interleave 2.6%, bch_decode 6.7%), and no class was consistently
// slowest — the residue is timer and scheduler noise, not data-dependence.  The
// gate is set at 2x the worst observed value for runner-to-runner headroom.
//
// This is a real ceiling, not an aspiration: an actual data-dependent branch in
// any function below moves the spread by an order of magnitude.  During
// development, an input-generation step accidentally left inside the timed
// region produced spreads of 107-155% here, which this gate rejected.
constexpr double kSpreadTolerancePct = 14.0;

// Iterations per class.  High enough that per-call timer resolution and
// scheduler noise average out; the spread, not the absolute time, is the
// measurand.
constexpr size_t kIters = 2000;

// Repetitions per class, of which the MINIMUM is taken.  Scheduler preemption,
// frequency scaling and cache eviction can only ever make a sample slower, so
// the minimum is the estimator least contaminated by them — a single mean lets
// one descheduled run masquerade as data-dependence.
constexpr size_t kReps = 5;

enum class InputClass : uint8_t {
  kZeros,
  kOnes,
  kPseudoRandom
};

constexpr std::array<InputClass, 3> kClasses = {InputClass::kZeros,
                                                InputClass::kOnes,
                                                InputClass::kPseudoRandom};

const char* class_name(InputClass c) noexcept {
  switch (c) {
  case InputClass::kZeros:
    return "all-zeros";
  case InputClass::kOnes:
    return "all-ones";
  case InputClass::kPseudoRandom:
    break;
  }
  return "pseudo-random";
}

// Fill `out` with 0/1 bit values for the given adversarial class.  The
// pseudo-random class uses the same LCG constants as bench_util so the
// pattern is reproducible without pulling in <random>.
void fill_class(std::span<uint8_t> out, InputClass c) noexcept {
  uint32_t state = 0xC0FFEEU;
  for (size_t i = 0; i < out.size(); ++i) {
    switch (c) {
    case InputClass::kZeros:
      out[i] = 0U;
      break;
    case InputClass::kOnes:
      out[i] = 1U;
      break;
    case InputClass::kPseudoRandom:
      state = state * 1664525U + 1013904223U;
      out[i] = static_cast<uint8_t>((state >> 16U) & 1U);
      break;
    }
  }
}

template <typename F> double measure_avg_ns(F&& f, size_t iters) {
  using clock = std::chrono::steady_clock;
  // Warm caches and branch predictors so the first class is not penalised.
  for (size_t i = 0; i < iters / 10U + 1U; ++i) {
    (void)f();
  }
  const auto start = clock::now();
  for (size_t i = 0; i < iters; ++i) {
    (void)f();
  }
  const auto end = clock::now();
  return std::chrono::duration<double, std::nano>(end - start).count() / static_cast<double>(iters);
}

// Run `run` once per input class and assert the spread is within tolerance.
//
// `prepare(c)` populates the input buffer for class `c` and is called OUTSIDE
// the timed region; `run()` executes only the primitive under test and returns
// a value that cannot be optimised away.  Keeping preparation out of the
// measurement matters: filling the pseudo-random class costs a serial LCG step
// per element, which on a 5880-byte buffer is comparable to the primitive
// itself and would masquerade as data-dependence in the primitive.
template <typename P, typename F>
void check_constant_time(const std::string& name, P&& prepare, F&& run) {
#ifndef NDEBUG
  // Release only.  Unoptimised builds run 3-14x slower and the spread is then
  // dominated by loop and bounds-check overhead rather than by the data path,
  // so the gate would measure the build rather than the primitive — the same
  // reason the `constant-time` task configures Release.  `task
  // test-acceptance` and acceptance.yml run the whole binary in Debug, so skip
  // there instead of failing; `task constant-time` is the gate that counts.
  SKIP(name + ": timing gate is Release-only; run `task constant-time`");
#endif
  std::array<double, kClasses.size()> means{};

  // Priming pass, discarded.  Without it the first class measured absorbs the
  // CPU frequency ramp and appears slower than the rest — an artefact that
  // moves between classes run to run, so it is noise, not data-dependence.
  prepare(kClasses.at(0));
  (void)measure_avg_ns(run, kIters);

  for (size_t i = 0; i < kClasses.size(); ++i) {
    const InputClass c = kClasses.at(i);
    prepare(c);
    double best = measure_avg_ns(run, kIters);
    for (size_t r = 1; r < kReps; ++r) {
      best = std::min(best, measure_avg_ns(run, kIters));
    }
    means.at(i) = best;
  }

  const double lo = *std::min_element(means.begin(), means.end());
  const double hi = *std::max_element(means.begin(), means.end());
  REQUIRE(lo > 0.0);
  const double spread_pct = (hi - lo) / lo * 100.0;

  // Reported unconditionally, and deliberately not via UNSCOPED_INFO: Catch2
  // buffers INFO and flushes it only when an assertion fails, so the spread --
  // the measurand this gate exists to bound -- used to be recorded on exactly
  // the runs where it was already too late.  A passing run threw its number
  // away, which left no way to tell a one-off scheduler artefact from drift
  // toward a data-dependent path.  This test also carries no BENCHMARK macros
  // (it needs the min-of-kReps estimator and preparation outside the timed
  // region), so it emits no <BenchmarkResults> and performance_report.py
  // cannot see it either; stdout is the only channel it has.
  //
  // Note where this lands.  Catch2's console reporter, redirected with
  // `console::out=`, does NOT capture std::cout, so these lines are absent
  // from wcet_console.txt; the XML reporter captures them per test case into
  // <TestCase><StdOut>, so wcet_results.xml is the file that archives them
  // (both are uploaded as the wcet-raw-output artifact).  They also appear on
  // the step's own stdout, which is the quickest place to read them but
  // expires with the run log.
  //
  // One line per primitive, fixed prefix and field order, so grepping
  // "[constant-time]" over the archived XML reconstructs a history, and a
  // future step can parse it into bench.json without the format changing
  // again.
  std::cout << "[constant-time] " << name << " spread " << std::fixed << std::setprecision(2)
            << spread_pct << "% tolerance " << kSpreadTolerancePct << "%";
  for (size_t i = 0; i < kClasses.size(); ++i) {
    std::cout << " | " << class_name(kClasses.at(i)) << " " << means.at(i) << " ns";
  }
  std::cout << std::endl;

  CHECK(spread_pct < kSpreadTolerancePct);
}

} // namespace

TEST_CASE("Constant-time: modulate_bpsk is input-independent", "[slow][wcet][constant-time]") {
  std::array<uint8_t, kGoldChipLength> chips{};
  std::array<int8_t, kGoldChipLength> out{};

  check_constant_time(
      "modulate_bpsk_i",
      [&](InputClass c) { fill_class(chips, c); },
      [&]() { return modulate_bpsk_i(chips, 1, out) == ModulationStatus::kOk; });
}

TEST_CASE("Constant-time: crc24_calculate is input-independent", "[slow][wcet][constant-time]") {
  std::array<uint8_t, 1176> data{};
  std::array<uint8_t, 24> crc{};

  check_constant_time(
      "crc24_calculate",
      [&](InputClass c) { fill_class(data, c); },
      [&]() { return crc24_calculate(data, crc) == CrcStatus::kOk; });
}

TEST_CASE("Constant-time: block_interleave is input-independent", "[slow][wcet][constant-time]") {
  std::array<uint8_t, kInterleaverSize> in{};
  std::array<uint8_t, kInterleaverSize> out{};

  check_constant_time(
      "block_interleave",
      [&](InputClass c) { fill_class(in, c); },
      [&]() { return block_interleave(in, out) == InterleaverStatus::kOk; });
}

TEST_CASE("Constant-time: bch_decode ML correlation is input-independent",
          "[slow][wcet][constant-time]") {
  std::array<uint8_t, kBchCodewordLength> word{};

  check_constant_time(
      "bch_decode",
      [&](InputClass c) { fill_class(word, c); },
      [&]() { return bch_decode(std::span<const uint8_t, kBchCodewordLength>(word)).status; });
}
