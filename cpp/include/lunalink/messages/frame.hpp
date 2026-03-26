#pragma once
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/ldpc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace lunalink::signal {

/// Navigation frame length (6000 binary symbols).
/// Per LSIS V1.0 §2.4: 12.0s duration @ 500 sps.
inline constexpr uint16_t kFrameLength = 6000;

/// Length of the sync pattern (68 binary symbols).
inline constexpr uint16_t kSyncLength = 68;

/// Length of the SB1 sub-block (52 binary symbols).
inline constexpr uint16_t kSb1Length = 52;

/// Length of the remaining payload (SB2-SB4) after LDPC+interleave (5880 binary symbols).
inline constexpr uint16_t kPayloadLength = 5880;

/// SB2 data bits (before CRC-24). Per LSIS V1.0 Table 18.
inline constexpr uint16_t kSb2DataBits = 1176;
/// SB2 message bits (data + CRC-24, LDPC SF2 input).
inline constexpr uint16_t kSb2MsgBits = 1200;
/// SB2 encoded length (LDPC SF2 output, rate 1/2).
inline constexpr uint16_t kSb2Length = 2400;

/// SB3 data bits (before CRC-24). Per LSIS V1.0 Table 19.
inline constexpr uint16_t kSb3DataBits = 846;
/// SB3 message bits (data + CRC-24, LDPC SF3 input).
inline constexpr uint16_t kSb3MsgBits = 870;
/// SB3 encoded length (LDPC SF3 output, rate 1/2).
inline constexpr uint16_t kSb3Length = 1740;

/// SB4 data bits (before CRC-24). Per LSIS V1.0 Table 20.
inline constexpr uint16_t kSb4DataBits = 846;
/// SB4 message bits (data + CRC-24, LDPC SF4 input).
inline constexpr uint16_t kSb4MsgBits = 870;
/// SB4 encoded length (LDPC SF4 output, rate 1/2; reuses SF3 matrices).
inline constexpr uint16_t kSb4Length = 1740;

/// Sync pattern: 68 unpacked binary symbols (LSIS V1.0 §2.4.1, Table 12).
/// Defined in frame.cpp with constinit + section pinning.
extern const std::array<uint8_t, kSyncLength> kSyncPattern;

/// Caller-allocated workspace for frame building functions.
/// Holds the interleaver staging buffer (5880 bytes) to keep it off the stack.
/// Same pattern as LdpcEncodeWorkspace — allocate once, reuse across calls.
struct FrameBuildWorkspace {
  alignas(16) std::array<uint8_t, kPayloadLength> staging{};
};

/// Symbol rate: 500 sps.
inline constexpr uint16_t kSymbolRate = 500;

/// Frame duration: 12.0 seconds. Must agree with kFrameDurationSec in time_of_transmission.hpp.
inline constexpr float kFrameDurationS = 12.0F;

/** @brief Status codes for frame assembly (sync + SB1-SB4 + interleave). [LSIS-AFS-601]
 * SEU-hardened with high Hamming distance bit patterns. */
enum class FrameStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kOutputTooSmall = 0xA5U, ///< Output buffer too small for result.
  kInvalidFid = 0x33U,     ///< FID value exceeds kBchFidMax.
  kInvalidToi = 0xCCU,     ///< TOI value exceeds kBchToiMax.
  kBchFailed = 0x0FU,      ///< SB1 BCH decode failed.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
  kLdpcFailed = 0x3CU,     ///< One or more LDPC subframes did not converge.
  kCrcFailed = 0xC3U,      ///< One or more CRC-24Q verifications failed.
};

/**
 * @brief Build a partial AFS navigation frame (§2.4). [LSIS-AFS-601]
 *
 * Current implementation constructs:
 *   - 68-symbol sync pattern
 *   - 52-symbol BCH-encoded SB1 header (FID + TOI)
 *   - 5880 symbols of zero padding (SB2-SB4 placeholder)
 *
 * @param fid       Frame ID (0-3).
 * @param toi       Time of Interval (0-99).
 * @param out       Buffer to populate (exactly 6000 symbols).
 * @param workspace Interleaver staging scratchpad (caller-allocated).
 *
 * @pre fid <= 3
 * @pre toi <= 99
 * @pre out.size() == kFrameLength (6000)
 * @post out is populated with valid partial frame symbols in {0, 1}
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus frame_build_partial(Fid fid,
                                              Toi toi,
                                              std::span<uint8_t, 6000ULL> out,
                                              FrameBuildWorkspace& workspace) noexcept;

/**
 * @brief Ergonomic overload for frame_build_partial that deduces from arrays/fixed-spans.
 */
template <typename T>
[[nodiscard]] inline FrameStatus
frame_build_partial(Fid fid, Toi toi, T&& out, FrameBuildWorkspace& workspace) noexcept {
  return frame_build_partial(fid, toi, std::span<uint8_t, 6000ULL>(out), workspace);
}

