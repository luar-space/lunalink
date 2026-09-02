// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/matched_code.hpp"

#include "lunalink/codes/prn_codes.hpp"
#include "lunalink/core/safety.hpp"

namespace lunalink::signal {

namespace {
/// Derived constants for optimized byte-level processing.
// Weil-10230 is 10230 chips.
// 10230 / 8 = 1278 bytes with 6 trailing chips.
constexpr uint32_t kWeil10230FullBytes = 1278U;
constexpr uint32_t kWeil10230TrailingChips = 6U;

// Result of PRN code generation and modifier computation.
struct MatchedCodeGenResult {
  PrnCode primary;
  uint8_t modifier = 0;
  bool ok = false;
};

// Generate primary code and compute sec^tert modifier for a given epoch.
[[nodiscard]] MatchedCodeGenResult generate_matched_codes(const MatchedCodeAssignment& assignment,
                                                          uint16_t epoch_idx) noexcept {
  MatchedCodeGenResult res{};

  if (weil10230_prn_packed(assignment.primary_prn, res.primary) != PrnStatus::kOk) {
    return res; // NOCOVER(DEFENCE_IN_DEPTH): TMR vote fault on PRN ID
  }

  PrnCode tertiary_code;
  if (weil1500_prn_packed(assignment.tertiary_prn, tertiary_code) != PrnStatus::kOk) {
    return res; // NOCOVER(DEFENCE_IN_DEPTH): CFI counter mismatch
  }

  const auto sec_idx = static_cast<uint32_t>(assignment.secondary_code_idx);
  const auto ep_idx = static_cast<uint32_t>(epoch_idx);
  const uint8_t sec_chip = kSecondaryCodes.at(sec_idx).at(ep_idx % kSecondaryCodeLength);

  const auto tert_idx =
      static_cast<uint16_t>((static_cast<uint32_t>(assignment.tertiary_phase_offset) +
                             static_cast<uint32_t>(epoch_idx / kSecondaryCodeLength)) %
                            kWeil1500ChipLength);
  uint8_t tert_chip = 0;
  if (unpack_chip(tertiary_code, tert_idx, tert_chip) != PrnStatus::kOk) {
    return res; // NOCOVER(DEFENCE_IN_DEPTH): code generation failure path
  }

  res.modifier = static_cast<uint8_t>(sec_chip ^ tert_chip);
  res.ok = true;
  return res;
}

// Apply sec^tert modifier to primary code and write unpacked chips to output.
// Returns kOk on success, kFaultDetected on CFI failure.
[[nodiscard]] MatchedCodeStatus apply_modifier_to_output(const PrnCode& primary_code,
                                                         uint8_t modifier,
                                                         std::span<uint8_t> out) noexcept {
  const uint8_t byte_modifier = (modifier == 0) ? 0x00U : 0xFFU;

  volatile uint32_t byte_count = 0;
  // NOLINTBEGIN(hicpp-signed-bitwise) -- bit shifts on unsigned values promoted from
  // uint8_t/uint32_t
  for (uint32_t b = 0; b < kWeil10230FullBytes; ++b) {
    const uint8_t p_byte = primary_code.data[b];
    const auto final_byte = static_cast<uint8_t>(p_byte ^ byte_modifier);

    const uint32_t base = b << 3U;
    out[base + 0U] = (final_byte >> 7U) & 1U;
    out[base + 1U] = (final_byte >> 6U) & 1U;
    out[base + 2U] = (final_byte >> 5U) & 1U;
    out[base + 3U] = (final_byte >> 4U) & 1U;
    out[base + 4U] = (final_byte >> 3U) & 1U;
    out[base + 5U] = (final_byte >> 2U) & 1U;
    out[base + 6U] = (final_byte >> 1U) & 1U;
    out[base + 7U] = (final_byte >> 0U) & 1U;
    LUNALINK_CRITICAL_ENTER;
    byte_count = byte_count + 1U;
    LUNALINK_CRITICAL_EXIT;
    wip_tick();
  }

  if (byte_count != kWeil10230FullBytes) {
    // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CFI terminal check
    return MatchedCodeStatus::kFaultDetected; // FIZ-1
  } // NOCOVER_END

  const auto last_byte =
      static_cast<uint8_t>(primary_code.data[kWeil10230FullBytes] ^ byte_modifier);
  for (uint32_t i = 0; i < kWeil10230TrailingChips; ++i) {
    out[(kWeil10230FullBytes << 3U) + i] = (last_byte >> (7U - i)) & 1U;
    wip_tick();
  }
  // NOLINTEND(hicpp-signed-bitwise)

  return MatchedCodeStatus::kOk;
}

} // namespace

/**
 * @brief Generate a 10230-chip epoch of the matched code. [LSIS-AFS-401]
 * Implements the combiner: Primary XOR Secondary XOR Tertiary.
 */
MatchedCodeStatus matched_code_epoch_checked(const MatchedCodeAssignment& assignment,
                                             uint16_t epoch_idx,
                                             std::span<uint8_t> out) noexcept {
  MatchedCodeStatus status = MatchedCodeStatus::kOk;

  if (out.size() < kWeil10230ChipLength) {
    status = MatchedCodeStatus::kOutputTooSmall;
  } else if (epoch_idx >= kEpochsPerFrame) {
    status = MatchedCodeStatus::kInvalidEpoch;
  } else if (!valid_matched_assignment(assignment)) {
    status = MatchedCodeStatus::kInvalidAssignment;
  }

  if (status == MatchedCodeStatus::kOk) {
    const auto gen = generate_matched_codes(assignment, epoch_idx);
    if (!gen.ok) {
      status = MatchedCodeStatus::kInvalidAssignment; // NOCOVER(DEFENCE_IN_DEPTH): modifier
                                                      // application fault
    } else {
      status = apply_modifier_to_output(gen.primary, gen.modifier, out);
    }
  }

  if (status != MatchedCodeStatus::kOk) {
    secure_scrub(out);
  }

  return status;
}

} // namespace lunalink::signal
