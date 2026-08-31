// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/signal/signal_gen.hpp"

#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace lunalink::signal {

namespace {

/// Unpack a packed Gold code into a chip array.  Called once per PRN per frame.
[[nodiscard]] SignalGenStatus unpack_gold(PrnId prn_id,
                                          std::span<uint8_t, kGoldChipLength> out) noexcept {
  PrnCode gold{};
  if (gold_prn_packed(prn_id, gold) != PrnStatus::kOk) {
    return SignalGenStatus::kInvalidPrn;
  }
  for (uint32_t i = 0; i < kGoldChipLength; ++i) {
    if (unpack_chip(gold, static_cast<uint16_t>(i), out[i]) != PrnStatus::kOk) {
      return SignalGenStatus::kFaultDetected;
    }
  }
  return SignalGenStatus::kOk;
}

/// Convert one native-rate epoch (int16 interleaved) to float, with optional
/// nearest-neighbour resampling via a precomputed LUT.
///
/// @param src_buf     Source: kIqPairsPerEpoch int16 values ([I0,Q0,I1,Q1,...]).
/// @param dst_buf     Destination float span (≥ n_out * 2 floats).
/// @param n_native    Native sample count per epoch (kIqSamplesPerEpoch).
/// @param n_out       Output sample count per epoch (may differ from n_native).
/// @param resample_lut  Empty span for native rate; otherwise n_out source indices.
inline void convert_epoch(std::span<const int16_t> src_buf,
                          std::span<float> dst_buf,
                          size_t n_native,
                          size_t n_out,
                          std::span<const uint16_t> resample_lut) noexcept {
  if (resample_lut.empty()) {
    // Direct copy: int16 → float.
    for (size_t j = 0; j < n_native * 2; ++j) {
      dst_buf[j] = static_cast<float>(src_buf[j]);
    }
  } else {
    // Nearest-neighbour resample + convert.
    for (size_t j = 0; j < n_out; ++j) {
      const size_t src = resample_lut[j];
      dst_buf[j * 2] = static_cast<float>(src_buf[src * 2]);
      dst_buf[j * 2 + 1] = static_cast<float>(src_buf[src * 2 + 1]);
    }
  }
}

/// Maximum output samples per epoch (4× oversampling of Q-rate: 10230 × 4 = 40920).
/// Any requested sample rate yielding more samples than this is rejected.
constexpr uint16_t kMaxResamplePerEpoch = 40960U;

/// Static workspace for the nearest-neighbour resample LUT.
/// Uses uint16_t indices (max 40960 fits in uint16_t, saves 6× vs size_t).
/// Avoids std::vector (heap allocation forbidden in flight code).
struct ResampleLut {
  // 80 KB — must NOT be stack-allocated. Use file-scope static or caller workspace.
  std::array<uint16_t, kMaxResamplePerEpoch> indices{};
  uint16_t n_out = 0;
  bool active = false; ///< true if resampling is needed (n_out != n_native).
};

// File-scope resample LUT — avoids stack overflow and repeated zero-init.
// NOT thread-safe; signal generation is single-threaded by design.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
ResampleLut g_resample_lut;

/// Compute output sample count per epoch and build the resampling LUT.
/// Returns true on success; false if the requested rate exceeds kMaxResamplePerEpoch.
[[nodiscard]] bool build_resample_lut(double sample_rate, ResampleLut& lut) noexcept {
  const auto n_native = static_cast<size_t>(kIqSamplesPerEpoch);
  const bool is_native = (std::fabs(sample_rate - kNativeSampleRateHz) < 1.0);
  const size_t n_out = is_native
                           ? n_native
                           : static_cast<size_t>(std::lround(static_cast<double>(n_native) *
                                                             sample_rate / kNativeSampleRateHz));

  if (n_out > kMaxResamplePerEpoch || n_out == 0) {
    lut.n_out = 0;
    lut.active = false;
    return false;
  }

  lut.n_out = static_cast<uint16_t>(n_out);
  lut.active = (n_out != n_native);

  if (lut.active) {
    const double ratio = static_cast<double>(n_native - 1) / static_cast<double>(n_out - 1);
    for (size_t j = 0; j < n_out; ++j) {
      // Index bounded by n_out <= kMaxResamplePerEpoch checked above —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      lut.indices[j] = static_cast<uint16_t>(std::lround(static_cast<double>(j) * ratio));
    }
  }
  return true;
}

} // namespace

