#include "lunalink/signal/matched_code.hpp"

#include "lunalink/signal/prn.hpp"
#include <algorithm> // for std::fill

namespace lunalink::signal {

namespace {
/// Derived constants for optimized byte-level processing.
/// We process as many full bytes as possible, then handle trailing chips.
constexpr uint32_t kWeil10230FullBytes = kWeil10230ChipLength / 8U; // 1278
constexpr uint32_t kWeil10230TrailingChips = kWeil10230ChipLength % 8U; // 6
static_assert(kWeil10230FullBytes * 8U + kWeil10230TrailingChips == kWeil10230ChipLength,
              "Weil-10230 byte-optimization constants are incorrect.");
} // namespace

MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    std::span<uint8_t>          out) noexcept {
  
  // ESA/JAXA Safety Policy: Invalidate output immediately to prevent stale data transmission
  // in case of early return or unhandled failure.
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), 0U);
  }

  if (out.size() < kWeil10230ChipLength) [[unlikely]] {
    return MatchedCodeStatus::kOutputTooSmall;
  }
  if (epoch_idx >= kEpochsPerFrame) [[unlikely]] {
    return MatchedCodeStatus::kInvalidEpoch;
  }
  if (!valid_matched_assignment(assignment)) [[unlikely]] {
    return MatchedCodeStatus::kInvalidAssignment;
  }

  PrnCode primary_code;
  if (weil10230_prn_packed(assignment.primary_prn, primary_code) !=
      PrnStatus::kOk) [[unlikely]] {
    return MatchedCodeStatus::kInvalidAssignment;
  }
  PrnCode tertiary_code;
  if (weil1500_prn_packed(assignment.tertiary_prn, tertiary_code) !=
      PrnStatus::kOk) [[unlikely]] {
    return MatchedCodeStatus::kInvalidAssignment;
  }

  // Secondary chip for this epoch (one secondary chip per primary epoch).
  const auto sec_idx = static_cast<uint32_t>(assignment.secondary_code_idx);
  const auto ep_idx  = static_cast<uint32_t>(epoch_idx);
  
  // Clang-Tidy safe access via .at() or just rely on our pre-validation.
  // Since we are in a performance critical path, but want to satisfy Tidy:
  const uint8_t sec_chip = kSecondaryCodes.at(sec_idx).at(ep_idx % kSecondaryCodeLength);

  // Tertiary chip (one tertiary chip per Ns=4 primary epochs).
  const auto tert_idx = static_cast<uint16_t>(
      (static_cast<uint32_t>(assignment.tertiary_phase_offset) +
       static_cast<uint32_t>(epoch_idx / kSecondaryCodeLength)) %
      kWeil1500ChipLength);
  uint8_t tert_chip = 0;
  if (unpack_chip(tertiary_code, tert_idx, &tert_chip) !=
      PrnStatus::kOk) [[unlikely]] {
    return MatchedCodeStatus::kInvalidAssignment;
  }

  // For the entire epoch, sec_chip and tert_chip are constant, so the
  // modifier (sec XOR tert) is a single bit we XOR into every primary chip.
  const auto modifier = static_cast<uint8_t>(sec_chip ^ tert_chip);

  // OPTIMIZATION: Process primary PRN in packed bytes for massive speedup.
  const uint8_t byte_modifier = (modifier == 0) ? 0x00U : 0xFFU;
  
  // 1. Process full bytes.
  // NOLINTBEGIN(hicpp-signed-bitwise)
  for (uint32_t b = 0; b < kWeil10230FullBytes; ++b) {
      const uint8_t p_byte = primary_code.data[b];
      const auto final_byte = static_cast<uint8_t>(p_byte ^ byte_modifier);
      
      // Unpack 8 chips to output.
      const uint32_t base = b << 3U;
      out[base + 0U] = (final_byte >> 7U) & 1U;
      out[base + 1U] = (final_byte >> 6U) & 1U;
      out[base + 2U] = (final_byte >> 5U) & 1U;
      out[base + 3U] = (final_byte >> 4U) & 1U;
      out[base + 4U] = (final_byte >> 3U) & 1U;
      out[base + 5U] = (final_byte >> 2U) & 1U;
      out[base + 6U] = (final_byte >> 1U) & 1U;
      out[base + 7U] = (final_byte >> 0U) & 1U;
  }
  
  // 2. Process remaining trailing chips.
  const auto last_byte = static_cast<uint8_t>(primary_code.data[kWeil10230FullBytes] ^ byte_modifier);
  for (uint32_t i = 0; i < kWeil10230TrailingChips; ++i) {
      out[(kWeil10230FullBytes << 3U) + i] = (last_byte >> (7U - i)) & 1U;
  }
  // NOLINTEND(hicpp-signed-bitwise)

  return MatchedCodeStatus::kOk;
}

MatchedCodeStatus matched_code_epoch(
    PrnId              prn_id,
    uint16_t           epoch_idx,
    std::span<uint8_t> out) noexcept {
  MatchedCodeAssignment assignment{};
  const auto assignment_status = default_matched_assignment_checked(
      prn_id,
      &assignment);
  if (assignment_status != MatchedCodeStatus::kOk) [[unlikely]] {
    // ESA/JAXA Policy: Ensure output is zeroed on failure.
    if (!out.empty()) {
        std::fill(out.begin(), out.end(), 0U);
    }
    return assignment_status;
  }
  return matched_code_epoch_checked(assignment, epoch_idx, out);
}

} // namespace lunalink::signal
