// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/core/safety.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

inline constexpr uint8_t kPrnCount = 210;
inline constexpr uint16_t kGoldChipLength = 2046;
inline constexpr uint16_t kWeil10230ChipLength = 10230;
inline constexpr uint16_t kWeil1500ChipLength = 1500;

/// S-band center frequency (Hz). LSIS V1.0 §2.3.
inline constexpr double kCenterFrequencyHz = 2491.75e6;
/// AFS-I Gold code chip rate (Hz). LSIS V1.0 §2.3.
inline constexpr double kAfsIChipRateHz = 1.023e6;
/// AFS-Q Weil code chip rate (Hz). LSIS V1.0 §2.3.
inline constexpr double kAfsQChipRateHz = 5.115e6;

/// Tag type for nominal type safety via TmrCheckedId.
struct PrnIdTag {};

/**
 * @brief TMR-protected PRN Identifier [1, 210].
 * Prevents accidental arithmetic or mixing with data bytes.
 * Extends TmrCheckedId with size_t conversion and three-way comparison.
 */
struct PrnId : TmrCheckedId<1, kPrnCount, PrnIdTag> {
  using TmrCheckedId::TmrCheckedId;

  explicit constexpr operator size_t() const noexcept {
    return static_cast<size_t>(value());
  }

  // Value-type three-way comparison required — NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr auto operator<=>(const PrnId& other) const noexcept {
    return value() <=> other.value();
  }
};

/**
 * @brief View into a PRN code sequence, bundling data with its chip count.
 * This ensures logical length (chips) and physical storage (span) are coupled.
 */
struct PrnCode {
  std::span<const uint8_t> data;
  uint16_t chip_length = 0;
};

/** @brief Status codes for PRN code generation and lookup (Gold, Weil-10230, Weil-1500).
 * SEU-hardened with high Hamming distance bit patterns. */
enum class PrnStatus : uint8_t {
  kOk = 0x5AU,               ///< Operation completed successfully.
  kInvalidPrn = 0xA5U,       ///< PRN ID out of valid range [1, 210].
  kNullOutput = 0x33U,       ///< Required buffer is null or empty.
  kInvalidChipIndex = 0xCCU, ///< Chip index exceeds code length.
  kFaultDetected = 0x99U,    ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// Get a view into the packed Gold-2046 code for a given PRN. [LSIS-AFS-101]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 2046
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard("PrnStatus reports kInvalidPrn or kFaultDetected; ignoring it leaves `out` "
            "pointing at zero-length data and lets a missing PRN propagate as if valid "
            "(LSIS-AFS-101)")]] PrnStatus
gold_prn_packed(PrnId prn_id, PrnCode& out) noexcept;

/// Get a view into the packed Weil-10230 code for a given PRN. [LSIS-AFS-102]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 10230
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard("PrnStatus reports kInvalidPrn or kFaultDetected; ignoring it leaves `out` "
            "pointing at zero-length data and lets a missing PRN propagate as if valid "
            "(LSIS-AFS-102)")]] PrnStatus
weil10230_prn_packed(PrnId prn_id, PrnCode& out) noexcept;

/// Get a view into the packed Weil-1500 code for a given PRN. [LSIS-AFS-102]
///
/// @pre prn_id.valid() == true
/// @post out.chip_length == 1500
/// @return PrnStatus::kOk or PrnStatus::kInvalidPrn
[[nodiscard("PrnStatus reports kInvalidPrn or kFaultDetected; ignoring it leaves `out` "
            "pointing at zero-length data and lets a missing PRN propagate as if valid "
            "(LSIS-AFS-102)")]] PrnStatus
weil1500_prn_packed(PrnId prn_id, PrnCode& out) noexcept;

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
[[nodiscard("PrnStatus reports kInvalidChipIndex; ignoring it leaves out_chip set to 0 "
            "(fail-safe) but lets the caller proceed as if the chip were valid "
            "(LSIS-AFS-101)")]] inline PrnStatus
unpack_chip(const PrnCode& code, uint16_t chip_idx, uint8_t& out_chip) noexcept {
  PrnStatus status = PrnStatus::kOk;

  // ESA Safety: Force fail-safe initial state.
  out_chip = 0U;

  if (chip_idx >= code.chip_length || (chip_idx >> 3U) >= code.data.size()) [[unlikely]] {
    status = PrnStatus::kInvalidChipIndex;
  } else {
    const auto byte = static_cast<uint32_t>(code.data[chip_idx >> 3U]);
    out_chip = static_cast<uint8_t>((byte >> (7U - (chip_idx & 7U))) & 1U);
  }
  return status;
}

} // namespace lunalink::signal
