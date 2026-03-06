#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace lunalink::signal {

/// Sync pattern length in symbols (bits): 68.
inline constexpr uint16_t kSyncLength = 68;

/// SB1 (BCH-encoded) length: 52 symbols.
inline constexpr uint8_t kSb1Length = 52;

/// SB2+SB3+SB4 interleaved payload length: 60 × 98 = 5880 symbols.
inline constexpr uint16_t kPayloadLength = 5880;

/// Total frame length: sync (68) + SB1 (52) + payload (5880) = 6000 symbols.
inline constexpr uint16_t kFrameLength = 6000;
static_assert(kFrameLength == kSyncLength + kSb1Length + kPayloadLength, "Frame length invariant broken");

/// Symbol rate: 500 symbols per second.
inline constexpr uint16_t kSymbolRate = 500;

/// Frame duration in seconds: 6000 / 500 = 12.
inline constexpr uint8_t kFrameDurationS = 12;

/// Block interleaver rows.
inline constexpr uint8_t kInterleaverRows = 60;

/// Block interleaver columns.
inline constexpr uint8_t kInterleaverCols = 98;

enum class FrameStatus : uint8_t {
  kOk = 0,
  kOutputTooSmall,
  kInvalidFid,
  kInvalidToi,
  kBchFailed,
};

/// The 68-symbol sync pattern as individual {0, 1} values.
/// Source: hex CC63F74536F49E04A (LSIS V1.0 §2.4.1, Table 12).
extern constinit const std::array<uint8_t, kSyncLength> kSyncPattern;

/// Build a partial AFS navigation frame (V2 stub).
///
/// Layout: 68-symbol sync + 52-symbol BCH-encoded SB1 + 5880 zero-padded
/// symbols (placeholder for LDPC-encoded, interleaved SB2–SB4).
///
/// @pre fid <= 3
/// @pre toi <= 99
/// @pre out.size() == kFrameLength (6000)
/// @post out is populated with valid partial frame symbols in {0, 1}
/// @complexity O(kFrameLength)
[[nodiscard]] FrameStatus frame_build_partial(
    uint8_t            fid,
    uint8_t            toi,
    std::span<uint8_t> out) noexcept;

/**
 * @brief Ergonomic overload for frame_build_partial that deduces from arrays/fixed-spans.
 */
template <typename T>
[[nodiscard]] inline FrameStatus frame_build_partial(
    uint8_t fid,
    uint8_t toi,
    T&&     out) noexcept {
  return frame_build_partial(fid, toi, std::span<uint8_t>(out));
}

} // namespace lunalink::signal
