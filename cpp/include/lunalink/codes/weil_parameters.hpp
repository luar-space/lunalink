#pragma once
#include "lunalink/core/safety.hpp"

#include <array>
#include <cstdint>

namespace lunalink::signal {

struct WeilParams {
  uint16_t weil_index;      ///< Weil shift parameter w (LSIS Table D-1 / E-1).
  uint16_t insertion_index; ///< Expansion insertion point p (LSIS §2.3.5.2).
};

/**
 * @brief Appendix D: Weil code parameters for PRN 1-210.
 */
extern const std::array<WeilParams, 210> kPrimaryWeilParams;

/**
 * @brief Appendix E: Tertiary Weil code indices for PRN 1-210.
 * Tertiary insertion index is always 1 per Figure E-1 schematic.
 */
extern const std::array<uint16_t, 210> kTertiaryWeilIndices;

} // namespace lunalink::signal