[[nodiscard]] SignalGenStatus generate_iq_epoch(std::span<const uint8_t> gold_chips,
                                                PrnId prn_id,
                                                uint16_t epoch_idx,
                                                int8_t data_symbol,
                                                std::span<int16_t> out) noexcept {
  if (gold_chips.size() < kGoldChipLength) {
    return SignalGenStatus::kInvalidInput;
  }
  if (out.size() < kIqPairsPerEpoch) {
    return SignalGenStatus::kInvalidInput;
  }
  if (data_symbol != 1 && data_symbol != -1) {
    return SignalGenStatus::kInvalidInput;
  }

  // 1. Generate matched Q-channel code for this epoch
  std::array<uint8_t, kWeil10230ChipLength> q_chips{};
  if (matched_code_epoch(prn_id, epoch_idx, std::span<uint8_t>(q_chips)) !=
      MatchedCodeStatus::kOk) {
    return SignalGenStatus::kInvalidPrn;
  }

  // 2. BPSK modulate I and Q
  std::array<int8_t, kGoldChipLength> i_mod{};
  if (modulate_bpsk_i(gold_chips.first(kGoldChipLength), data_symbol, std::span<int8_t>(i_mod)) !=
      ModulationStatus::kOk) {
    return SignalGenStatus::kModulationFailed;
  }

  std::array<int8_t, kWeil10230ChipLength> q_mod{};
  if (modulate_bpsk_q(std::span<const uint8_t>(q_chips), std::span<int8_t>(q_mod)) !=
      ModulationStatus::kOk) {
    return SignalGenStatus::kModulationFailed;
  }

  // 3. Multiplex I/Q into interleaved output
  if (multiplex_iq(std::span<const int8_t>(i_mod),
                   std::span<const int8_t>(q_mod),
                   out.subspan(0, kIqPairsPerEpoch)) != IqMuxStatus::kOk) {
    return SignalGenStatus::kMuxFailed;
  }

  return SignalGenStatus::kOk;
}

[[nodiscard]] SignalGenStatus generate_iq_frame(PrnId prn_id,
                                                std::span<const uint8_t> frame_symbols,
                                                std::span<int16_t> out) noexcept {
  if (frame_symbols.size() != kEpochsPerFrame) {
    return SignalGenStatus::kInvalidInput;
  }
  if (out.size() < kIqPairsPerFrame) {
    return SignalGenStatus::kInvalidInput;
  }

  // Unpack Gold code ONCE for the entire frame.
  std::array<uint8_t, kGoldChipLength> gold_chips{};
  {
    const auto s = unpack_gold(prn_id, std::span<uint8_t, kGoldChipLength>(gold_chips));
    if (s != SignalGenStatus::kOk) {
      return s;
    }
  }

  for (uint16_t epoch = 0; epoch < kEpochsPerFrame; ++epoch) {
    const uint8_t sym = frame_symbols[epoch];
    if (sym > 1) {
      return SignalGenStatus::kInvalidInput;
    }
    const auto data_symbol = static_cast<int8_t>(1 - 2 * static_cast<int>(sym));

    const auto status = generate_iq_epoch(
        std::span<const uint8_t>(gold_chips),
        prn_id,
        epoch,
        data_symbol,
        out.subspan(static_cast<size_t>(epoch) * kIqPairsPerEpoch, kIqPairsPerEpoch));

    if (status != SignalGenStatus::kOk) {
      return status;
    }
  }

  return SignalGenStatus::kOk;
}

