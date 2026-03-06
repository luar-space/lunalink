#pragma once
#include <cstdint>
#include "lunalink/signal/prn.hpp"
#include <span>
#include <algorithm> // for std::fill
#include <cassert>

namespace lunalink::signal {

enum class ModulationStatus : uint8_t {
  kOk = 0,
  kInvalidSymbol,
  kInvalidChipValue,
  kOutputTooSmall,
};

/**
 * @brief Internal generic BPSK modulator implementation.
 * Uses a branchless arithmetic mapping: 0 -> +1, 1 -> -1.
 * Mapping formula: (1 - 2*chip) * data_symbol.
 *
 * @note JAXA Safety: Output is zeroed on error. Buffers must not overlap.
 */
[[nodiscard]] inline ModulationStatus modulate_bpsk_generic(
    std::span<const uint8_t> chips,
    int8_t                   data_symbol,
    std::span<int8_t>        out) noexcept {
  
  // ESA/JAXA Safety Policy: Pre-initialise buffer.
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), static_cast<int8_t>(0));
  }

  if (data_symbol != 1 && data_symbol != -1) [[unlikely]] {
    return ModulationStatus::kInvalidSymbol;
  }
  if (out.size() < chips.size()) [[unlikely]] {
    return ModulationStatus::kOutputTooSmall;
  }

  // JAXA Safety: Aliasing Protection.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  assert(static_cast<const void*>(out.data()) >= static_cast<const void*>(chips.data() + chips.size()) ||
         static_cast<const void*>(out.data() + out.size()) <= static_cast<const void*>(chips.data()));

  for (std::size_t i = 0; i < chips.size(); ++i) {
    const uint8_t chip = chips[i];
    if (chip > 1) [[unlikely]] {
      // Re-clear on error to prevent partial modulation leakage.
      std::fill(out.begin(), out.end(), static_cast<int8_t>(0));
      return ModulationStatus::kInvalidChipValue;
    }
    // Branchless mapping: 0 -> +1, 1 -> -1
    out[i] = static_cast<int8_t>((1 - 2 * static_cast<int>(chip)) * static_cast<int>(data_symbol));
  }
  return ModulationStatus::kOk;
}

/**
 * @brief Modulate a chip sequence for the AFS-I channel.
 */
[[nodiscard]] inline ModulationStatus modulate_bpsk_i(
    std::span<const uint8_t> chips,
    int8_t                   data_symbol,
    std::span<int8_t>        out) noexcept {
  return modulate_bpsk_generic(chips, data_symbol, out);
}

/**
 * @brief Ergonomic overload for modulate_bpsk_i.
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_i(
    T&& chips,
    int8_t data_symbol,
    U&& out) noexcept {
  return modulate_bpsk_i(std::span<const uint8_t>(chips), data_symbol, std::span<int8_t>(out));
}

/**
 * @brief Modulate a chip sequence for the AFS-Q channel.
 */
[[nodiscard]] inline ModulationStatus modulate_bpsk_q(
    std::span<const uint8_t> chips,
    std::span<int8_t>        out) noexcept {
  return modulate_bpsk_generic(chips, 1, out);
}

/**
 * @brief Ergonomic overload for modulate_bpsk_q.
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_q(
    T&& chips,
    U&& out) noexcept {
  return modulate_bpsk_q(std::span<const uint8_t>(chips), std::span<int8_t>(out));
}

/**
 * @brief Generic modulator for testing or non-standard sizes.
 */
template <typename T, typename U>
[[nodiscard]] inline ModulationStatus modulate_bpsk_any(
    T&& chips,
    int8_t data_symbol,
    U&& out) noexcept {
  return modulate_bpsk_generic(std::span<const uint8_t>(chips), data_symbol, std::span<int8_t>(out));
}

} // namespace lunalink::signal
