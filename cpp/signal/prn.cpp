#include <cstddef>
#include <algorithm> // for std::fill

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"
#include "lunalink/signal/prn_table_weil10230.hpp"
#include "lunalink/signal/prn_table_weil1500.hpp"

namespace lunalink::signal {

PrnStatus gold_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  PrnStatus status = PrnStatus::kOk;
  // ESA Safety: Mandatory pre-initialisation of output structure.
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    status = PrnStatus::kInvalidPrn;
  } else {
    // Each Gold PRN is 2046 chips -> 256 bytes packed.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    out.data = std::span<const uint8_t>(&kGoldPrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 256);
    out.chip_length = kGoldChipLength;
  }
  return status;
}

PrnStatus weil10230_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  PrnStatus status = PrnStatus::kOk;
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    status = PrnStatus::kInvalidPrn;
  } else {
    // Each Weil-10230 PRN is 10230 chips -> 1279 bytes packed.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    out.data = std::span<const uint8_t>(&kWeil10230PrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 1279);
    out.chip_length = kWeil10230ChipLength;
  }
  return status;
}

PrnStatus weil1500_prn_packed(PrnId prn_id, PrnCode& out) noexcept {
  PrnStatus status = PrnStatus::kOk;
  out.data = {};
  out.chip_length = 0;

  if (!prn_id.valid()) [[unlikely]] {
    status = PrnStatus::kInvalidPrn;
  } else {
    // Each Weil-1500 PRN is 1500 chips -> 188 bytes packed.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    out.data = std::span<const uint8_t>(&kWeil1500PrnsPacked[static_cast<size_t>(prn_id) - 1U][0], 188);
    out.chip_length = kWeil1500ChipLength;
  }
  return status;
}

uint64_t weil10230_codebook_checksum() noexcept {
  uint64_t sum = 0;
  for (const auto& row : kWeil10230PrnsPacked) {
    for (const uint8_t val : row) {
      sum += val;
    }
  }
  return sum;
}

uint64_t weil1500_codebook_checksum() noexcept {
  uint64_t sum = 0;
  for (const auto& row : kWeil1500PrnsPacked) {
    for (const uint8_t val : row) {
      sum += val;
    }
  }
  return sum;
}

} // namespace lunalink::signal
