#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"

namespace lunalink::signal {

const std::array<uint8_t, kGoldChipLength> &gold_prn(uint8_t prn_id) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  return kGoldPrns[static_cast<size_t>(prn_id) - 1U];
}

} // namespace lunalink::signal
