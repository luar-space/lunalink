// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"

#include <algorithm> // for std::fill
#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for BPSK modulation (AFS-I data channel and AFS-Q pilot). [LSIS-AFS-201]
 * SEU-hardened with high Hamming distance bit patterns. */
enum class ModulationStatus : uint8_t {
  kOk = 0x5AU,               ///< Operation completed successfully.
  kInvalidSymbol = 0xA5U,    ///< Data symbol not in {-1, +1}.
  kInvalidChipValue = 0x33U, ///< Chip value not in {0, 1}.
  kOutputTooSmall = 0xCCU,   ///< Output buffer too small for result.
  kFaultDetected = 0x99U,    ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// @brief Validate modulation inputs: checks data symbol is +/-1, output span
///        is large enough for the chip sequence, and input/output do not alias.
[[nodiscard("ModulationStatus reports invalid data-symbol, output-buffer or "
            "input/output aliasing failures; ignoring it lets unsafe modulator "
            "inputs through (LSIS-AFS-201, FIZ-1)")]] inline ModulationStatus
validate_modulation_inputs(std::span<const uint8_t> chips,
                           int8_t data_symbol,
                           std::span<int8_t> out) noexcept {
  if (data_symbol != 1 && data_symbol != -1) [[unlikely]] {
    return ModulationStatus::kInvalidSymbol;
  }
  if (out.size() < chips.size()) [[unlikely]] {
    return ModulationStatus::kOutputTooSmall;
  }
  if (spans_overlap(static_cast<const void*>(chips.data()),
                    chips.size_bytes(),
                    static_cast<const void*>(out.data()),
                    out.size_bytes())) [[unlikely]] {
    return ModulationStatus::kFaultDetected; // FIZ-1
  }
  return ModulationStatus::kOk;
}

/**
 * @brief Internal generic BPSK modulator implementation. [LSIS-AFS-201]
 * Uses a branchless arithmetic mapping: 0 -> +1, 1 -> -1.
 */
[[nodiscard("ModulationStatus reports validation failures, kInvalidChipValue mid-loop, "
            "and CFI terminal-count faults; ignoring it accepts a partially-scrubbed "
            "output as valid (LSIS-AFS-201, FIZ-1)")]] inline ModulationStatus
modulate_bpsk_generic(std::span<const uint8_t> chips,
                      int8_t data_symbol,
                      std::span<int8_t> out) noexcept {
  ModulationStatus status = validate_modulation_inputs(chips, data_symbol, out);

  if (status == ModulationStatus::kOk) {
    volatile uint32_t loop_count = 0;
    for (std::size_t i = 0; i < chips.size(); ++i) {
      const uint8_t chip = chips[i];
      if (chip > 1) [[unlikely]] {
        status = ModulationStatus::kInvalidChipValue;
        break;
      }
      // Branchless mapping: 0 -> +1, 1 -> -1
      out[i] =
          static_cast<int8_t>((1 - 2 * static_cast<int>(chip)) * static_cast<int>(data_symbol));
      LUNALINK_CRITICAL_ENTER;
      loop_count = loop_count + 1U;
      LUNALINK_CRITICAL_EXIT;
      wip_tick();
    }

    // CFI: Terminal count verification.
    if (status == ModulationStatus::kOk && loop_count != chips.size()) [[unlikely]] {
      status = ModulationStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI terminal
                                                 // count — loop count always matches chips.size()
                                                 // under nominal execution
    }

    if (status != ModulationStatus::kOk) [[unlikely]] {
      // Re-clear on error to prevent partial modulation leakage.
      secure_scrub(out);
    }
  }
  return status;
}

/**
 * @brief Modulate a chip sequence for the AFS-I channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard(
    "ModulationStatus reports modulator validation/CFI failures; ignoring it "
    "lets a corrupt AFS-I chip stream reach the RF stage (LSIS-AFS-201)")]] inline ModulationStatus
modulate_bpsk_i(T&& prn_chips, int8_t data_symbol, U&& out) noexcept {
  return modulate_bpsk_generic(
      std::span<const uint8_t>(prn_chips), data_symbol, std::span<int8_t>(out));
}

/**
 * @brief Modulate a chip sequence for the AFS-Q channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard(
    "ModulationStatus reports modulator validation/CFI failures; ignoring it "
    "lets a corrupt AFS-Q chip stream reach the RF stage (LSIS-AFS-201)")]] inline ModulationStatus
modulate_bpsk_q(T&& chips, U&& out) noexcept {
  return modulate_bpsk_generic(std::span<const uint8_t>(chips), 1, std::span<int8_t>(out));
}

/**
 * @brief Modulate a chip sequence for any BPSK channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard(
    "ModulationStatus reports modulator validation/CFI failures; ignoring it "
    "lets a corrupt BPSK chip stream reach the RF stage (LSIS-AFS-201)")]] inline ModulationStatus
modulate_bpsk_any(T&& chips, int8_t data_symbol, U&& out) noexcept {
  return modulate_bpsk_generic(
      std::span<const uint8_t>(chips), data_symbol, std::span<int8_t>(out));
}

} // namespace lunalink::signal
