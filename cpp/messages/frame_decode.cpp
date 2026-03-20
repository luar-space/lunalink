/// @file frame_decode.cpp
/// @brief RX pipeline orchestrator: sync → BCH → deinterleave → LDPC → CRC. [LSIS-AFS-720]

#include "lunalink/messages/frame_decode.hpp"

#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"

#include <algorithm>

namespace lunalink::signal {

namespace {

/// Copy n_data bits from decoded message to output, stripping the CRC-24 tail.
/// Returns false on bounds violation.
[[nodiscard]] bool
strip_crc_copy(std::span<const uint8_t> msg, std::span<uint8_t> data_out, size_t n_data) noexcept {
  if (msg.size() < n_data || data_out.size() < n_data) {
    return false;
  }
  std::ranges::copy_n(msg.begin(), static_cast<ptrdiff_t>(n_data), data_out.begin());
  return true;
}

/// Stage 0: Input validation and aliasing check.
[[nodiscard]] FrameDecodeStatus
decode_validate_input(std::span<const float> rx_symbols,
                      const FrameDecodeWorkspace& workspace) noexcept {
  if (rx_symbols.size() < kFrameDecodeMinInput) {
    return FrameDecodeStatus::kInvalidInput;
  }
  if (spans_overlap(static_cast<const void*>(rx_symbols.data()),
                    rx_symbols.size_bytes(),
                    static_cast<const void*>(&workspace),
                    sizeof(workspace))) {
    // Aliasing check: rx_symbols must not overlap workspace [MA §11]
    return FrameDecodeStatus::kFaultDetected;
  }
  return FrameDecodeStatus::kOk;
}

/// Stage 1: Sync detection — threshold + detect.
/// On success, populates result.sync and returns the frame offset via offset_out.
[[nodiscard]] FrameDecodeStatus decode_stage_sync(std::span<const float> rx_symbols,
                                                  FrameDecodeResult& result,
                                                  float sync_threshold,
                                                  ITelemetryHal& hal,
                                                  size_t& offset_out) noexcept {
  SyncDetector sync_det(hal);

  if (sync_threshold > 0.0F) {
    if (!sync_det.set_threshold(sync_threshold)) {
      return FrameDecodeStatus::kFaultDetected;
    }
  }

  const auto sync_status = sync_det.detect(rx_symbols, result.sync);
  if (sync_status == SyncDetectorStatus::kFaultDetected) {
    return FrameDecodeStatus::kFaultDetected;
  }
  if (sync_status != SyncDetectorStatus::kOk || !result.sync.detected) {
    return FrameDecodeStatus::kSyncNotFound;
  }

  // Verify enough symbols remain after sync offset for a complete frame
  offset_out = static_cast<size_t>(result.sync.frame_offset);
  if (offset_out + kFrameLength > rx_symbols.size()) {
    return FrameDecodeStatus::kInvalidInput;
  }

  return FrameDecodeStatus::kOk;
}

/// Stage 2: SB1 — soft BCH decode from float span.
[[nodiscard]] FrameDecodeStatus decode_stage_bch(std::span<const float> rx_symbols,
                                                 FrameDecodeResult& result,
                                                 size_t offset) noexcept {
  const size_t sb1_start = offset + kSyncLength;
  std::span<const float> sb1_soft = rx_symbols.subspan(sb1_start, kSb1Length);
  result.bch = bch_decode_soft(sb1_soft.first<kSb1Length>());

  if (result.bch.status == BchStatus::kFaultDetected) {
    return FrameDecodeStatus::kFaultDetected;
  }
  if (result.bch.status != BchStatus::kOk) {
    return FrameDecodeStatus::kBchFailed;
  }
  return FrameDecodeStatus::kOk;
}

/// Stage 3: Payload — soft deinterleave.
[[nodiscard]] FrameDecodeStatus decode_stage_deinterleave(std::span<const float> rx_symbols,
                                                          FrameDecodeWorkspace& workspace,
                                                          size_t offset) noexcept {
  const size_t payload_start = offset + kSyncLength + kSb1Length;
  std::span<const float> payload_interleaved = rx_symbols.subspan(payload_start, kPayloadLength);

  const auto deint_status = block_deinterleave_soft(payload_interleaved,
                                                    std::span<float>(workspace.deinterleave_buf));

  if (deint_status != InterleaverStatus::kOk) {
    return FrameDecodeStatus::kFaultDetected;
  }
  return FrameDecodeStatus::kOk;
}

/// Stage 4: LDPC decode all 3 subframes.
[[nodiscard]] FrameDecodeStatus decode_stage_ldpc(FrameDecodeResult& result,
                                                  FrameDecodeWorkspace& workspace,
                                                  uint8_t ldpc_iterations) noexcept {
  const float* deint = workspace.deinterleave_buf.data();

  // SB2: 2400 → 1200
  const auto ldpc_sb2 = ldpc_decode(LdpcSubframe::kSF2,
                                    std::span<const float>(deint, kSb2Length),
                                    ldpc_iterations,
                                    std::span<uint8_t>(workspace.sb2_msg),
                                    workspace.ldpc_ws);
  result.sb2.ldpc_converged = (ldpc_sb2 == LdpcStatus::kOk);

  // SB3: 1740 → 870
  // Pointer arithmetic on workspace deinterleave_buf for subframe offsets
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto ldpc_sb3 = ldpc_decode(LdpcSubframe::kSF3,
                                    std::span<const float>(deint + kSb2Length, kSb3Length),
                                    ldpc_iterations,
                                    std::span<uint8_t>(workspace.sb3_msg),
                                    workspace.ldpc_ws);
  result.sb3.ldpc_converged = (ldpc_sb3 == LdpcStatus::kOk);

  // SB4: 1740 → 870
  const auto ldpc_sb4 =
      ldpc_decode(LdpcSubframe::kSF4,
                  std::span<const float>(deint + kSb2Length + kSb3Length, kSb4Length),
                  ldpc_iterations,
                  std::span<uint8_t>(workspace.sb4_msg),
                  workspace.ldpc_ws);
  result.sb4.ldpc_converged = (ldpc_sb4 == LdpcStatus::kOk);
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // Any hardware fault in LDPC → escalate
  if (ldpc_sb2 == LdpcStatus::kFaultDetected || ldpc_sb3 == LdpcStatus::kFaultDetected ||
      ldpc_sb4 == LdpcStatus::kFaultDetected) {
    return FrameDecodeStatus::kFaultDetected;
  }
  return FrameDecodeStatus::kOk;
}

/// Stage 5: CRC-24Q verify + strip_crc_copy to output buffers.
[[nodiscard]] FrameDecodeStatus
decode_stage_crc_and_copy(FrameDecodeResult& result, FrameDecodeWorkspace& workspace) noexcept {
  // Only verify CRC on converged subframes — running CRC on unconverged LDPC
  // output could produce a false pass (~2^-24) on junk data.
  if (result.sb2.ldpc_converged) {
    result.sb2.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb2_msg));
  }
  if (result.sb3.ldpc_converged) {
    result.sb3.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb3_msg));
  }
  if (result.sb4.ldpc_converged) {
    result.sb4.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb4_msg));
  }

  // Strip CRC, copy data to output buffers
  if (result.sb2.crc_ok) {
    if (!strip_crc_copy(workspace.sb2_msg, workspace.sb2_data, kSb2DataBits)) {
      return FrameDecodeStatus::kFaultDetected;
    }
  }
  if (result.sb3.crc_ok) {
    if (!strip_crc_copy(workspace.sb3_msg, workspace.sb3_data, kSb3DataBits)) {
      return FrameDecodeStatus::kFaultDetected;
    }
  }
  if (result.sb4.crc_ok) {
    if (!strip_crc_copy(workspace.sb4_msg, workspace.sb4_data, kSb4DataBits)) {
      return FrameDecodeStatus::kFaultDetected;
    }
  }

  return FrameDecodeStatus::kOk;
}

