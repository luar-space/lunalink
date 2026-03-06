// GENERATED — do not edit. Re-run scripts/gen_prn_table.py to regenerate.
#pragma once
#include <cstdint>

namespace lunalink::signal {
// 2046 chips packed MSB-first into 256 bytes per PRN.
// NOLINTNEXTLINE(hicpp-avoid-c-arrays)
alignas(64) extern const uint8_t kGoldPrnsPacked[210][256];
} // namespace lunalink::signal