[[nodiscard]] SignalGenStatus generate_iq_epoch_float(PrnId prn_id,
                                                      uint16_t epoch_idx,
                                                      int8_t data_symbol,
                                                      double sample_rate,
                                                      std::span<float> out,
                                                      size_t& samples_written) noexcept {
  samples_written = 0;

  if (data_symbol != 1 && data_symbol != -1) {
    return SignalGenStatus::kInvalidInput;
  }
  if (sample_rate <= 0.0) {
    return SignalGenStatus::kInvalidInput;
  }

  const auto n_native = static_cast<size_t>(kIqSamplesPerEpoch);
  if (!build_resample_lut(sample_rate, g_resample_lut)) {
    return SignalGenStatus::kInvalidInput;
  }
  const auto& lut = g_resample_lut;

  if (out.size() < static_cast<size_t>(lut.n_out) * 2U) {
    return SignalGenStatus::kInvalidInput;
  }

  // Unpack Gold code.
  std::array<uint8_t, kGoldChipLength> gold_chips{};
  {
    const auto s = unpack_gold(prn_id, gold_chips);
    if (s != SignalGenStatus::kOk) {
      return s;
    }
  }

  // Generate native epoch.
  std::array<int16_t, kIqPairsPerEpoch> native_buf{};
  {
    const auto s = generate_iq_epoch(gold_chips, prn_id, epoch_idx, data_symbol, native_buf);
    if (s != SignalGenStatus::kOk) {
      return s;
    }
  }

  const auto resample_span = lut.active ? std::span<const uint16_t>(lut.indices.data(), lut.n_out)
                                        : std::span<const uint16_t>();
  convert_epoch(native_buf,
                out.subspan(0, static_cast<size_t>(lut.n_out) * 2U),
                n_native,
                lut.n_out,
                resample_span);

  samples_written = static_cast<size_t>(lut.n_out) * 2U;
  return SignalGenStatus::kOk;
}

[[nodiscard]] SignalGenStatus generate_iq_frame_float(PrnId prn_id,
                                                      std::span<const uint8_t> frame_symbols,
                                                      double sample_rate,
                                                      std::span<float> out,
                                                      size_t& samples_written) noexcept {
  samples_written = 0;

  if (frame_symbols.size() != kEpochsPerFrame) {
    return SignalGenStatus::kInvalidInput;
  }
  if (sample_rate <= 0.0) {
    return SignalGenStatus::kInvalidInput;
  }

  const auto n_native = static_cast<size_t>(kIqSamplesPerEpoch);
  if (!build_resample_lut(sample_rate, g_resample_lut)) {
    return SignalGenStatus::kInvalidInput;
  }
  const auto& lut = g_resample_lut;
  const size_t total_out_pairs = static_cast<size_t>(lut.n_out) * kEpochsPerFrame;

  if (out.size() < total_out_pairs * 2U) {
    return SignalGenStatus::kInvalidInput;
  }

  // Unpack Gold code ONCE for the entire frame.
  std::array<uint8_t, kGoldChipLength> gold_chips{};
  {
    const auto s = unpack_gold(prn_id, gold_chips);
    if (s != SignalGenStatus::kOk) {
      return s;
    }
  }

  const auto resample_span = lut.active ? std::span<const uint16_t>(lut.indices.data(), lut.n_out)
                                        : std::span<const uint16_t>();

  // Stack buffer for one epoch at native rate.
  std::array<int16_t, kIqPairsPerEpoch> native_buf{};

  for (uint16_t epoch = 0; epoch < kEpochsPerFrame; ++epoch) {
    const uint8_t sym = frame_symbols[epoch];
    if (sym > 1) {
      return SignalGenStatus::kInvalidInput;
    }
    const auto data_symbol = static_cast<int8_t>(1 - 2 * static_cast<int>(sym));

    const auto status = generate_iq_epoch(gold_chips, prn_id, epoch, data_symbol, native_buf);

    if (status != SignalGenStatus::kOk) {
      return status;
    }

    const size_t out_offset = static_cast<size_t>(epoch) * static_cast<size_t>(lut.n_out) * 2U;
    convert_epoch(native_buf,
                  out.subspan(out_offset, static_cast<size_t>(lut.n_out) * 2U),
                  n_native,
                  lut.n_out,
                  resample_span);
  }

  samples_written = total_out_pairs * 2;
  return SignalGenStatus::kOk;
}

} // namespace lunalink::signal
