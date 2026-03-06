#pragma once
#include <cstdint>

namespace lunalink::signal {

enum class ModulationStatus : uint8_t {
  kOk = 0,
  kNullInput,
  kInvalidSymbol,
  kInvalidChipValue,
};

/// Modulate chip sequence with a BPSK data symbol (AFS-I channel, BPSK(1)).
///   chips       - array of {0, 1}, length chip_count
///   data_symbol - +1 or -1
///   out         - caller-allocated, length >= chip_count
/// Chip mapping per spec section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1;
/// result multiplied by data_symbol.
/// Returns explicit status for invalid input conditions.
[[nodiscard]] ModulationStatus modulate_bpsk_i(
    const uint8_t* chips,
    uint16_t       chip_count,
    int8_t         data_symbol,
    int8_t*        out
) noexcept;

/// Modulate chip sequence for AFS-Q pilot channel (BPSK(5), no data symbol).
///   chips      - array of {0, 1}, length chip_count
///   chip_count - number of chips (typically 10230 for one AFS-Q epoch)
///   out        - caller-allocated, length >= chip_count
/// Chip mapping per spec section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1.
/// Returns explicit status for invalid input conditions.
[[nodiscard]] ModulationStatus modulate_bpsk_q(
    const uint8_t* chips,
    uint16_t       chip_count,
    int8_t*        out
) noexcept;

} // namespace lunalink::signal
