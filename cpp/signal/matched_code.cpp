#include "lunalink/signal/matched_code.hpp"

#include "lunalink/signal/prn.hpp"
#include <algorithm> // for std::fill

namespace lunalink::signal {

namespace {
/// Derived constants for optimized byte-level processing.
// Weil-10230 is 10230 chips. 
// 10230 / 8 = 1278 bytes with 6 trailing chips.
constexpr uint32_t kWeil10230FullBytes = 1278U;
constexpr uint32_t kWeil10230TrailingChips = 6U;
} // namespace

/**
 * @brief Generate a 10230-chip epoch of the matched code. [LSIS-AFS-401]
 * Implements the combiner: Primary XOR Secondary XOR Tertiary.
 */
MatchedCodeStatus matched_code_epoch_checked(
    const MatchedCodeAssignment& assignment,
    uint16_t                    epoch_idx,
    std::span<uint8_t>          out) noexcept {
  
  MatchedCodeStatus status = MatchedCodeStatus::kOk;

  // ESA/JAXA Safety Policy: Invalidate output immediately to prevent stale data transmission
  // in case of early return or unhandled failure.
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), 0U);
  }

  if (out.size() < kWeil10230ChipLength) [[unlikely]] {
    status = MatchedCodeStatus::kOutputTooSmall;
  } else if (epoch_idx >= kEpochsPerFrame) [[unlikely]] {
    status = MatchedCodeStatus::kInvalidEpoch;
  } else if (!valid_matched_assignment(assignment)) [[unlikely]] {
    status = MatchedCodeStatus::kInvalidAssignment;
  }

  if (status == MatchedCodeStatus::kOk) {
    PrnCode primary_code;
    if (weil10230_prn_packed(assignment.primary_prn, primary_code) !=
        PrnStatus::kOk) [[unlikely]] {
      status = MatchedCodeStatus::kInvalidAssignment;
    }
    
    PrnCode tertiary_code;
    if (status == MatchedCodeStatus::kOk && weil1500_prn_packed(assignment.tertiary_prn, tertiary_code) !=
        PrnStatus::kOk) [[unlikely]] {
      status = MatchedCodeStatus::kInvalidAssignment;
    }

    if (status == MatchedCodeStatus::kOk) {
      // Secondary chip for this epoch (one secondary chip per primary epoch).
      const auto sec_idx = static_cast<uint32_t>(assignment.secondary_code_idx);
      const auto ep_idx  = static_cast<uint32_t>(epoch_idx);
      
      const uint8_t sec_chip = kSecondaryCodes.at(sec_idx).at(ep_idx % kSecondaryCodeLength);

      // Tertiary chip (one tertiary chip per Ns=4 primary epochs).
      const auto tert_idx = static_cast<uint16_t>(
          (static_cast<uint32_t>(assignment.tertiary_phase_offset) +
           static_cast<uint32_t>(epoch_idx / kSecondaryCodeLength)) %
          kWeil1500ChipLength);
      uint8_t tert_chip = 0;
      if (unpack_chip(tertiary_code, tert_idx, tert_chip) !=
          PrnStatus::kOk) [[unlikely]] {
        status = MatchedCodeStatus::kInvalidAssignment;
      }

      if (status == MatchedCodeStatus::kOk) {
        // For the entire epoch, sec_chip and tert_chip are constant, so the
        // modifier (sec XOR tert) is a single bit we XOR into every primary chip.
        const auto modifier = static_cast<uint8_t>(sec_chip ^ tert_chip);

        // OPTIMIZATION: Process primary PRN in packed bytes for massive speedup.
        const uint8_t byte_modifier = (modifier == 0) ? 0x00U : 0xFFU;
        
        uint32_t byte_count = 0;
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
            byte_count++;
        }
        
        // CFI: Control-Flow Integrity check for loop terminal count.
        if (byte_count != kWeil10230FullBytes) [[unlikely]] {
            status = MatchedCodeStatus::kFaultDetected;
        }

        if (status == MatchedCodeStatus::kOk) {
            // 2. Process remaining trailing chips.
            const auto last_byte = static_cast<uint8_t>(primary_code.data[kWeil10230FullBytes] ^ byte_modifier);
            for (uint32_t i = 0; i < kWeil10230TrailingChips; ++i) {
                out[(kWeil10230FullBytes << 3U) + i] = (last_byte >> (7U - i)) & 1U;
            }
        }
        // NOLINTEND(hicpp-signed-bitwise)
      }
    }
  }

  return status;
}

} // namespace lunalink::signal
