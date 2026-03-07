#pragma once
#include <cstdint>
#include <span>
#include <array>

#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for Matched Code operations.
 * Non-contiguous bit patterns with high Hamming distance resist SEU bit-flips.
 */
enum class MatchedCodeStatus : uint8_t {
  kOk                = 0x5AU,  // 01011010
  kInvalidPrn        = 0xA5U,  // 10100101
  kInvalidEpoch      = 0x33U,  // 00110011
  kOutputTooSmall    = 0xCCU,  // 11001100
  kInvalidAssignment = 0x0FU,  // 00001111
  kFaultDetected     = 0x99U,  // 10011001
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
/// Uses CheckedRange to ensure invariants are enforced at the type level (Mission Assurance Pattern).
struct MatchedCodeAssignment {
  PrnId    primary_prn;
  CheckedRange<uint8_t, 0, 3> secondary_code_idx{0};    ///< 0-3 index into S0-S3
  PrnId    tertiary_prn;
  CheckedRange<uint16_t, 0, 1499> tertiary_phase_offset{0}; ///< Offset in chips (0-1499)
};

/// Validate if an assignment struct contains valid IDs and indices.
[[nodiscard]] inline constexpr bool valid_matched_assignment(
    const MatchedCodeAssignment& assignment) {
  return assignment.primary_prn.valid() &&
         assignment.secondary_code_idx < 4 &&
         assignment.tertiary_prn.valid() &&
         assignment.tertiary_phase_offset < 1500;
}

[[nodiscard]] inline constexpr bool is_interim_prn(PrnId prn_id) noexcept {
  return prn_id.value() >= 1 && prn_id.value() <= kInterimAssignmentMaxPrn;
}

[[nodiscard]] inline constexpr MatchedCodeStatus secondary_code_index_checked(
    PrnId    prn_id,
    uint8_t& out_idx) noexcept {
  
  MatchedCodeStatus status = MatchedCodeStatus::kOk;

  if (!is_interim_prn(prn_id)) {
    status = MatchedCodeStatus::kInvalidPrn;
    out_idx = 0U;
  } else {
    // Interim mapping: (PRN-1) % 4
    out_idx = static_cast<uint8_t>((prn_id.value() - 1U) % 4U);
  }
  return status;
}

/// Retrieve the default interim assignment for a PRN ID (1-12 only). [LSIS-AFS-401]
/// Per LSIS Table 11.
[[nodiscard]] inline constexpr MatchedCodeStatus default_matched_assignment_checked(
    PrnId                   prn_id,
    MatchedCodeAssignment& out_assignment) noexcept {
  
  uint8_t secondary_idx = 0;
  const auto status = secondary_code_index_checked(prn_id, secondary_idx);
  
  if (status == MatchedCodeStatus::kOk) {
    out_assignment = MatchedCodeAssignment{
        .primary_prn           = prn_id,
        .secondary_code_idx    = CheckedRange<uint8_t, 0, 3>{secondary_idx},
        .tertiary_prn          = prn_id, // Interim: Tertiary = Primary
        .tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>{0} // Interim: Phase = 0
    };
  }
  return status;
}

/// Checked version of matched_code_epoch using a specific assignment. [LSIS-AFS-401]
[[nodiscard]] MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    std::span<uint8_t>          out) noexcept;

/// Get a single primary epoch of the AFS-Q pilot code using default assignments. [LSIS-AFS-401]
[[nodiscard]] inline MatchedCodeStatus matched_code_epoch(
    PrnId              prn_id,
    uint16_t           epoch_idx,
    std::span<uint8_t> out) noexcept {
  
  MatchedCodeAssignment assignment;
  const auto assignment_status = default_matched_assignment_checked(prn_id, assignment);
  
  MatchedCodeStatus status = MatchedCodeStatus::kOk;

  if (assignment_status != MatchedCodeStatus::kOk) {
    // ESA Safety Policy: Ensure output is zeroed on failure.
    if (!out.empty()) {
        std::fill(out.begin(), out.end(), 0U);
    }
    status = assignment_status;
  } else {
    status = matched_code_epoch_checked(assignment, epoch_idx, out);
  }
  return status;
}

/**
 * @brief Ergonomic template overload for any container with data() and size().
 */
template <typename T>
[[nodiscard]] inline MatchedCodeStatus matched_code_epoch(
    PrnId              prn_id,
    uint16_t           epoch_idx,
    T&&                out) noexcept {
  return matched_code_epoch(prn_id, epoch_idx, std::span<uint8_t>(out));
}

/**
 * @brief Ergonomic template overload for any container with data() and size().
 */
template <typename T>
[[nodiscard]] inline MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    T&&                         out) noexcept {
  return matched_code_epoch_checked(assignment, epoch_idx, std::span<uint8_t>(out));
}

} // namespace lunalink::signal