/// Determine overall decode outcome from per-subframe results.
[[nodiscard]] FrameDecodeStatus decode_classify_result(const FrameDecodeResult& result) noexcept {
  const bool all_ldpc = result.sb2.ldpc_converged && result.sb3.ldpc_converged &&
                        result.sb4.ldpc_converged;
  const bool all_crc = result.sb2.crc_ok && result.sb3.crc_ok && result.sb4.crc_ok;

  if (!all_ldpc) {
    return FrameDecodeStatus::kLdpcFailed;
  }
  if (!all_crc) {
    return FrameDecodeStatus::kCrcFailed;
  }
  return FrameDecodeStatus::kOk;
}

#ifdef LUNALINK_FAULT_INJECTION
/// SWIFI return-value injection helper for frame_decode stages.
/// Checks the one-shot injection field; if set, resets it and overwrites
/// status with the injected value.  Otherwise status is unchanged.
void fi_override_stage(uint8_t& fi_flag,
                       FrameDecodeStatus injected,
                       FrameDecodeStatus& status) noexcept {
  if (fi_flag != 0U) {
    fi_flag = 0U; // one-shot
    status = injected;
  }
}
#endif

} // namespace

// Single-exit accumulator pattern per c_plus_plus_guidelines.rst §3.4. [LSIS-AFS-720]
FrameDecodeStatus frame_decode(std::span<const float> rx_symbols,
                               FrameDecodeResult& result,
                               FrameDecodeWorkspace& workspace,
                               ITelemetryHal& hal,
                               uint8_t ldpc_iterations,
                               float sync_threshold) noexcept {
  // ── Pessimistic default ──────────────────────────────────────────────
  result = FrameDecodeResult{};
  FrameDecodeStatus status = FrameDecodeStatus::kFaultDetected;

  // ── CFI sentinel ─────────────────────────────────────────────────────
  volatile uint32_t cfi_count = 0x7E7E7E7EU;
  size_t offset = 0;

  // ── Input validation ─────────────────────────────────────────────────
  // cppcheck-suppress redundantInitialization  ; pessimistic default is intentional safety pattern
  status = decode_validate_input(rx_symbols, workspace);

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: input validated
    LUNALINK_CRITICAL_EXIT;
    status = decode_stage_sync(rx_symbols, result, sync_threshold, hal, offset);
#ifdef LUNALINK_FAULT_INJECTION
    // SWIFI: override sync result with injected failure. [SW-SAF-06]
    fi_override_stage(workspace.fi_force_sync_fail,
                      FrameDecodeStatus::kSyncNotFound, status);
#endif
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: sync detected
    LUNALINK_CRITICAL_EXIT;
    status = decode_stage_bch(rx_symbols, result, offset);
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 3: BCH decoded
    LUNALINK_CRITICAL_EXIT;
    status = decode_stage_deinterleave(rx_symbols, workspace, offset);
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 4: deinterleaved
    LUNALINK_CRITICAL_EXIT;
    status = decode_stage_ldpc(result, workspace, ldpc_iterations);
#ifdef LUNALINK_FAULT_INJECTION
    // SWIFI: override LDPC result with injected failure. [SW-SAF-06]
    fi_override_stage(workspace.fi_force_ldpc_fail,
                      FrameDecodeStatus::kFaultDetected, status);
#endif
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 5: LDPC decoded
    LUNALINK_CRITICAL_EXIT;
    status = decode_stage_crc_and_copy(result, workspace);
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 6: CRC checked + data copied
    LUNALINK_CRITICAL_EXIT;
  }

  // ── CFI terminal check ───────────────────────────────────────────────
  // Only check if we completed all 6 stages (status == kOk).
  // An early-out error means fewer milestones — that's expected, not a CFI fault.
