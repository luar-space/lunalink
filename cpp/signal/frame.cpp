#include "lunalink/signal/frame.hpp"

#include "lunalink/signal/bch.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace lunalink::signal {

// Sync pattern: CC63F74536F49E04A (hex), 68 bits MSB-first.
// LSIS V1.0 §2.4.1, Table 12.
const std::array<uint8_t, kSyncLength> kSyncPattern = {
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
    uint8_t     fid,
    uint8_t     toi,
    uint8_t*    out,
    std::size_t out_len
) noexcept {
  if (out == nullptr) {
    return FrameStatus::kNullOutput;
  }
  if (out_len < kFrameLength) {
    return FrameStatus::kOutputTooSmall;
  }

  // 1. Copy sync pattern (68 symbols) per §2.4.1.
  std::copy(kSyncPattern.begin(), kSyncPattern.end(), out);

  // Validate FID and TOI ranges before encoding.
  if (fid > 3) {
    return FrameStatus::kInvalidFid;
  }
  if (toi > 99) {
    return FrameStatus::kInvalidToi;
  }

  // 2. BCH-encode SB1 into frame at offset 68 per §2.4.2.1.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto bch_status = bch_encode(fid, toi, out + kSyncLength, kSb1Length);
  if (bch_status != BchStatus::kOk) {
    return FrameStatus::kBchFailed;
  }

  // 3. Zero-pad SB2+SB3+SB4 payload (5880 symbols) per §2.4.3.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::fill_n(out + kSyncLength + kSb1Length, kPayloadLength, static_cast<uint8_t>(0));

  return FrameStatus::kOk;
}

} // namespace lunalink::signal
