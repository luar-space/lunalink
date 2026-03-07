#pragma once
#include <cstdint>
#include <span>
#include <array>

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

enum class MatchedCodeStatus : uint8_t {
  kOk = 0,
  kInvalidPrn,
  kInvalidEpoch,
  kOutputTooSmall,
  kInvalidAssignment,
};

inline constexpr uint16_t kEpochsPerFrame = 6000;
inline constexpr uint8_t  kSecondaryCodeLength = 4;
inline constexpr uint8_t  kSecondaryCodeCount = 4;
inline constexpr uint8_t  kInterimAssignmentMaxPrn = 12;

/// Secondary codes S0-S3 (LSIS Table 10).
inline constexpr std::array<std::array<uint8_t, 4>, 4> kSecondaryCodes = {{
    {1, 1, 1, 0}, // S0
    {0, 1, 1, 1}, // S1
    {1, 0, 1, 1}, // S2
    {1, 1, 0, 1}, // S3
}};

/// Structure defining the matched-code construction parameters for a given PRN.
struct MatchedCodeAssignment {
  PrnId    primary_prn;
  uint8_t  secondary_code_idx = 0;    ///< 0-3 index into S0-S3
  PrnId    tertiary_prn;
  uint16_t tertiary_phase_offset = 0; ///< Offset in chips (0-1499)
};

/// Validate if an assignment struct contains valid IDs and indices.
[[nodiscard]] inline constexpr bool valid_matched_assignment(
    const MatchedCodeAssignment& assignment) {
  return assignment.primary_prn.valid() && assignment.tertiary_prn.valid() &&
         assignment.secondary_code_idx < 4 &&
         assignment.tertiary_phase_offset < 1500;
}

[[nodiscard]] inline constexpr bool is_interim_prn(PrnId prn_id) noexcept {
  return prn_id.value >= 1 && prn_id.value <= kInterimAssignmentMaxPrn;
}

[[nodiscard]] inline constexpr MatchedCodeStatus secondary_code_index_checked(
    PrnId    prn_id,
    uint8_t* out_idx) noexcept {
  if (out_idx == nullptr) {
    return MatchedCodeStatus::kInvalidAssignment;
  }
  if (!is_interim_prn(prn_id)) {
    return MatchedCodeStatus::kInvalidPrn;
  }
  // Interim mapping: (PRN-1) % 4
  *out_idx = static_cast<uint8_t>((prn_id.value - 1U) % 4U);
  return MatchedCodeStatus::kOk;
}

/// Retrieve the default interim assignment for a PRN ID (1-12 only).
/// Per LSIS Table 11.
[[nodiscard]] constexpr MatchedCodeStatus default_matched_assignment_checked(
    PrnId                  prn_id,
    MatchedCodeAssignment* out_assignment) {
  if (out_assignment == nullptr) {
    return MatchedCodeStatus::kInvalidAssignment;
  }
  if (!is_interim_prn(prn_id)) {
    return MatchedCodeStatus::kInvalidPrn;
  }
  uint8_t secondary_idx = 0;
  const auto secondary_status = secondary_code_index_checked(
      prn_id,
      &secondary_idx);
  if (secondary_status != MatchedCodeStatus::kOk) {
    return secondary_status;
  }
  *out_assignment = MatchedCodeAssignment{
      .primary_prn           = prn_id,
      .secondary_code_idx    = secondary_idx,
      .tertiary_prn          = prn_id, // Interim: Tertiary = Primary
      .tertiary_phase_offset = 0       // Interim: Phase = 0
  };
  return MatchedCodeStatus::kOk;
}

/// Checked version of matched_code_epoch using a specific assignment.
/// @pre valid_matched_assignment(assignment)
[[nodiscard]] MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    std::span<uint8_t>          out) noexcept;

/// Generate one primary epoch (10 230 chips) of the matched AFS-Q code.
/// Uses the default interim assignment.
/// @post out is populated with valid matched chips if kOk
[[nodiscard]] MatchedCodeStatus matched_code_epoch(
    PrnId              prn_id,
    uint16_t           epoch_idx,
    std::span<uint8_t> out) noexcept;

// Ergonomic overloads
template <typename T>
[[nodiscard]] inline MatchedCodeStatus matched_code_epoch(
    PrnId    prn_id,
    uint16_t epoch_idx,
    T&&      out) noexcept {
  return matched_code_epoch(prn_id, epoch_idx, std::span<uint8_t>(out));
}

template <typename T>
[[nodiscard]] inline MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    T&&                         out) noexcept {
  return matched_code_epoch_checked(assignment, epoch_idx, std::span<uint8_t>(out));
}

} // namespace lunalink::signal
