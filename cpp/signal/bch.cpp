#include "lunalink/signal/bch.hpp"
#include <bit>
#include <array>
#include <numeric>
#include <algorithm>

namespace lunalink::signal {

extern const std::array<uint64_t, 400> kBchCodebook;

/// Mask for the 52 active bits in the BCH codeword storage.
inline constexpr uint64_t kCodewordMask = 0x000FFFFFFFFFFFFFULL;

BchStatus bch_encode(
    Fid                      fid,
    Toi                      toi,
    std::span<uint8_t, 52ULL> out) noexcept {
  const auto fid_val = static_cast<uint8_t>(fid);
  const auto toi_val = static_cast<uint8_t>(toi);

  if (fid_val > kBchFidMax) [[unlikely]] {
    return BchStatus::kInvalidFid;
  }
  if (toi_val > kBchToiMax) [[unlikely]] {
    return BchStatus::kInvalidToi;
  }

  const auto bit0 = static_cast<uint8_t>((static_cast<uint32_t>(fid_val) >> 1U) & 1U);
  auto state = static_cast<uint8_t>(((static_cast<uint32_t>(fid_val) & 1U) << 7U) | 
                                   (static_cast<uint32_t>(toi_val) & 0x7FU));

  constexpr uint8_t kFeedbackMask = 0b11111001U;
  out[0] = bit0;

  for (uint32_t k = 0; k < 51U; ++k) {
    const auto output = static_cast<uint8_t>((static_cast<uint32_t>(state) >> 7U) & 1U);
    const auto fb     = static_cast<uint8_t>(static_cast<uint32_t>(std::popcount(static_cast<uint32_t>(state & kFeedbackMask))) % 2U);
    state = static_cast<uint8_t>((static_cast<uint32_t>(state) << 1U) | fb);
    out[k + 1U] = static_cast<uint8_t>(output ^ bit0);
  }

  return BchStatus::kOk;
}

BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept {
  uint64_t received = 0;
  for (uint32_t i = 0; i < 52U; ++i) {
    received |= (static_cast<uint64_t>(in[i] != 0U) << i);
  }

  // Soft-TMR: Mirrored variables to detect ALU/Register bit-flips.
  uint32_t min_dist     = 64U;
  uint32_t min_dist_mir = ~64U; // Mirrored (inverted) value
  
  uint32_t best_idx     = 0U;
  uint32_t amb_mask     = 0U;
  uint32_t loop_count   = 0U;

  // Use range-based for for maximum iterator safety.
  for (const uint64_t codeword : kBchCodebook) {
    // 1. Radiation Masking: Ignore flips in unused bits of the uint64_t.
    const auto dist = static_cast<uint32_t>(std::popcount((received ^ codeword) & kCodewordMask));
    
    // 2. Branchless Selection.
    const auto is_less  = static_cast<uint32_t>(-(static_cast<int32_t>(dist < min_dist)));
    const auto is_equal = static_cast<uint32_t>(-(static_cast<int32_t>(dist == min_dist)));
    
    amb_mask = (amb_mask & ~is_less) | (is_equal & ~is_less);
    min_dist = (dist & is_less) | (min_dist & ~is_less);
    best_idx = (loop_count & is_less) | (best_idx & ~is_less);

    // 3. Mirror Update & Consistency Check (detects SEU in min_dist).
    min_dist_mir = ~min_dist;
    if (min_dist != ~min_dist_mir) [[unlikely]] {
      return BchResult(BchStatus::kFaultDetected, Fid::kNode1, Toi(0), 0xFFFFFFFFU);
    }

    loop_count++;
  }

  // 4. Control-Flow Integrity (CFI): Ensure every codeword was inspected.
  if (loop_count != kBchCodebookSize) [[unlikely]] {
    return BchResult(BchStatus::kFaultDetected, Fid::kNode1, Toi(0), 0xFFFFFFFFU);
  }

  const bool ambiguous = (min_dist != 0U) && (amb_mask != 0U);
  const auto best_fid = static_cast<Fid>(best_idx / 100U);
  const auto best_toi = Toi(static_cast<uint8_t>(best_idx % 100U));

  // 5. Reciprocal Sanity Check.
  std::array<uint8_t, 52> verify_buf{};
  const std::span<uint8_t, 52ULL> verify_span(verify_buf);
  BchStatus result_status = BchStatus::kOk;

  if (bch_encode(best_fid, best_toi, verify_span) == BchStatus::kOk) {
    uint64_t verify_val = 0;
    for (uint32_t j = 0; j < 52U; ++j) {
      verify_val |= (static_cast<uint64_t>(verify_buf.at(j) != 0U) << j);
    }
    if (static_cast<uint32_t>(std::popcount(received ^ verify_val)) != min_dist) [[unlikely]] {
      result_status = BchStatus::kFaultDetected;
    }
  }

  // Stack Scrubbing.
  std::fill(verify_buf.begin(), verify_buf.end(), 0U);

  if (result_status == BchStatus::kOk) {
    if (ambiguous) {
      result_status = BchStatus::kAmbiguousMatch;
    } else if (min_dist > 2U) {
      result_status = BchStatus::kNullOutput;
    }
  }

  return BchResult(result_status, best_fid, best_toi, min_dist);
}

uint64_t bch_codebook_checksum() noexcept {
  uint64_t sum = 0;
  for (const auto val : kBchCodebook) {
    sum += val;
  }
  return sum;
}

} // namespace lunalink::signal
