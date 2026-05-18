#pragma once
#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <span>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <cmath>
#include <numbers>
#include <utility>
#endif

namespace lunalink::signal {

/// Optimized radix-2 Cooley-Tukey DIT FFT/IFFT for fixed size N.
/// On Apple platforms, delegates to the vDSP Accelerate framework for
/// hardware-tuned FFT (AMX/NEON).  On other platforms, uses a scalar
/// radix-2 Cooley-Tukey DIT implementation with pre-computed twiddle
/// factors and bit-reverse permutation table.
/// Inverse uses 1/N scaling in both backends.
/// No heap allocation. Designed for PCPS acquisition on AFS-I (N=2048).
template <uint32_t N> class Fft {
  static_assert((N & (N - 1U)) == 0 && N >= 2, "N must be a power of 2");

  static constexpr uint32_t kLog2N = []() {
    uint32_t v = N, r = 0;
    while (v >>= 1U) {
      ++r;
    }
    return r;
  }();

public:
  using Complex = std::complex<float>;

#ifdef __APPLE__
  // ─── vDSP (Apple Accelerate) backend ────────────────────────────────────────
  //
  // Uses the modern ``vDSP_DFT_Execute`` path with split-complex IO.  Our
  // std::complex<float> is interleaved, so we deinterleave on entry and
  // re-interleave on exit via ``vDSP_ctoz`` / ``vDSP_ztoc`` — both are
  // NEON-vectorised memory-bandwidth-limited and effectively free relative
  // to the FFT itself.
  //
  // A2 measurement (M4, fft_bench): DFT_Execute is 2.4–3.0× faster than
  // the legacy ``vDSP_fft_zip`` across N ∈ {2048..65536}, despite the
  // deinterleave round-trip.

  Fft() noexcept = default;

  /// In-place forward FFT.  Returns false (and zeroes the buffer) if the
  /// vDSP setup could not be created — see ``setup_fwd()``.  This null-
  /// setup safe-zero is the detection step for FMEA entry FM-082b
  /// (vDSP backend runtime failure, Tier 2/3 only — Tier 1 uses the
  /// scalar branch and is structurally immune).
  ///
  /// Thread-safety: Fft<N> is **not** safe to share across threads.  The
  /// ``mutable`` split-complex scratch (``realp_`` / ``imagp_``) is rewritten
  /// on every call.  Each owning thread must hold its own instance — this
  /// is the convention used by ``Correlator`` (one Fft<N> per Correlator,
  /// one Correlator per worker thread) and the bench harness.
  void forward(std::span<Complex, N> buf) const noexcept {
    const auto setup = setup_fwd();
    if (setup == nullptr) [[unlikely]] {
      // vDSP_DFT_zop_CreateSetup returned null — safe-zero the buffer rather
      // than execute on a null setup (UB).  Caller observes an all-zero
      // spectrum, which propagates to a missed acquisition / scrubbed
      // accumulator downstream.  The non-Apple scalar fallback has no
      // analogous failure mode.
      std::fill(buf.begin(), buf.end(), Complex{0.0F, 0.0F});
      return;
    }
    auto* re = realp_.data();
    auto* im = imagp_.data();
    DSPSplitComplex split{re, im};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(buf.data()), 2, &split, 1, N);
    vDSP_DFT_Execute(setup, re, im, re, im);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    vDSP_ztoc(&split, 1, reinterpret_cast<DSPComplex*>(buf.data()), 2, N);
  }

  /// In-place inverse FFT with 1/N scaling.  Same null-setup safe-zero
  /// (FM-082b) and thread-safety as ``forward``.
  void inverse(std::span<Complex, N> buf) const noexcept {
    const auto setup = setup_inv();
    if (setup == nullptr) [[unlikely]] {
      std::fill(buf.begin(), buf.end(), Complex{0.0F, 0.0F});
      return;
    }
    auto* re = realp_.data();
    auto* im = imagp_.data();
    DSPSplitComplex split{re, im};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(buf.data()), 2, &split, 1, N);
    vDSP_DFT_Execute(setup, re, im, re, im);
    // vDSP DFT_Execute is unnormalised in *both* directions — scaling is
    // therefore part of the inverse contract, not a stylistic choice, and
    // is required to keep the Apple-vDSP backend bit-aligned with the
    // scalar Cooley-Tukey conjugate-trick fallback below (which divides
    // by N at the same point).
    const float scale = 1.0F / static_cast<float>(N);
    vDSP_vsmul(re, 1, &scale, re, 1, N);
    vDSP_vsmul(im, 1, &scale, im, 1, N);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    vDSP_ztoc(&split, 1, reinterpret_cast<DSPComplex*>(buf.data()), 2, N);
  }

