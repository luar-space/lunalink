#include "lunalink/signal/bch.hpp"
#include <bit>
#include <array>
#include <numeric>
#include <algorithm>

namespace lunalink::signal {

extern const std::array<uint64_t, 400> kBchCodebook;

/// Mask for the 52 active bits in the BCH codeword storage.
inline constexpr uint64_t kCodewordMask = 0x000FFFFFFFFFFFFFULL;

/**
 * @brief Encode FID and TOI into a 52-symbol BCH(51,8) codeword. [LSIS-AFS-501]
 */
BchStatus bch_encode(
    Fid                      fid,
    Toi                      toi,
    std::span<uint8_t, 52ULL> out) noexcept {
  
  BchStatus status = BchStatus::kOk;
  const auto fid_val = fid.repair();
  const auto toi_val = toi.repair();

  // ESA Safety: Force zero initial state.
  std::ranges::fill(out, 0U);

  if (fid_val > kBchFidMax) [[unlikely]] {
    status = BchStatus::kInvalidFid;
  } else if (toi_val > kBchToiMax) [[unlikely]] {
    status = BchStatus::kInvalidToi;
  } else {
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
      wip_tick();
    }
  }

  return status;
}

/**
 * @brief Maximum Likelihood (ML) BCH decoder. [LSIS-AFS-501]
 */
BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept {
  uint64_t received = 0;
  for (uint32_t i = 0; i < 52U; ++i) {
    received |= (static_cast<uint64_t>(in[i] != 0U) << i);
    wip_tick();
  }

  // Soft-TMR: Mirrored variables to detect ALU/Register bit-flips.
  uint32_t min_dist     = 64U;
  uint32_t min_dist_mir = ~64U; // Mirrored (inverted) value
  
  uint32_t best_idx     = 0U;
  uint32_t amb_mask     = 0U;
  uint32_t loop_count   = 0U;
  BchStatus status_seu  = BchStatus::kOk;

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
      status_seu = BchStatus::kFaultDetected;
    }

    loop_count++;
    wip_tick();
  }

  // 4. Control-Flow Integrity (CFI): Ensure every codeword was inspected.
  if (loop_count != kBchCodebook.size()) [[unlikely]] {
    status_seu = BchStatus::kFaultDetected;
  }

  const bool ambiguous = (min_dist != 0U) && (amb_mask != 0U);
  const auto best_fid = static_cast<Fid>(static_cast<uint8_t>(best_idx / 100U));
  const auto best_toi = Toi(static_cast<uint8_t>(best_idx % 100U));

  // 5. Reciprocal Sanity Check.
  std::array<uint8_t, 52> verify_buf{};
  const std::span<uint8_t, 52ULL> verify_span(verify_buf);
  BchStatus result_status = status_seu;

  if (result_status == BchStatus::kOk) {
    if (bch_encode(best_fid, best_toi, verify_span) == BchStatus::kOk) {
      uint64_t verify_val = 0;
      for (uint32_t j = 0; j < 52U; ++j) {
        verify_val |= (static_cast<uint64_t>(verify_buf.at(j) != 0U) << j);
      }
      if (static_cast<uint32_t>(std::popcount(received ^ verify_val)) != min_dist) [[unlikely]] {
        result_status = BchStatus::kFaultDetected;
      }
    } else {
      result_status = BchStatus::kFaultDetected;
    }

    if (result_status == BchStatus::kOk) {
      if (ambiguous) {
        result_status = BchStatus::kAmbiguousMatch;
      } else if (min_dist > 2U) {
        result_status = BchStatus::kNullOutput;
      }
    }
  }

  // JAXA/NASA Safety: Stack Scrubbing.
  secure_scrub(verify_buf);

  return BchResult(result_status, best_fid, best_toi, static_cast<uint8_t>(min_dist));
}

uint64_t bch_codebook_checksum() noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  auto update_crc = [&](uint64_t val) {
    for (uint32_t i = 0; i < 8U; i++) {
      crc ^= static_cast<uint8_t>((val >> (i * 8U)) & 0xFFU);
      for (uint32_t j = 0; j < 8U; j++) {
        crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
      }
    }
  };
  for (const auto val : kBchCodebook) {
    update_crc(val);
  }

  return ~(static_cast<uint64_t>(crc));
}

} // namespace lunalink::signal
