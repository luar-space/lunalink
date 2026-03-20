#include "lunalink/encoding/bch.hpp"

#include "lunalink/core/crc32.hpp"
#include "lunalink/encoding/bch_table.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace lunalink::signal {

namespace {
/// Mask for the 52 active bits in the BCH codeword storage.
inline constexpr uint64_t kCodewordMask = 0x000FFFFFFFFFFFFFULL;

/// Number of TOI values: kBchToiMax + 1 (used as FID/TOI index stride).
constexpr uint32_t kBchToiCount = kBchToiMax + 1U;

// Hard ML search result.
struct BchHardSearchResult {
  uint32_t best_idx = 0;
  uint32_t min_dist = 64;
  uint32_t amb_mask = 0;
  bool cfi_ok = false;
};

// Hard ML search: find minimum Hamming distance codeword.
BchHardSearchResult bch_hard_search(uint64_t received) noexcept {
  BchHardSearchResult res{};
  uint32_t cfi_count = 0xA5A5A5A5U;

  for (uint32_t i = 0; i < kBchCodebook.size(); ++i) {
    const uint64_t codeword = kBchCodebook.at(i);
    const auto dist = static_cast<uint32_t>(std::popcount((received ^ codeword) & kCodewordMask));

    bool is_new_min = (dist < res.min_dist);
    res.amb_mask = is_new_min ? 0U : (dist == res.min_dist ? 1U : res.amb_mask);
    res.best_idx = is_new_min ? i : res.best_idx;
    res.min_dist = is_new_min ? dist : res.min_dist;

    cfi_count++;
    wip_tick();
  }
  res.cfi_ok = (cfi_count == (0xA5A5A5A5U + static_cast<uint32_t>(kBchCodebook.size())));
  return res;
}

// Soft ML search result.
struct BchSoftSearchResult {
  uint32_t best_idx = 0;
  uint32_t best_idx_mir = ~0U;
  float best_metric = -3.4e+38F;
  bool cfi_ok = false;
  bool mirror_ok = false;
};

// Soft ML search: find maximum correlation codeword.
BchSoftSearchResult bch_soft_search(std::span<const float, 52ULL> llr) noexcept {
  constexpr float kLlrClamp = 100.0F;
  BchSoftSearchResult res{};
  uint32_t cfi_count = 0xA5A5A5A5U;

  for (uint32_t i = 0; i < kBchCodebook.size(); ++i) {
    const uint64_t codeword = kBchCodebook.at(i);
    float metric = 0.0F;
    for (uint32_t j = 0; j < 52U; ++j) {
      const float s = 1.0F - 2.0F * static_cast<float>((codeword >> j) & 1ULL);
      const float cl = std::clamp(llr[j], -kLlrClamp, kLlrClamp);
      metric += cl * s;
      wip_tick();
    }
    if (metric > res.best_metric) {
      res.best_metric = metric;
      res.best_idx = i;
      res.best_idx_mir = ~i;
    }
    cfi_count++;
  }

  res.cfi_ok = (cfi_count == (0xA5A5A5A5U + static_cast<uint32_t>(kBchCodebook.size())));
  res.mirror_ok = ((res.best_idx ^ res.best_idx_mir) == 0xFFFFFFFFU);
  return res;
}
} // namespace

