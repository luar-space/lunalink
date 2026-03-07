#pragma once
#include <array>
#include <cstddef>
#include "lunalink/signal/bch.hpp"
#include <cstdint>
#include <span>

namespace lunalink::signal {

/// Navigation frame length (6000 binary symbols).
/// Per LSIS V1.0 §2.4: 12.0s duration @ 500 sps.
inline constexpr uint16_t kFrameLength = 6000;

/// Length of the sync pattern (68 binary symbols).
inline constexpr uint8_t kSyncLength = 68;

/// Length of the SB1 sub-block (52 binary symbols).
inline constexpr uint8_t kSb1Length = 52;

/// Length of the remaining payload (SB2-SB4) before CRC/LDPC (5880 binary symbols).
inline constexpr uint16_t kPayloadLength = 5880;

/// Symbol rate: 500 sps.
inline constexpr uint16_t kSymbolRate = 500;

/// Frame duration: 12.0 seconds.
inline constexpr float kFrameDurationS = 12.0F;

/// Status codes for navigation frame building.
enum class FrameStatus : uint8_t {
  kOk              = 0x5AU,  // 01011010
  kOutputTooSmall  = 0xA5U,  // 10100101
  kInvalidFid      = 0x33U,  // 00110011
  kInvalidToi      = 0xCCU,  // 11001100
  kBchFailed       = 0x0FU,  // 00001111
};

/**
 * @brief Build a partial AFS navigation frame (§2.4).
 *
 * Current implementation constructs:
 *   - 68-symbol sync pattern
 *   - 52-symbol BCH-encoded SB1 header (FID + TOI)
 *   - 5880 symbols of zero padding (SB2-SB4 placeholder)
 *
 * @param fid  Frame ID (0-3).
 * @param toi  Time of Interval (0-99).
 * @param out  Buffer to populate (exactly 6000 symbols).
 *
 * @pre fid <= 3
 * @pre toi <= 99
 * @pre out.size() == kFrameLength (6000)
 * @post out is populated with valid partial frame symbols in {0, 1}
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus frame_build_partial(
    Fid                        fid,
    Toi                        toi,
    std::span<uint8_t, 6000ULL> out) noexcept;

/**
 * @brief Ergonomic overload for frame_build_partial that deduces from arrays/fixed-spans.
 */
template <typename T>
[[nodiscard]] inline FrameStatus frame_build_partial(
    Fid fid,
    Toi toi,
    T&& out) noexcept {
  return frame_build_partial(fid, toi, std::span<uint8_t, 6000ULL>(out));
}

} // namespace lunalink::signal
