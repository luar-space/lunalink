#include <cstddef>

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"
#include "lunalink/signal/prn_table_weil10230.hpp"
#include "lunalink/signal/prn_table_weil1500.hpp"

namespace lunalink::signal {

PrnStatus gold_prn_packed(uint8_t prn_id, const uint8_t **out_packed) noexcept {
  if (out_packed == nullptr) {
    return PrnStatus::kNullOutput;
  }
  if (prn_id < 1U || prn_id > kPrnCount) {
    return PrnStatus::kInvalidPrn;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  *out_packed = kGoldPrnsPacked[static_cast<size_t>(prn_id) - 1U];
  return PrnStatus::kOk;
}

PrnStatus weil10230_prn_packed(uint8_t prn_id,
                               const uint8_t **out_packed) noexcept {
  if (out_packed == nullptr) {
    return PrnStatus::kNullOutput;
  }
  if (prn_id < 1U || prn_id > kPrnCount) {
    return PrnStatus::kInvalidPrn;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  *out_packed = kWeil10230PrnsPacked[static_cast<size_t>(prn_id) - 1U];
  return PrnStatus::kOk;
}

PrnStatus weil1500_prn_packed(uint8_t prn_id,
                              const uint8_t **out_packed) noexcept {
  if (out_packed == nullptr) {
    return PrnStatus::kNullOutput;
  }
  if (prn_id < 1U || prn_id > kPrnCount) {
    return PrnStatus::kInvalidPrn;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  *out_packed = kWeil1500PrnsPacked[static_cast<size_t>(prn_id) - 1U];
  return PrnStatus::kOk;
}

} // namespace lunalink::signal
