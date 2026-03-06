#pragma once
#include <cstdint>

namespace lunalink::signal {

inline constexpr uint8_t  kPrnCount            = 210;
inline constexpr uint16_t kGoldChipLength      = 2046;
inline constexpr uint16_t kWeil10230ChipLength = 10230;
inline constexpr uint16_t kWeil1500ChipLength  = 1500;

enum class PrnStatus : uint8_t {
  kOk = 0,
  kInvalidPrn,
  kNullInput,
  kNullOutput,
  kInvalidChipIndex,
};

/// Returns pointer to the packed Gold-2046 bytes for prn_id (1-indexed).
[[nodiscard]] PrnStatus gold_prn_packed(
    uint8_t          prn_id,
    const uint8_t**  out_packed) noexcept;

/// Returns pointer to the packed Weil-10230 bytes for prn_id (1-indexed).
[[nodiscard]] PrnStatus weil10230_prn_packed(
    uint8_t          prn_id,
    const uint8_t**  out_packed) noexcept;

/// Returns pointer to the packed Weil-1500 bytes for prn_id (1-indexed).
[[nodiscard]] PrnStatus weil1500_prn_packed(
    uint8_t          prn_id,
    const uint8_t**  out_packed) noexcept;

/// Extract one chip from a packed PRN sequence (MSB-first).
[[nodiscard]] inline PrnStatus unpack_chip(
    const uint8_t* packed,
    uint16_t       chip_idx,
    uint16_t       chip_count,
    uint8_t*       out_chip) noexcept {
  if (packed == nullptr) {
    return PrnStatus::kNullInput;
  }
  if (out_chip == nullptr) {
    return PrnStatus::kNullOutput;
  }
  if (chip_idx >= chip_count) {
    return PrnStatus::kInvalidChipIndex;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto byte = static_cast<unsigned>(packed[chip_idx >> 3U]);
  *out_chip = static_cast<uint8_t>((byte >> (7U - (chip_idx & 7U))) & 1U);
  return PrnStatus::kOk;
}

// Keep backward-compatible alias.
inline constexpr uint8_t kGoldPrnCount = kPrnCount;

} // namespace lunalink::signal
