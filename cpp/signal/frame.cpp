#include "lunalink/signal/frame.hpp"
#include "lunalink/signal/bch.hpp"
#include "lunalink/signal/interleaver.hpp"
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

  // 3. SB2-SB4 payload: Interleaved block (5880 symbols).
  // Note: Builders for SB2-SB4 are implemented in Gateway 3.
  // We apply the interleaver to the zeroed buffer to ensure the structure is ready.
  if (status == FrameStatus::kOk) {
      // Temporary staging buffer for non-interleaved data (SB2+SB3+SB4)
      // Mandate: Zero dynamic allocation. Use stack.
      alignas(16) std::array<uint8_t, kInterleaverSize> staging{};
      std::ranges::fill(staging, 0U);

      // In Gateway 3, we will fill 'staging' with encoded LDPC symbols here.

      const auto int_status = block_interleave(staging, out.subspan<kSyncLength + 52ULL, kInterleaverSize>());
      if (int_status != InterleaverStatus::kOk) [[unlikely]] {
          status = FrameStatus::kFaultDetected;  // FIZ-2
          secure_scrub(out);
      }
      secure_scrub(staging);
  }

  return status;
}

FrameStatus commit_frame(std::span<const uint8_t> frame_buf) noexcept {
  // SW-SAF-01: No frame shall be transmitted unless the post-generation CRC-24Q check passes.
  // The last 24 symbols of frame_buf must be the CRC-24Q of the preceding symbols.
  if (!crc24_verify(frame_buf)) {
    return FrameStatus::kFaultDetected;  // FIZ-2
  }
  return FrameStatus::kOk;
}

} // namespace lunalink::signal