#ifdef LUNALINK_FAULT_INJECTION
  if (workspace.cfi_skip_inject > 0) {
    cfi_count = cfi_count - workspace.cfi_skip_inject;
  }
#endif
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    if (cfi_count != (0x7E7E7E7EU + 6U)) [[unlikely]] {
      const FaultPacket pkt{0, 0x80, 2, 6, 0};
      hal.emit(pkt);
      status = FrameDecodeStatus::kFaultDetected;
    }
  }

  // ── Determine overall status ─────────────────────────────────────────
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (status == FrameDecodeStatus::kOk) {
    status = decode_classify_result(result);
  }

  // ── Mandatory scrubbing ──────────────────────────────────────────────
  // Scrub ALL workspace temporaries unconditionally (deinterleave buf, msg bufs).
  // Scrub ALL output data buffers on any non-kOk status — including kFaultDetected
  // after CFI terminal check, where crc_ok may already be true. [MA §7]
  secure_scrub(workspace.deinterleave_buf);
  secure_scrub(workspace.sb2_msg);
  secure_scrub(workspace.sb3_msg);
  secure_scrub(workspace.sb4_msg);

  if (status != FrameDecodeStatus::kOk) {
    secure_scrub(workspace.sb2_data);
    secure_scrub(workspace.sb3_data);
    secure_scrub(workspace.sb4_data);
  }

  // ── Single exit ──────────────────────────────────────────────────────
  result.status = status;
  return status;
}

} // namespace lunalink::signal
