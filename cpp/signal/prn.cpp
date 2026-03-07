#include <algorithm>  // for std::fill
#include <cstddef>

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"
#include "lunalink/signal/prn_table_weil10230.hpp"
#include "lunalink/signal/prn_table_weil1500.hpp"
#include "lunalink/signal/safety.hpp"

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
    const size_t idx = static_cast<size_t>(prn_id.repair()) - 1U;
    out.data = std::span(kGoldPrnsPacked.at(idx));
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
    const size_t idx = static_cast<size_t>(prn_id.repair()) - 1U;
    out.data = std::span(kWeil10230PrnsPacked.at(idx));
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
    const size_t idx = static_cast<size_t>(prn_id.repair()) - 1U;
    out.data = std::span(kWeil1500PrnsPacked.at(idx));
    out.chip_length = kWeil1500ChipLength;
  }
  return status;
}

uint64_t weil10230_codebook_checksum() noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  for (const auto& row : kWeil10230PrnsPacked) {
    for (const uint8_t val : row) {
      crc ^= val;
      for (uint32_t i = 0; i < 8U; i++) {
        crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
      }
      wip_tick();
    }
  }
  return ~(static_cast<uint64_t>(crc));
}

uint64_t weil1500_codebook_checksum() noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  for (const auto& row : kWeil1500PrnsPacked) {
    for (const uint8_t val : row) {
      crc ^= val;
      for (uint32_t i = 0; i < 8U; i++) {
        crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
      }
      wip_tick();
    }
  }
  return ~(static_cast<uint64_t>(crc));
}

}  // namespace lunalink::signal
