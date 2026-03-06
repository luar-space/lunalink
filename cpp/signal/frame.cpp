#include "lunalink/signal/frame.hpp"

#include "lunalink/signal/bch.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <cstring>

namespace lunalink::signal {

// Frame layout static verification
static_assert(kFrameLength == kSyncLength + kSb1Length + kPayloadLength,
              "Frame length invariant broken.");

// Sync pattern: CC63F74536F49E04A (hex), 68 bits MSB-first.
// LSIS V1.0 §2.4.1, Table 12.
constinit const std::array<uint8_t, kSyncLength> kSyncPattern = {
    // C        C        6        3
    1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1,
    // F        7        4        5
    1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    // 3        6        F        4
    0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0,
    // 9        E        0        4
    1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    // A (4 bits)
    1, 0, 1, 0,
};

FrameStatus frame_build_partial(
    uint8_t            fid,
    uint8_t            toi,
    std::span<uint8_t> out) noexcept {
  
  // ESA/JAXA Safety Policy: Pre-initialise output buffer to known safe state (all zeros).
  if (!out.empty()) {
    std::fill(out.begin(), out.end(), 0U);
  }

  if (out.size() < kFrameLength) [[unlikely]] {
    return FrameStatus::kOutputTooSmall;
  }

  // 1. Sync pattern: 68 symbols.
  std::ranges::copy(kSyncPattern, out.begin());

  // 2. SB1 (FID + TOI): 52 symbols BCH(51,8).
  const auto bch_status = bch_encode(
      fid, toi, out.subspan(kSyncLength, kSb1Length));
  
  if (bch_status != BchStatus::kOk) [[unlikely]] {
    // ESA Safety: Explicitly clear partial results on failure to ensure atomicity.
    std::fill(out.begin(), out.end(), 0U);
    
    // ECSS exhaustive error mapping.
    switch (bch_status) {
      case BchStatus::kInvalidFid:     return FrameStatus::kInvalidFid;
      case BchStatus::kInvalidToi:     return FrameStatus::kInvalidToi;
      case BchStatus::kOutputTooSmall: return FrameStatus::kOutputTooSmall;
      case BchStatus::kNullOutput:     return FrameStatus::kBchFailed;
      case BchStatus::kOk:             break; // Should not be reachable
    }
    return FrameStatus::kBchFailed;
  }

  // 3. Zero-pad SB2+SB3+SB4 payload (5880 symbols) per §2.4.3.
  std::ranges::fill(out.subspan(kSyncLength + kSb1Length, kPayloadLength), 0U);

  return FrameStatus::kOk;
}

} // namespace lunalink::signal
