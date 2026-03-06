#include "lunalink/signal/tiered_code.hpp"

#include "lunalink/signal/prn.hpp"

namespace lunalink::signal {

TieredCodeStatus tiered_code_epoch_checked(const TieredCodeAssignment &assignment,
                                           uint16_t epoch_idx,
                                           uint8_t *out) noexcept {
  if (out == nullptr) {
    return TieredCodeStatus::kNullOutput;
  }
  if (epoch_idx >= kEpochsPerFrame) {
    return TieredCodeStatus::kInvalidEpoch;
  }
  if (!valid_tiered_assignment(assignment)) {
    return TieredCodeStatus::kInvalidAssignment;
  }

  const uint8_t *primary_packed = nullptr;
  if (weil10230_prn_packed(assignment.primary_prn, &primary_packed) !=
      PrnStatus::kOk) {
    return TieredCodeStatus::kInvalidAssignment;
  }
  const uint8_t *tertiary_packed = nullptr;
  if (weil1500_prn_packed(assignment.tertiary_prn, &tertiary_packed) !=
      PrnStatus::kOk) {
    return TieredCodeStatus::kInvalidAssignment;
  }

  // Secondary chip for this epoch (one secondary chip per primary epoch).
  const auto sec_idx = static_cast<uint32_t>(assignment.secondary_code_idx);
  const auto ep_idx  = static_cast<uint32_t>(epoch_idx);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  const uint8_t sec_chip = kSecondaryCodes[sec_idx][ep_idx % kSecondaryCodeLength];

  // Tertiary chip (one tertiary chip per Ns=4 primary epochs).
  const auto tert_idx = static_cast<uint16_t>(
      (static_cast<uint32_t>(assignment.tertiary_phase_offset) +
       static_cast<uint32_t>(epoch_idx / kSecondaryCodeLength)) %
      kWeil1500ChipLength);
  uint8_t tert_chip = 0;
  if (unpack_chip(tertiary_packed, tert_idx, kWeil1500ChipLength, &tert_chip) !=
      PrnStatus::kOk) {
    return TieredCodeStatus::kInvalidAssignment;
  }

  // For the entire epoch, sec_chip and tert_chip are constant, so the
  // modifier (sec XOR tert) is a single bit we XOR into every primary chip.
  const auto modifier = static_cast<uint8_t>(sec_chip ^ tert_chip);

  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t primary_chip = 0;
    if (unpack_chip(primary_packed, i, kWeil10230ChipLength, &primary_chip) !=
        PrnStatus::kOk) {
      return TieredCodeStatus::kInvalidAssignment;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out[i] = static_cast<uint8_t>(primary_chip ^ modifier);
  }
  return TieredCodeStatus::kOk;
}

TieredCodeStatus tiered_code_epoch(uint8_t prn_id, uint16_t epoch_idx,
                                   uint8_t *out) noexcept {
  TieredCodeAssignment assignment{};
  const auto assignment_status = default_tiered_assignment_checked(
      prn_id,
      &assignment);
  if (assignment_status != TieredCodeStatus::kOk) {
    return assignment_status;
  }
  return tiered_code_epoch_checked(assignment, epoch_idx, out);
}

} // namespace lunalink::signal