private:
  // Per-instance split-complex scratch (mutable for use from const methods —
  // legitimate scratch-storage exemption).  No heap; sized at compile time.
  // Total per Fft<N>: 8N bytes (2 × std::array<float, N>).  Owner must
  // serialise access — see thread-safety note on ``forward()``.
  alignas(16) mutable std::array<float, N> realp_{};
  alignas(16) mutable std::array<float, N> imagp_{};

  /// Lazily-created, leaked-at-exit DFT setups (one fwd + one inv per
  /// template instantiation).  Thread-safe since C++11 (magic statics).
  /// Returns nullptr if Accelerate cannot honour the request (out of
  /// memory, unsupported size on this OS revision); callers handle by
  /// scrubbing the output buffer.
  static vDSP_DFT_Setup setup_fwd() noexcept {
    static vDSP_DFT_Setup s = vDSP_DFT_zop_CreateSetup(
        nullptr, N, vDSP_DFT_FORWARD);
    return s;
  }
  static vDSP_DFT_Setup setup_inv() noexcept {
    static vDSP_DFT_Setup s = vDSP_DFT_zop_CreateSetup(
        nullptr, N, vDSP_DFT_INVERSE);
    return s;
  }

#else
  // ─── Scalar radix-2 Cooley-Tukey DIT backend ───────────────────────────────

  /// Pre-compute twiddle factors and bit-reverse permutation table.
  ///
  /// Thread-safety: the scalar backend's ``forward``/``inverse`` only
  /// read ``twiddles_`` and ``bitrev_`` — both are populated in the
  /// constructor and never mutated thereafter.  A single Fft<N>
  /// instance is therefore safe to share across threads (opposite of
  /// the Apple/vDSP backend, which holds per-instance ``mutable``
  /// split-complex scratch).  Despite this asymmetry, callers should
  /// treat Fft<N> as *not* shareable to keep the cross-platform
  /// contract uniform — Correlator owns one Fft<N> per worker thread
  /// regardless of backend.
  Fft() noexcept {
    // Twiddle factors: W_N^k = exp(-j 2π k/N) for k=0..N/2-1.
    for (uint32_t k = 0; k < N / 2U; ++k) {
      const auto angle = static_cast<float>(-2.0 * std::numbers::pi * static_cast<double>(k) /
                                            static_cast<double>(N));
      // Index bounded by loop limit k < N/2 —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      twiddles_[k] = Complex(std::cos(angle), std::sin(angle));
    }

    // Bit-reverse permutation table.
    for (uint32_t i = 0; i < N; ++i) {
      uint32_t j = 0;
      for (uint32_t b = 0; b < kLog2N; ++b) {
        j |= ((i >> b) & 1U) << (kLog2N - 1U - b);
      }
      // Index bounded by loop limit i < N —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      bitrev_[i] = static_cast<uint16_t>(j);
    }
  }

  /// In-place forward FFT.
  void forward(std::span<Complex, N> buf) const noexcept {
    bit_reverse_permute(buf);
    butterfly(buf);
  }

  /// In-place inverse FFT with 1/N scaling.
  /// Uses conjugate trick: IFFT(x) = conj(FFT(conj(x))) / N.
  void inverse(std::span<Complex, N> buf) const noexcept {
    // Conjugate input.
    for (uint32_t i = 0; i < N; ++i) {
      buf[i] = std::conj(buf[i]);
    }
    // Forward FFT.
    bit_reverse_permute(buf);
    butterfly(buf);
    // Conjugate output and scale.
    const auto scale = 1.0F / static_cast<float>(N);
    for (uint32_t i = 0; i < N; ++i) {
      buf[i] = std::conj(buf[i]) * scale;
    }
  }

