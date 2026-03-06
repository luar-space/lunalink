#pragma once
#include <cstdint>
#include <span>

namespace lunalink::signal {

inline constexpr uint8_t  kPrnCount            = 210;
inline constexpr uint16_t kGoldChipLength      = 2046;
inline constexpr uint16_t kWeil10230ChipLength = 10230;
inline constexpr uint16_t kWeil1500ChipLength  = 1500;

/**
 * @brief Strong type for PRN Identifiers [1, 210].
 * Prevents accidental arithmetic or mixing with data bytes.
 */
struct PrnId {
  uint8_t value = 1U;

  constexpr PrnId() noexcept = default;
  explicit constexpr PrnId(uint8_t v) noexcept : value(v) {}

  [[nodiscard]] constexpr bool valid() const noexcept {
    return value >= 1U && value <= kPrnCount;
  }

  explicit constexpr operator uint8_t() const noexcept { return value; }
  explicit constexpr operator size_t() const noexcept { return static_cast<size_t>(value); }

  // NOLINTBEGIN(fuchsia-overloaded-operator)
  constexpr bool operator==(const PrnId&) const = default;
  constexpr auto operator<=>(const PrnId&) const = default;
  // NOLINTEND(fuchsia-overloaded-operator)
};

/**
 * @brief View into a PRN code sequence, bundling data with its chip count.
 * This ensures logical length (chips) and physical storage (span) are coupled.
 */
struct PrnCode {
  std::span<const uint8_t> data;
  uint16_t                 chip_length = 0;
};

/// Returns explicit status on input validation or processing errors.
enum class PrnStatus : uint8_t {
  kOk = 0,
  kInvalidPrn,
  kNullOutput,
  kInvalidChipIndex,
};

/// Get a view into the packed Gold-2046 code for a given PRN.
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 2046
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus gold_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/// Get a view into the packed Weil-10230 code for a given PRN.
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 10230
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus weil10230_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/// Get a view into the packed Weil-1500 code for a given PRN.
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 1500
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus weil1500_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/**
 * @brief Extract one chip from a packed PRN sequence (MSB-first).
 *
 * @param[in]  code      PRN code view.
 * @param[in]  chip_idx  Logical chip index [0, chip_length-1].
 * @param[out] out_chip  Pointer to output chip. Cleared to 0 on failure.
 *
 * @pre out_chip != nullptr
 * @pre chip_idx < code.chip_length
 * @post *out_chip is 0 or 1 if kOk
 * @return PrnStatus::kOk or PrnStatus::kInvalidChipIndex
 */
[[nodiscard]] inline PrnStatus unpack_chip(
    const PrnCode& code,
    uint16_t       chip_idx,
    uint8_t*       out_chip) noexcept {
  if (out_chip == nullptr) [[unlikely]] {
    return PrnStatus::kNullOutput;
  }
  // ESA Safety: Force fail-safe initial state.
  *out_chip = 0U;

  if (chip_idx >= code.chip_length) [[unlikely]] {
    return PrnStatus::kInvalidChipIndex;
  }
  if (static_cast<uint32_t>(chip_idx >> 3U) >= static_cast<uint32_t>(code.data.size())) [[unlikely]] {
    return PrnStatus::kInvalidChipIndex;
  }
  const auto byte = static_cast<uint32_t>(code.data[chip_idx >> 3U]);
  *out_chip = static_cast<uint8_t>((byte >> (7U - (chip_idx & 7U))) & 1U);
  return PrnStatus::kOk;
}

// Keep backward-compatible alias.
inline constexpr uint8_t kGoldPrnCount = kPrnCount;

} // namespace lunalink::signal
