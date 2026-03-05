#pragma once
#include <cstdint>

namespace lunalink::signal {

inline constexpr uint8_t  kGoldPrnCount   = 210;
inline constexpr uint16_t kGoldChipLength = 2046;

/// Returns pointer to the 2046-chip Gold-2046 sequence for prn_id (1-indexed).
/// Precondition: prn_id in [1, 210]. Validated by the binding layer; UB otherwise.
[[nodiscard]] const uint8_t* gold_prn(uint8_t prn_id) noexcept;

} // namespace lunalink::signal
