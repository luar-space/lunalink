#pragma once
#include <complex>
#include <cstdint>
#include <span>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <array>
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

  Fft() noexcept = default;

  /// In-place forward FFT.
  void forward(std::span<Complex, N> buf) const noexcept {
    // std::complex<float> is layout-compatible with float[2] (C++11 §26.4/4).
    // reinterpret_cast to access interleaved real/imag pairs —
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* ptr = reinterpret_cast<float*>(buf.data());
    // Stride of 2 walks interleaved [re0, im0, re1, im1, ...] as split.
    DSPSplitComplex split{ptr, ptr + 1};
    vDSP_fft_zip(vdsp_setup(), &split, 2, kLog2N, kFFTDirection_Forward);
  }

  /// In-place inverse FFT with 1/N scaling.
  void inverse(std::span<Complex, N> buf) const noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* ptr = reinterpret_cast<float*>(buf.data());
    DSPSplitComplex split{ptr, ptr + 1};
    vDSP_fft_zip(vdsp_setup(), &split, 2, kLog2N, kFFTDirection_Inverse);
    // vDSP inverse is unnormalised — scale by 1/N to match standard IDFT.
    const float scale = 1.0F / static_cast<float>(N);
    // Widening to vDSP_Length for element count (2 floats per complex) —
    // NOLINTNEXTLINE(bugprone-implicit-widening-of-multiplication-result)
    vDSP_vsmul(ptr, 1, &scale, ptr, 1, static_cast<vDSP_Length>(N) * 2U);
  }

private:
  /// Lazily-created, leaked-at-exit FFTSetup (one per template instantiation).
  /// Thread-safe since C++11 (magic statics).
  static FFTSetup vdsp_setup() noexcept {
    static FFTSetup s = vDSP_create_fftsetup(kLog2N, kFFTRadix2);
    return s;
  }

#else
  // ─── Scalar radix-2 Cooley-Tukey DIT backend ───────────────────────────────

  /// Pre-compute twiddle factors and bit-reverse permutation table.
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
  [[nodiscard]] bool forward(std::span<Complex> buf) const noexcept {
    if (buf.size() != N) {
      return false;
    }
    forward(std::span<Complex, N>{buf.data(), N});
    return true;
  }

  /// Inverse FFT on dynamic span (must be exactly N elements).
  [[nodiscard]] bool inverse(std::span<Complex> buf) const noexcept {
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