/**
 * @brief Frame Commit Gate: verify the assembled frame by recomputing CRC-24Q. [LSIS-AFS-606]
 * [SW-SAF-01]
 *
 * Verifies that the last 24 symbols of frame_buf are the correct CRC-24Q of the
 * preceding symbols. Returns kFaultDetected if the checksum fails — no frame
 * shall be transmitted unless this check passes.
 *
 * @note This function performs a read-only check. The caller is responsible for
 *       scrubbing the frame buffer if this function returns kFaultDetected.
 * @note The LSIS spec defines no frame-level CRC; per-subframe CRCs are embedded
 *       inside LDPC codewords. This gate is a defence-in-depth hook for transmit
 *       pipelines that append an outer CRC before DMA hand-off.
 *
 * @param frame_buf  Assembled frame buffer (must be at least 25 symbols).
 * @return FrameStatus::kOk if the CRC matches; kFaultDetected otherwise. // FIZ-2
 */
[[nodiscard]] FrameStatus commit_frame(std::span<const uint8_t> frame_buf) noexcept;

/**
 * @brief Build Subframe 2: CRC-24 + LDPC SF2 encode. [LSIS-AFS-602]
 *
 * Computes CRC-24Q over data, appends it, then LDPC-encodes the 1200-bit
 * message to produce 2400 encoded symbols.
 *
 * @param data      SB2 navigation data (1176 bits, one bit per byte).
 * @param out       Output buffer for encoded symbols (2400 symbols).
 * @param workspace LDPC encoder scratchpad (caller-allocated).
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus subframe_build_sb2(std::span<const uint8_t, kSb2DataBits> data,
                                             std::span<uint8_t, kSb2Length> out,
                                             LdpcEncodeWorkspace& workspace) noexcept;

/**
 * @brief Build Subframe 3: CRC-24 + LDPC SF3 encode. [LSIS-AFS-603]
 *
 * Computes CRC-24Q over data, appends it, then LDPC-encodes the 870-bit
 * message to produce 1740 encoded symbols.
 *
 * @param data      SB3 navigation data (846 bits, one bit per byte).
 * @param out       Output buffer for encoded symbols (1740 symbols).
 * @param workspace LDPC encoder scratchpad (caller-allocated).
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus subframe_build_sb3(std::span<const uint8_t, kSb3DataBits> data,
                                             std::span<uint8_t, kSb3Length> out,
                                             LdpcEncodeWorkspace& workspace) noexcept;

/**
 * @brief Build Subframe 4: CRC-24 + LDPC SF4 encode. [LSIS-AFS-604]
 *
 * Identical pipeline to SB3 (LDPC SF4 reuses SF3 matrices).
 *
 * @param data      SB4 navigation data (846 bits, one bit per byte).
 * @param out       Output buffer for encoded symbols (1740 symbols).
 * @param workspace LDPC encoder scratchpad (caller-allocated).
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus subframe_build_sb4(std::span<const uint8_t, kSb4DataBits> data,
                                             std::span<uint8_t, kSb4Length> out,
                                             LdpcEncodeWorkspace& workspace) noexcept;

/**
 * @brief Build a complete AFS navigation frame (§2.4). [LSIS-AFS-605]
 *
 * Constructs a full 6000-symbol frame:
 *   - 68-symbol sync pattern (Table 12)
 *   - 52-symbol BCH-encoded SB1 header (FID + TOI)
 *   - 5880 interleaved symbols (SB2 + SB3 + SB4)
 *
 * Pipeline: data → CRC-24 → LDPC encode → concatenate → block interleave.
 *
 * @param fid             Frame ID (0-3).
 * @param toi             Time of Interval (0-99).
 * @param sb2_data        Subframe 2 navigation data (1176 bits).
 * @param sb3_data        Subframe 3 navigation data (846 bits).
 * @param sb4_data        Subframe 4 navigation data (846 bits).
 * @param out             Buffer to populate (exactly 6000 symbols).
 * @param ldpc_workspace  LDPC encoder scratchpad (caller-allocated, reused across subframes).
 * @param frame_workspace Interleaver staging scratchpad (caller-allocated).
 * @return FrameStatus::kOk or an error code.
 */
[[nodiscard]] FrameStatus frame_build(Fid fid,
                                      Toi toi,
                                      std::span<const uint8_t, kSb2DataBits> sb2_data,
                                      std::span<const uint8_t, kSb3DataBits> sb3_data,
                                      std::span<const uint8_t, kSb4DataBits> sb4_data,
                                      std::span<uint8_t, kFrameLength> out,
                                      LdpcEncodeWorkspace& ldpc_workspace,
                                      FrameBuildWorkspace& frame_workspace) noexcept;

} // namespace lunalink::signal
