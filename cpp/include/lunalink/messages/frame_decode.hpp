// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "lunalink/core/telemetry.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for the full RX decode pipeline: sync -> BCH -> deinterleave -> LDPC -> CRC.
 * [LSIS-AFS-720] SEU-hardened with high Hamming distance bit patterns. */
enum class FrameDecodeStatus : uint8_t {
  kOk = 0x5AU,            ///< All subframes decoded and CRC-verified.
  kSyncNotFound = 0xA5U,  ///< Sync pattern not detected above threshold.
  kBchFailed = 0x33U,     ///< SB1 BCH decode failed.
  kLdpcFailed = 0xCCU,    ///< One or more LDPC subframes did not converge.
  kCrcFailed = 0x0FU,     ///< One or more CRC-24Q verifications failed.
  kInvalidInput = 0x66U,  ///< Input parameters out of range or invalid.
  kFaultDetected = 0x99U, ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

// ─── Result Structs ──────────────────────────────────────────────────────────

/// Per-subframe decode status.
struct SubframeDecodeResult {
  bool ldpc_converged = false; ///< true if LDPC decoder converged (H·c=0)
  /// true if CRC-24Q verified after LDPC decode AND the frame status is kOk.
  /// frame_decode() scrubs every data buffer on a non-kOk status and clears
  /// this flag with them, so a set flag always has data behind it.
  bool crc_ok = false;
};

/// Full frame decode result. Navigation data is in the workspace buffers.
struct FrameDecodeResult {
  FrameDecodeStatus status = FrameDecodeStatus::kFaultDetected;
  SyncResult sync{};
  BchResult bch{BchStatus::kFaultDetected, Fid{}, Toi{}, 0};
  SubframeDecodeResult sb2{};
  SubframeDecodeResult sb3{};
  SubframeDecodeResult sb4{};
};

// ─── Workspace ───────────────────────────────────────────────────────────────

/// Caller-allocated scratchpad for frame_decode(). No heap allocation.
/// Contains both internal temporaries and output data buffers.
///
/// After a successful frame_decode(), read decoded navigation data from:
///   sb2_data[0..kSb2DataBits), sb3_data[0..kSb3DataBits), sb4_data[0..kSb4DataBits)
///
/// WARNING: ~120 KB total. Must be heap-allocated or placed in a dedicated
/// memory section on flight hardware. Do NOT allocate on a task stack.
struct FrameDecodeWorkspace {
  // Internal temporaries
  LdpcWorkspace ldpc_ws{};
  std::array<float, kPayloadLength> deinterleave_buf{};

  // LDPC decoded message bits (includes CRC-24)
  std::array<uint8_t, kSb2MsgBits> sb2_msg{};
  std::array<uint8_t, kSb3MsgBits> sb3_msg{};
  std::array<uint8_t, kSb4MsgBits> sb4_msg{};

  // Output: decoded navigation data (CRC-24 stripped)
  std::array<uint8_t, kSb2DataBits> sb2_data{};
  std::array<uint8_t, kSb3DataBits> sb3_data{};
  std::array<uint8_t, kSb4DataBits> sb4_data{};

#ifdef LUNALINK_FAULT_INJECTION
  /// SWIFI: number of CFI milestones to skip. Set before calling frame_decode().
  uint32_t cfi_skip_inject = 0;
  /// SWIFI: when non-zero, the sync detection stage returns kSyncNotFound
  /// immediately. Used for return-value injection testing. [SW-SAF-06]
  uint8_t fi_force_sync_fail = 0;
  /// SWIFI: when non-zero, the LDPC decode stage returns kFaultDetected
  /// immediately. Used for return-value injection testing. [SW-SAF-06]
  uint8_t fi_force_ldpc_fail = 0;
#endif
};

// ─── Constants ───────────────────────────────────────────────────────────────

/// Default LDPC iteration count for frame_decode().
inline constexpr uint8_t kFrameDecodeLdpcIterations = 50;

/// Default channel-LLR scale for frame_decode(): symbols reach the LDPC
/// decoder unscaled.
inline constexpr float kFrameDecodeLlrScale = 1.0F;

/// Minimum input size: must contain at least one complete frame.
inline constexpr size_t kFrameDecodeMinInput = kFrameLength;

/// Workspace size for memory budgeting.
/// ~375 KB with float64 sum-product decoder — must be heap-allocated.
inline constexpr size_t kFrameDecodeWorkspaceBytes = sizeof(FrameDecodeWorkspace);
static_assert(kFrameDecodeWorkspaceBytes < 400'000, "FrameDecodeWorkspace exceeds 400 KB budget");

// ─── Frame Decoder ───────────────────────────────────────────────────────────

/// Decode a complete AFS navigation frame from soft-decision symbols. [LSIS-AFS-720]
///
/// Orchestrates the full RX pipeline:
///   1. Sync pattern detection (SyncDetector)
///   2. SB1 region → soft BCH decode → FID, TOI
///   3. Payload region → soft deinterleave → split SB2/SB3/SB4
///   4. LDPC decode each subframe
///   5. CRC-24Q validate each subframe
///   6. Strip CRC, copy data to workspace output buffers
///
/// @param rx_symbols       Received soft symbols (bipolar: +1.0 = bit 0, −1.0 = bit 1),
///                         at unit signal amplitude: the sync threshold is an absolute
///                         correlation sum. Must contain at least kFrameLength symbols
///                         after the detected sync offset.
/// @param result           Output: full decode result with per-subframe status.
/// @param workspace        Caller-allocated scratchpad. After success, decoded data
///                         is in workspace.sb2_data, sb3_data, sb4_data.
/// @param hal              Telemetry HAL for fault reporting.
/// @param ldpc_iterations  LDPC BP iteration count (default: 50).
/// @param sync_threshold   Sync detection threshold (default: kDefaultSyncThreshold).
/// @param llr_scale        Factor applied to the de-interleaved payload before LDPC
///                         decoding, converting unit-amplitude symbols to channel LLRs.
///                         For BPSK over AWGN with noise variance sigma^2 it is
///                         2/sigma^2. Sync detection and BCH decoding use the
///                         unscaled symbols. Must be finite and positive; otherwise
///                         kInvalidInput (default: kFrameDecodeLlrScale, no scaling).
///
/// On any status other than kOk the output data buffers are scrubbed and the
/// per-subframe crc_ok flags are cleared. ldpc_converged still records which
/// subframes converged.
///
/// @return FrameDecodeStatus indicating overall decode outcome.
[[nodiscard("frame_decode status carries CRC-24Q, LDPC and BCH failure flags — silently "
            "discarding it masks frame errors that violate FM-034/CBIT-AFS")]] FrameDecodeStatus
frame_decode(std::span<const float> rx_symbols,
             FrameDecodeResult& result,
             FrameDecodeWorkspace& workspace,
             ITelemetryHal& hal,
             uint8_t ldpc_iterations = kFrameDecodeLdpcIterations,
             float sync_threshold = kDefaultSyncThreshold,
             float llr_scale = kFrameDecodeLlrScale) noexcept;

} // namespace lunalink::signal