BchStatus bch_encode(Fid fid, Toi toi, std::span<uint8_t, 52ULL> out) noexcept {
  BchStatus status = BchStatus::kOk;
  // Use peek() to allow range validation to see the actual state
  const auto fid_val = fid.storage.peek().value();
  const auto toi_val = toi.storage.peek().value();

  std::ranges::fill(out, 0U);

  if (fid_val > kBchFidMax) [[unlikely]] {
    status = BchStatus::kInvalidFid;
  } else if (toi_val > kBchToiMax) [[unlikely]] {
    status = BchStatus::kInvalidToi;
  } else {
    const auto bit0 = static_cast<uint8_t>((static_cast<uint32_t>(fid_val) >> 1U) & 1U);
    auto state = static_cast<uint8_t>(((static_cast<uint32_t>(fid_val) & 1U) << 7U) |
                                      (static_cast<uint32_t>(toi_val) & 0x7FU));

    // BCH(51,8) generator polynomial tap mask: feedback from register stages
    // matching the primitive polynomial. [LSIS Appendix B]
    constexpr uint8_t kFeedbackMask = 0b11111001U;
    out[0] = bit0;

    for (uint32_t k = 0; k < 51U; ++k) {
      const auto output = static_cast<uint8_t>((static_cast<uint32_t>(state) >> 7U) & 1U);
      const auto fb = static_cast<uint8_t>(
          static_cast<uint32_t>(std::popcount(static_cast<uint32_t>(state & kFeedbackMask))) % 2U);
      state = static_cast<uint8_t>((static_cast<uint32_t>(state) << 1U) | fb);
      out[k + 1U] = static_cast<uint8_t>(output ^ bit0);
      wip_tick();
    }
  }
  return status;
}

BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept {
  uint64_t received = 0;
  for (uint32_t i = 0; i < 52U; ++i) {
    received |= (static_cast<uint64_t>(in[i] != 0U) << i);
    wip_tick();
  }

  const auto sr = bch_hard_search(received);

  // Single exit: CFI failure sets status rather than early-returning.
  BchStatus status = BchStatus::kOk;
  if (!sr.cfi_ok) [[unlikely]] {
    status = BchStatus::kFaultDetected; // FIZ-1
  } else if (sr.amb_mask != 0U && sr.min_dist != 0U) {
    status = BchStatus::kAmbiguousMatch;
  } else if (sr.min_dist > 2U) {
    status = BchStatus::kNullOutput;
  }

  const bool fault = (status == BchStatus::kFaultDetected);
  const auto fid = Fid(fault ? 0U : static_cast<uint8_t>(sr.best_idx / kBchToiCount));
  const auto toi = Toi(fault ? 0U : static_cast<uint8_t>(sr.best_idx % kBchToiCount));
  return BchResult(status, fid, toi, fault ? 255U : static_cast<uint8_t>(sr.min_dist));
}

BchResult bch_decode_soft(std::span<const float, 52ULL> llr) noexcept {
  // Input validation: set status flag, do not early-return (single exit mandate).
  // IEEE NaN comparisons are always false; an unguarded NaN would silently bias
  // best_metric, returning an incorrect FID/TOI with kOk.
  BchStatus status = BchStatus::kOk;
  for (uint32_t j = 0; j < 52U; ++j) {
    if (!std::isfinite(llr[j])) [[unlikely]] {
      status = BchStatus::kInvalidInput;
    }
    wip_tick();
  }

  BchSoftSearchResult sr{};
  if (status == BchStatus::kOk) {
    sr = bch_soft_search(llr);

    // CFI Verification
    if (!sr.cfi_ok) [[unlikely]] {
      status = BchStatus::kFaultDetected; // FIZ-1
    }
    // Variable mirror integrity check: detects SEU corruption of best_idx mid-loop.
    if (!sr.mirror_ok) [[unlikely]] {
      status = BchStatus::kFaultDetected; // FIZ-1
    }
  }

  const bool fault = (status == BchStatus::kFaultDetected);
  const auto fid = Fid(fault ? 0U : static_cast<uint8_t>(sr.best_idx / kBchToiCount));
  const auto toi = Toi(fault ? 0U : static_cast<uint8_t>(sr.best_idx % kBchToiCount));
  // 0xFF: soft-decode sentinel — hamming_distance is not a Hamming distance here.
  return BchResult(status, fid, toi, 0xFFU);
}

uint64_t bch_codebook_checksum() noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  for (const auto val : kBchCodebook) {
    for (uint32_t i = 0; i < 8U; i++) {
      crc = crc32_ieee_byte(crc, static_cast<uint8_t>((val >> (i * 8U)) & 0xFFU));
    }
    wip_tick();
  }
  return ~(static_cast<uint64_t>(crc));
}

} // namespace lunalink::signal
