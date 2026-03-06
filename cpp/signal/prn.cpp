#include <cassert>
#include <cstddef>

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"
#include "lunalink/signal/prn_table_weil10230.hpp"
#include "lunalink/signal/prn_table_weil1500.hpp"

namespace lunalink::signal {

const uint8_t* gold_prn_packed(uint8_t prn_id) noexcept {
  assert(prn_id >= 1 && prn_id <= kPrnCount);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  return kGoldPrnsPacked[static_cast<size_t>(prn_id) - 1U];
}

const uint8_t* weil10230_prn_packed(uint8_t prn_id) noexcept {
  assert(prn_id >= 1 && prn_id <= kPrnCount);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  return kWeil10230PrnsPacked[static_cast<size_t>(prn_id) - 1U];
}

const uint8_t* weil1500_prn_packed(uint8_t prn_id) noexcept {
  assert(prn_id >= 1 && prn_id <= kPrnCount);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  return kWeil1500PrnsPacked[static_cast<size_t>(prn_id) - 1U];
}

} // namespace lunalink::signal
