#pragma once
#include <cstdint>
#include "lunalink/signal/prn.hpp"
#include <span>
#include <algorithm> // for std::fill
#include <cassert>

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for Modulation operations.
 * Non-contiguous bit patterns with high Hamming distance resist SEU bit-flips.
 */
enum class ModulationStatus : uint8_t {
  kOk               = 0x5AU,  // 01011010
  kInvalidSymbol    = 0xA5U,  // 10100101
  kInvalidChipValue = 0x33U,  // 00110011
  kOutputTooSmall   = 0xCCU,  // 11001100
  kFaultDetected    = 0x99U,  // 10011001
};

/**
 * @brief Internal generic BPSK modulator implementation. [LSIS-AFS-201]
 * Uses a branchless arithmetic mapping: 0 -> +1, 1 -> -1.
 */
[[nodiscard]] inline ModulationStatus modulate_bpsk_generic(
    std::span<const uint8_t> chips,
    int8_t                   data_symbol,
    std::span<int8_t>        out) noexcept {
  
  ModulationStatus status = ModulationStatus::kOk;

  // ESA/JAXA Safety Policy: Pre-initialise buffer.
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), static_cast<int8_t>(0));
  }

  if (data_symbol != 1 && data_symbol != -1) [[unlikely]] {
    status = ModulationStatus::kInvalidSymbol;
  } else if (out.size() < chips.size()) [[unlikely]] {
    status = ModulationStatus::kOutputTooSmall;
  }

  if (status == ModulationStatus::kOk) {
    // JAXA Safety: Aliasing Protection.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    assert(static_cast<const void*>(out.data()) >= static_cast<const void*>(chips.data() + chips.size()) ||
           static_cast<const void*>(out.data() + out.size()) <= static_cast<const void*>(chips.data()));

    uint32_t loop_count = 0;
    for (std::size_t i = 0; i < chips.size(); ++i) {
      const uint8_t chip = chips[i];
      if (chip > 1) [[unlikely]] {
        status = ModulationStatus::kInvalidChipValue;
        break;
      }
      // Branchless mapping: 0 -> +1, 1 -> -1
      out[i] = static_cast<int8_t>((1 - 2 * static_cast<int>(chip)) * static_cast<int>(data_symbol));
      loop_count++;
    }

    // CFI: Terminal count verification.
    if (status == ModulationStatus::kOk && loop_count != chips.size()) [[unlikely]] {
        status = ModulationStatus::kFaultDetected;
    }

    if (status != ModulationStatus::kOk) [[unlikely]] {
      // Re-clear on error to prevent partial modulation leakage.
      std::fill(out.begin(), out.end(), static_cast<int8_t>(0));
    }
  }
  return status;
}

/**
 * @brief Modulate a chip sequence for the AFS-I channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_i(
    T&&    prn_chips,
    int8_t data_symbol,
    U&&    out) noexcept {
  return modulate_bpsk_generic(std::span<const uint8_t>(prn_chips), data_symbol, std::span<int8_t>(out));
}

/**
 * @brief Modulate a chip sequence for the AFS-Q channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_q(
    T&& chips,
    U&& out) noexcept {
  return modulate_bpsk_generic(std::span<const uint8_t>(chips), 1, std::span<int8_t>(out));
}

/**
 * @brief Modulate a chip sequence for any BPSK channel. [LSIS-AFS-201]
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_any(
    T&& chips,
    int8_t data_symbol,
    U&& out) noexcept {
  return modulate_bpsk_generic(std::span<const uint8_t>(chips), data_symbol, std::span<int8_t>(out));
}

} // namespace lunalink::signal