private:
  std::array<Complex, N / 2> twiddles_{};
  std::array<uint16_t, N> bitrev_{};

  /// Apply precomputed bit-reverse permutation.
  void bit_reverse_permute(std::span<Complex, N> buf) const noexcept {
    for (uint32_t i = 0; i < N; ++i) {
      // Index bounded by loop limit i < N —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      const auto j = static_cast<uint32_t>(bitrev_[i]);
      if (i < j) {
        std::swap(buf[i], buf[j]);
      }
    }
  }

  /// Forward-only butterfly with scalar real/imag decomposition.
  /// Avoids std::complex multiply to help auto-vectorization.
  void butterfly(std::span<Complex, N> buf) const noexcept {
    // Reinterpret as interleaved float pairs [re0, im0, re1, im1, ...].
    // Scalar float access for vectorizable butterfly; explicit type aids readability —
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,hicpp-use-auto)
    float* const data = reinterpret_cast<float*>(buf.data());
    // Scalar twiddle access for vectorizable butterfly; explicit type aids readability —
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,hicpp-use-auto)
    const float* const tw = reinterpret_cast<const float*>(twiddles_.data());

    for (uint32_t half = 1; half < N; half <<= 1U) {
      const uint32_t step = half << 1U;
      const uint32_t tw_stride = N / step;

      for (uint32_t group = 0; group < N; group += step) {
        // Pointer arithmetic for scalar butterfly; widening is safe for N<=65536 —
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-implicit-widening-of-multiplication-result)
        float* const p = data + (group << 1U);
        for (uint32_t k = 0; k < half; ++k) {
          const uint32_t tw_idx = (k * tw_stride) << 1U;
          const float wr = tw[tw_idx];
          const float wi = tw[tw_idx + 1U];

          const uint32_t ek = k << 1U;
          const uint32_t ok = (k + half) << 1U;

          const float er = p[ek];
          const float ei = p[ek + 1U];
          const float odr = p[ok];
          const float odi = p[ok + 1U];

          // Complex multiply: w * odd = (wr*odr - wi*odi, wr*odi + wi*odr)
          const float tr = wr * odr - wi * odi;
          const float ti = wr * odi + wi * odr;

          p[ek] = er + tr;
          p[ek + 1U] = ei + ti;
          p[ok] = er - tr;
          p[ok + 1U] = ei - ti;
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-implicit-widening-of-multiplication-result)
      }
    }
  }

#endif // __APPLE__

public:
  /// Forward FFT on dynamic span (must be exactly N elements).
  [[nodiscard("Fft::forward returns false on buffer-size mismatch (must be exactly N); "
                "ignoring it leaves the input buffer untransformed while the caller "
                "assumes a frequency-domain result")]] bool
  forward(std::span<Complex> buf) const noexcept {
    if (buf.size() != N) {
      return false;
    }
    forward(std::span<Complex, N>{buf.data(), N});
    return true;
  }

  /// Inverse FFT on dynamic span (must be exactly N elements).
  [[nodiscard("Fft::inverse returns false on buffer-size mismatch (must be exactly N); "
                "ignoring it leaves the input buffer untransformed while the caller "
                "assumes a time-domain result")]] bool
  inverse(std::span<Complex> buf) const noexcept {
    if (buf.size() != N) {
      return false;
    }
    inverse(std::span<Complex, N>{buf.data(), N});
    return true;
  }
};

/// Convenience alias for 2048-pt FFT (EPL correlator integration length).
using Fft2048 = Fft<2048>;

/// 4096-pt FFT for unambiguous PCPS acquisition.
/// Zero-padding the 2046-chip code + 2048-sample received signal to 4096
/// gives linear (not circular) cross-correlation, eliminating the code-phase
/// ambiguity from the 2048/2046 mismatch (spec finding SF-004).
using Fft4096 = Fft<4096>;

/// 8192-pt FFT for GPS L1 C/A acquisition at 8 spc with dynamic FFT sizing
/// (next_pow2(8184) = 8192).
using Fft8192 = Fft<8192>;

/// 16384-pt FFT for multi-signal PCPS acquisition:
///   - GPS L1 C/A at 8 spc (1023 × 8 = 8184 samples, zero-padded)
///   - BeiDou B1C at 1 spc (10230 chips, zero-padded)
///   - AFS-Q at 1 spc (10230 chips, zero-padded)
using Fft16384 = Fft<16384>;

/// 32768-pt FFT for GPS L5 acquisition at 2 samples/chip and
/// Galileo E1 at 8 samples/chip (4092 × 8 = 32736 samples).
/// Zero-padded for linear cross-correlation.
using Fft32768 = Fft<32768>;

/// 65536-pt FFT for deep coherent integration on long-code signals:
///   - AFS-Q at t_coh=4 epochs (10230 × 4 = 40920 samples)
///   - BeiDou B1C / NavIC at t_coh=2+ epochs
using Fft65536 = Fft<65536>;

} // namespace lunalink::signal
