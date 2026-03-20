#pragma once
#include "lunalink/core/safety.hpp"

#include <array>
#include <cstdint>

namespace lunalink::signal {

/**
 * @brief Precomputed BCH(51,8) codewords.
 * 400 entries (4 FIDs x 100 TOIs).
 */
extern const std::array<uint64_t, 400> kBchCodebook;

} // namespace lunalink::signal
