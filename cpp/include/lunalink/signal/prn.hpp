#pragma once
#include <cstdint>
#include <span>
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

inline constexpr uint8_t  kPrnCount            = 210;
inline constexpr uint16_t kGoldChipLength      = 2046;
inline constexpr uint16_t kWeil10230ChipLength = 10230;
inline constexpr uint16_t kWeil1500ChipLength  = 1500;

/**
 * @brief Triple Modular Redundant (TMR) type for PRN Identifiers [1, 210].
 * Prevents accidental arithmetic or mixing with data bytes.
 * Implements 2-of-3 voting on access (ESA/NASA Mission Assurance Pattern).
 */
struct PrnId {
  TmrValue<CheckedRange<uint8_t, 1, kPrnCount>> storage{CheckedRange<uint8_t, 1, kPrnCount>{1U}};

  constexpr PrnId() noexcept = default;
  explicit constexpr PrnId(uint8_t v) noexcept : storage(CheckedRange<uint8_t, 1, kPrnCount>{v}) {}

  [[nodiscard]] constexpr uint8_t value() const noexcept {
    return static_cast<uint8_t>(storage.peek());
  }

  /**
   * @brief Perform active repair of the TMR triplets.
   * @return The repaired majority value.
   */
  uint8_t repair() const noexcept {
    return static_cast<uint8_t>(storage.vote());
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    const uint8_t v = value();
    return v >= 1U && v <= kPrnCount;
  }

  explicit constexpr operator uint8_t() const noexcept { return value(); }
  explicit constexpr operator size_t() const noexcept { return static_cast<size_t>(value()); }

  // NOLINTBEGIN(fuchsia-overloaded-operator)
  constexpr bool operator==(const PrnId& other) const noexcept {
    return value() == other.value();
  }
  constexpr auto operator<=>(const PrnId& other) const noexcept {
    return value() <=> other.value();
  }
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

/**
 * @brief Fault-tolerant status codes for PRN operations.
 * Non-contiguous bit patterns with high Hamming distance resist SEU bit-flips.
 */
enum class PrnStatus : uint8_t {
  kOk               = 0x5AU,  // 01011010
  kInvalidPrn       = 0xA5U,  // 10100101
  kNullOutput       = 0x33U,  // 00110011
  kInvalidChipIndex = 0xCCU,  // 11001100
  kFaultDetected    = 0x99U,  // 10011001
};

/// Get a view into the packed Gold-2046 code for a given PRN. [LSIS-AFS-101]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 2046
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus gold_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/// Get a view into the packed Weil-10230 code for a given PRN. [LSIS-AFS-102]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 10230
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus weil10230_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/// Get a view into the packed Weil-1500 code for a given PRN. [LSIS-AFS-102]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 1500
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard]] PrnStatus weil1500_prn_packed(
    PrnId    prn_id,
    PrnCode& out) noexcept;

/**
 * @brief Verify the integrity of the Weil-10230 codebook (Self-Test/CBIT). [CBIT-AFS-PRN]
 */
[[nodiscard]] uint64_t weil10230_codebook_checksum() noexcept;

/**
 * @brief Verify the integrity of the Weil-1500 codebook (Self-Test/CBIT). [CBIT-AFS-PRN]
 */
[[nodiscard]] uint64_t weil1500_codebook_checksum() noexcept;

/**
 * @brief Extract one chip from a packed PRN sequence (MSB-first). [LSIS-AFS-101]
 *
 * @param[in]  code      PRN code view.
 * @param[in]  chip_idx  Logical chip index [0, chip_length-1].
 * @param[out] out_chip  Reference to output chip. Cleared to 0 on failure.
 *
 * @pre chip_idx < code.chip_length
 * @post out_chip is 0 or 1 if kOk
 * @return PrnStatus::kOk or PrnStatus::kInvalidChipIndex
 */
[[nodiscard]] inline PrnStatus unpack_chip(
    const PrnCode& code,
    uint16_t       chip_idx,
    uint8_t&       out_chip) noexcept {
  
  PrnStatus status = PrnStatus::kOk;

  // ESA Safety: Force fail-safe initial state.
  out_chip = 0U;

  if (chip_idx >= code.chip_length || 
      static_cast<uint32_t>(chip_idx >> 3U) >= static_cast<uint32_t>(code.data.size())) [[unlikely]] {
    status = PrnStatus::kInvalidChipIndex;
  } else {
    const auto byte = static_cast<uint32_t>(code.data[chip_idx >> 3U]);
    out_chip = static_cast<uint8_t>((byte >> (7U - (chip_idx & 7U))) & 1U);
  }
  return status;
}

// Keep backward-compatible alias.
inline constexpr uint8_t kGoldPrnCount = kPrnCount;

} // namespace lunalink::signal
