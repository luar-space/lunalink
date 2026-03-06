#pragma once
#include <cassert>
#include <cstdint>

namespace lunalink::signal {

inline constexpr uint8_t  kPrnCount            = 210;
inline constexpr uint16_t kGoldChipLength      = 2046;
inline constexpr uint16_t kWeil10230ChipLength = 10230;
inline constexpr uint16_t kWeil1500ChipLength  = 1500;

/// Extract a single chip from a packed byte array (MSB-first bit ordering).
/// Returns 0 or 1.
/// Precondition: packed != nullptr, chip_idx < total chip count for the array.
inline uint8_t unpack_chip(const uint8_t* packed, uint16_t chip_idx) noexcept {
  assert(packed != nullptr);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto byte = static_cast<unsigned>(packed[chip_idx >> 3U]);
  return static_cast<uint8_t>((byte >> (7U - (chip_idx & 7U))) & 1U);
}

/// Returns pointer to the packed Gold-2046 bytes for prn_id (1-indexed).
/// Use unpack_chip() to extract individual chips.
/// Precondition: prn_id in [1, 210]. Validated by the binding layer; UB otherwise.
[[nodiscard]] const uint8_t* gold_prn_packed(uint8_t prn_id) noexcept;

/// Returns pointer to the packed Weil-10230 bytes for prn_id (1-indexed).
[[nodiscard]] const uint8_t* weil10230_prn_packed(uint8_t prn_id) noexcept;

/// Returns pointer to the packed Weil-1500 bytes for prn_id (1-indexed).
[[nodiscard]] const uint8_t* weil1500_prn_packed(uint8_t prn_id) noexcept;

// Keep backward-compatible alias.
inline constexpr uint8_t kGoldPrnCount = kPrnCount;

} // namespace lunalink::signal
