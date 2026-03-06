// GENERATED — do not edit. Re-run scripts/gen_prn_table.py to regenerate.
#pragma once
#include <cstdint>

namespace lunalink::signal {
// 1500 chips packed MSB-first into 188 bytes per PRN.
// NOLINTNEXTLINE(hicpp-avoid-c-arrays)
alignas(64) extern const uint8_t kWeil1500PrnsPacked[210][188];
} // namespace lunalink::signal
