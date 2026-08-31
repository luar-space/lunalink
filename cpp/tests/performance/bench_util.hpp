#pragma once

// Shared helpers for performance benchmarks.  Keep this header dependency-light
// (no Catch2, no project headers): it is included from both
// ``cpp/tests/performance/`` (open-source bench files) and
// ``cpp/proprietary/tests/signal/`` (proprietary bench files), and pulling
// project-specific includes here would couple the two trees.
//
// Schema-specific CSV row writers live in their respective bench files —
// the fft_bench schema (size × backend × mode × ns/fft) and the
// acquisition_bench schema (signal × fft_size × variant × µs/acquire) are
// not interchangeable; abstracting them would add complexity without
// reducing duplication.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <string>

namespace lunalink::bench_util {

/// Tiny LCG + Box-Muller RNG.  Avoids ``<random>`` so the bench has no
/// dependency on the standard library's distribution machinery (which
/// also bloats compile time when instantiated repeatedly).  Constants
/// are Numerical Recipes' classic LCG.
class Lcg {
public:
  /// The Numerical Recipes LCG (multiplier 1664525, additive 1013904223,
  /// implicit modulus 2^32) reaches every state in [0, 2^32) once per
  /// period — odd-seed coercion is unnecessary for this constant set, so
  /// the seed is taken verbatim.  A zero seed is permitted; the first
  /// ``uniform()`` call advances away from it before the value is
  /// observed.
  explicit Lcg(std::uint32_t seed) noexcept : state_(seed) {}

  /// Uniform on [0, 1) with 24-bit precision.
  float uniform() noexcept {
    state_ = state_ * 1664525U + 1013904223U;
    return static_cast<float>(state_ & 0xFFFFFFU) / static_cast<float>(0x1000000U);
  }

  /// One AWGN sample (real, imag) ~ N(0, 1) via Box-Muller.
  /// ``u1`` is clamped at ``1e-9F`` to keep ``log(u1)`` finite when the
  /// LCG produces zero or near-zero output; the resulting tail caps the
  /// largest emitted sample at roughly ±6.4σ in single precision —
  /// adequate for bench fills, where exact tail fidelity is not the
  /// point.
  std::complex<float> awgn() noexcept {
    const float u1 = std::max(uniform(), 1e-9F);
    const float u2 = uniform();
    const float r = std::sqrt(-2.0F * std::log(u1));
    const float th = 2.0F * std::numbers::pi_v<float> * u2;
    return {r * std::cos(th), r * std::sin(th)};
  }

private:
  std::uint32_t state_;
};

/// Truncate-then-write the header line on the first call per process,
/// then close.  Subsequent calls in the same process are no-ops.
/// Callers append data rows themselves with ``std::ofstream(... ios::app)``.
inline void
write_csv_header_once(const std::string& path, const char* header, bool& header_written) {
  if (header_written) {
    return;
  }
  std::ofstream trunc(path, std::ios::trunc);
  trunc << header << "\n";
  header_written = true;
}

} // namespace lunalink::bench_util
