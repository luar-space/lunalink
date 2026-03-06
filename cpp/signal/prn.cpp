#include <cstddef>
#include <algorithm> // for std::fill

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"
#include "lunalink/signal/prn_table_weil10230.hpp"
#include "lunalink/signal/prn_table_weil1500.hpp"

namespace lunalink::signal {

PrnStatus gold_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  // ESA Safety: Mandatory pre-initialisation of output structure.
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    return PrnStatus::kInvalidPrn;
  }
  // Each Gold PRN is 2046 chips -> 256 bytes packed.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  out.data = std::span<const uint8_t>(&kGoldPrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 256);
  out.chip_length = kGoldChipLength;
  return PrnStatus::kOk;
}

PrnStatus weil10230_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    return PrnStatus::kInvalidPrn;
  }
  // Each Weil-10230 PRN is 10230 chips -> 1279 bytes packed.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  out.data = std::span<const uint8_t>(&kWeil10230PrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 1279);
  out.chip_length = kWeil10230ChipLength;
  return PrnStatus::kOk;
}

PrnStatus weil1500_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    return PrnStatus::kInvalidPrn;
  }
  // Each Weil-1500 PRN is 1500 chips -> 188 bytes packed.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  out.data = std::span<const uint8_t>(&kWeil1500PrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 188);
  out.chip_length = kWeil1500ChipLength;
  return PrnStatus::kOk;
}

} // namespace lunalink::signal
