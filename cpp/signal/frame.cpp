#include "lunalink/signal/frame.hpp"
#include "lunalink/signal/bch.hpp"
#include <algorithm>
#include <array>
#include <ranges>

namespace lunalink::signal {

// Sync pattern: CC63F74536F49E04A (hex), 68 bits MSB-first.
// LSIS V1.0 §2.4.1, Table 12.
alignas(64) LUNALINK_LUT_SECTION constinit const std::array<uint8_t, kSyncLength> kSyncPattern = {
    1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0,
    1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    1, 0, 1, 0,
};

FrameStatus frame_build_partial(
    Fid                        fid,
    Toi                        toi,
    std::span<uint8_t, 6000ULL> out) noexcept {
  
  FrameStatus status = FrameStatus::kOk;

  // ESA Safety Policy: Zero-initialise entire frame.
  std::ranges::fill(out, 0U);

  // 1. Sync pattern: 68 symbols.
  std::ranges::copy(kSyncPattern, out.begin());

  // 2. SB1 (FID + TOI): 52 symbols BCH(51,8).
  const auto bch_status = bch_encode(fid, toi, out.subspan<kSyncLength, 52ULL>());
  
  if (bch_status != BchStatus::kOk) [[unlikely]] {
    secure_scrub(out);
    switch (bch_status) {
      case BchStatus::kInvalidFid:     status = FrameStatus::kInvalidFid; break;
      case BchStatus::kInvalidToi:     status = FrameStatus::kInvalidToi; break;
      default:                         status = FrameStatus::kBchFailed; break;
    }
  }

  // 3. SB2-SB4 payload: remains zeroed per initialization.
  return status;
}

} // namespace lunalink::signal
