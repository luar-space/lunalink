#pragma once
#include <cstdint>

namespace lunalink::signal {

/// Modulate chip sequence with a BPSK data symbol (AFS-I channel, BPSK(1)).
///   chips       - array of {0, 1}, length chip_count
///   data_symbol - +1 or -1
///   out         - caller-allocated, length >= chip_count
/// Chip mapping per spec section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1;
/// result multiplied by data_symbol.
void modulate_bpsk_i(
    const uint8_t* chips,
    uint16_t       chip_count,
    int8_t         data_symbol,
    int8_t*        out
) noexcept;

} // namespace lunalink::signal
