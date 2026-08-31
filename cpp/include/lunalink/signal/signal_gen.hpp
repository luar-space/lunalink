#pragma once
#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for composite signal generation. [LSIS-AFS-301]
 *  SEU-hardened with high Hamming distance bit patterns. */
enum class SignalGenStatus : uint8_t {
  kOk = 0x5AU,               ///< Operation completed successfully.
  kInvalidPrn = 0xA5U,       ///< PRN ID out of range or has no assignment.
  kInvalidInput = 0x33U,     ///< Frame symbols wrong length or invalid values.
  kModulationFailed = 0xCCU, ///< BPSK modulation step failed.
  kMuxFailed = 0x99U,        ///< IQ multiplexer failed.
  kFaultDetected = 0x66U,    ///< CFI or integrity check failed.
};

/// Interleaved I/Q samples per 2 ms epoch: 10230 × 2 = 20460.
inline constexpr uint32_t kIqPairsPerEpoch = 2 * kIqSamplesPerEpoch;

/// Interleaved I/Q samples per 12 s frame: 20460 × 6000 = 122,760,000.
inline constexpr uint32_t kIqPairsPerFrame = static_cast<uint32_t>(kIqPairsPerEpoch) *
                                             kEpochsPerFrame;

/// Native Q-channel sample rate in Hz (5.115 MHz).
inline constexpr double kNativeSampleRateHz = 5'115'000.0;

/**
 * @brief Generate one 2 ms epoch of interleaved I/Q at native rate. [LSIS-AFS-301]
 *
 * Performs: matched code → BPSK-I/Q modulate → IQ multiplex.
 * The caller provides the pre-unpacked Gold code to avoid redundant work
 * when generating multiple epochs for the same PRN.
 *
 * @param gold_chips   Pre-unpacked Gold code (kGoldChipLength uint8_t).
 * @param prn_id       PRN identifier (for Q-channel matched code lookup).
 * @param epoch_idx    Epoch within frame (0–5999).
 * @param data_symbol  Navigation data bit: +1 or -1.
 * @param out          Output buffer for interleaved [I,Q] pairs (≥ 20460 int16_t).
 * @return SignalGenStatus::kOk on success.
 */
[[nodiscard("SignalGenStatus reports PRN/epoch range and buffer-size failures from "
            "the matched-code → BPSK → IQ-mux pipeline; ignoring it allows malformed "
            "I/Q samples to reach the RF stage (LSIS-AFS-301)")]] SignalGenStatus
generate_iq_epoch(std::span<const uint8_t> gold_chips,
                  PrnId prn_id,
                  uint16_t epoch_idx,
                  int8_t data_symbol,
                  std::span<int16_t> out) noexcept;

/**
 * @brief Generate a full 12 s frame of interleaved I/Q at native rate (int16). [LSIS-AFS-301]
 *
 * Unpacks the Gold code once, then loops over 6000 epochs.
 *
 * @param prn_id        PRN identifier (1–12 for interim assignments).
 * @param frame_symbols 6000 binary frame symbols (0 or 1, from frame_build).
 * @param out           Output buffer (≥ kIqPairsPerFrame int16_t).
 * @return SignalGenStatus::kOk on success.
 */
[[nodiscard("SignalGenStatus reports PRN/symbol-bounds and buffer-size failures "
            "from the 6000-epoch frame-generation pipeline; ignoring it allows a "
            "malformed I/Q frame to reach the RF stage (LSIS-AFS-301)")]] SignalGenStatus
generate_iq_frame(PrnId prn_id,
                  std::span<const uint8_t> frame_symbols,
                  std::span<int16_t> out) noexcept;

/**
 * @brief Generate one 2 ms epoch as float32, with optional resampling. [LSIS-AFS-301]
 *
 * @param prn_id        PRN identifier (1–12 for interim assignments).
 * @param epoch_idx     Epoch within frame (0–5999).
 * @param data_symbol   Navigation data bit: +1 or -1.
 * @param sample_rate   Target sample rate in Hz (0 or kNativeSampleRateHz for native).
 * @param out           Output buffer (≥ required float count).
 * @param samples_written  Set to the number of float values written to out.
 * @return SignalGenStatus::kOk on success.
 */
[[nodiscard("SignalGenStatus reports PRN/epoch range, sample-rate validity and "
            "buffer-size failures from the float-output epoch pipeline; ignoring it "
            "leaves samples_written stale and floats indeterminate "
            "(LSIS-AFS-301)")]] SignalGenStatus
generate_iq_epoch_float(PrnId prn_id,
                        uint16_t epoch_idx,
                        int8_t data_symbol,
                        double sample_rate,
                        std::span<float> out,
                        size_t& samples_written) noexcept;

/**
 * @brief Generate a full 12 s frame as float32, with optional resampling. [LSIS-AFS-301]
 *
 * If sample_rate == kNativeSampleRateHz, writes float32 directly (no resampling).
 * Otherwise, performs nearest-neighbour resampling per epoch with a precomputed
 * index table (zero per-sample lround in the hot loop).
 *
 * @param prn_id        PRN identifier (1–12 for interim assignments).
 * @param frame_symbols 6000 binary frame symbols (0 or 1).
 * @param sample_rate   Target sample rate in Hz (0 or kNativeSampleRateHz for native).
 * @param out           Output buffer (≥ required float count).
 * @param samples_written  Set to the number of float values written to out.
 * @return SignalGenStatus::kOk on success.
 */
[[nodiscard("SignalGenStatus reports PRN/symbol-bounds, sample-rate validity and "
            "buffer-size failures from the float-output frame pipeline; ignoring it "
            "leaves samples_written stale and floats indeterminate "
            "(LSIS-AFS-301)")]] SignalGenStatus
generate_iq_frame_float(PrnId prn_id,
                        std::span<const uint8_t> frame_symbols,
                        double sample_rate,
                        std::span<float> out,
                        size_t& samples_written) noexcept;

} // namespace lunalink::signal
