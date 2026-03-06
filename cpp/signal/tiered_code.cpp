#include "lunalink/signal/tiered_code.hpp"

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

bool tiered_code_epoch_checked(const TieredCodeAssignment &assignment,
                               uint16_t epoch_idx, uint8_t *out) noexcept {
  if (out == nullptr || epoch_idx >= kEpochsPerFrame ||
      !valid_tiered_assignment(assignment)) {
    return false;
  }

  const uint8_t *primary_packed = weil10230_prn_packed(assignment.primary_prn);
  const uint8_t *tertiary_packed = weil1500_prn_packed(assignment.tertiary_prn);

  // Secondary chip for this epoch (one secondary chip per primary epoch).
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  const uint8_t sec_chip = kSecondaryCodes[assignment.secondary_code_idx]
                                          [epoch_idx % kSecondaryCodeLength];

  // Tertiary chip (one tertiary chip per Ns=4 primary epochs).
  const auto tert_idx = static_cast<uint16_t>(
      (static_cast<uint16_t>(assignment.tertiary_phase_offset) +
       static_cast<uint16_t>(epoch_idx / kSecondaryCodeLength)) %
      kWeil1500ChipLength);
  const uint8_t tert_chip = unpack_chip(tertiary_packed, tert_idx);

  // For the entire epoch, sec_chip and tert_chip are constant, so the
  // modifier (sec XOR tert) is a single bit we XOR into every primary chip.
  const auto modifier = static_cast<uint8_t>(sec_chip ^ tert_chip);

  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out[i] = static_cast<uint8_t>(unpack_chip(primary_packed, i) ^ modifier);
  }
  return true;
}

void tiered_code_epoch(uint8_t prn_id, uint16_t epoch_idx, uint8_t *out) noexcept {
  if (prn_id < 1U || prn_id > kPrnCount || epoch_idx >= kEpochsPerFrame ||
      out == nullptr) {
    return;
  }
  const auto assignment = default_tiered_assignment(prn_id);
  (void)tiered_code_epoch_checked(assignment, epoch_idx, out);
}

} // namespace lunalink::signal
