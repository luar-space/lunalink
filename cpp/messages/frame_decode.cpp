/// @file frame_decode.cpp
/// @brief RX pipeline orchestrator: sync → BCH → deinterleave → LDPC → CRC. [LSIS-AFS-720]

#include "lunalink/messages/frame_decode.hpp"

#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"

namespace lunalink::signal {

namespace {

/// Copy n_data bits from decoded message to output, stripping the CRC-24 tail.
/// Returns false on bounds violation.
[[nodiscard]] bool strip_crc_copy(std::span<const uint8_t> msg,
                                  std::span<uint8_t> data_out,
                                  size_t n_data) noexcept {
    if (msg.size() < n_data || data_out.size() < n_data) {
        return false;
    }
    for (size_t i = 0; i < n_data; ++i) {
        data_out[i] = msg[i];
    }
    return true;
}

} // namespace

// Single-exit accumulator pattern per c_plus_plus_guidelines.rst §3.4. [LSIS-AFS-720]
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
FrameDecodeStatus frame_decode(
    std::span<const float>   rx_symbols,
    FrameDecodeResult&       result,
    FrameDecodeWorkspace&    workspace,
    ITelemetryHal&           hal,
    uint8_t                  ldpc_iterations,
    float                    sync_threshold) noexcept {

    // ── Pessimistic default ──────────────────────────────────────────────
    result = FrameDecodeResult{};
    FrameDecodeStatus status = FrameDecodeStatus::kFaultDetected;

    // ── CFI sentinel ─────────────────────────────────────────────────────
    volatile uint32_t cfi_count = 0x7E7E7E7EU;

    // ── Input validation ─────────────────────────────────────────────────
    if (rx_symbols.size() < kFrameDecodeMinInput) {
        // cppcheck-suppress redundantInitialization  ; pessimistic default is intentional safety pattern
        status = FrameDecodeStatus::kInvalidInput;
    } else if (spans_overlap(
                   static_cast<const void*>(rx_symbols.data()), rx_symbols.size_bytes(),
                   static_cast<const void*>(&workspace), sizeof(workspace))) {
        // Aliasing check: rx_symbols must not overlap workspace [MA §11]
        status = FrameDecodeStatus::kFaultDetected;
    } else {
        status = FrameDecodeStatus::kOk;  // Promote from pessimistic default
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 1: input validated
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 1: Sync detection ──────────────────────────────────────────
    SyncDetector sync_det(hal);

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk && sync_threshold > 0.0F) {
        if (!sync_det.set_threshold(sync_threshold)) {
            status = FrameDecodeStatus::kFaultDetected;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        const auto sync_status = sync_det.detect(rx_symbols, result.sync);
        if (sync_status == SyncDetectorStatus::kFaultDetected) {
            status = FrameDecodeStatus::kFaultDetected;
        } else if (sync_status != SyncDetectorStatus::kOk || !result.sync.detected) {
            status = FrameDecodeStatus::kSyncNotFound;
        }
    }

    // Verify enough symbols remain after sync offset for a complete frame
    size_t offset = 0;
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        offset = static_cast<size_t>(result.sync.frame_offset);
        if (offset + kFrameLength > rx_symbols.size()) {
            status = FrameDecodeStatus::kInvalidInput;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 2: sync detected
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 2: SB1 — soft BCH decode ──────────────────────────────────
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        const size_t sb1_start = offset + kSyncLength;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::span<const float> sb1_soft{rx_symbols.data() + sb1_start, kSb1Length};
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        std::array<float, kSb1Length> sb1_buf{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        for (size_t i = 0; i < kSb1Length; ++i) { sb1_buf[i] = sb1_soft[i]; }
        result.bch = bch_decode_soft(std::span<const float, 52>(sb1_buf.data(), 52));

        if (result.bch.status == BchStatus::kFaultDetected) {
            status = FrameDecodeStatus::kFaultDetected;
        } else if (result.bch.status != BchStatus::kOk) {
            status = FrameDecodeStatus::kBchFailed;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 3: BCH decoded
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 3: Payload — soft deinterleave ─────────────────────────────
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        const size_t payload_start = offset + kSyncLength + kSb1Length;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::span<const float> payload_interleaved{
            rx_symbols.data() + payload_start, kPayloadLength};
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        const auto deint_status = block_deinterleave_soft(
            payload_interleaved,
            std::span<float>(workspace.deinterleave_buf));

        if (deint_status != InterleaverStatus::kOk) {
            status = FrameDecodeStatus::kFaultDetected;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 4: deinterleaved
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 4: LDPC decode each subframe ───────────────────────────────
    // After deinterleave: [0..2400) = SB2, [2400..4140) = SB3, [4140..5880) = SB4
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const float* deint = workspace.deinterleave_buf.data();

        // SB2: 2400 → 1200
        const auto ldpc_sb2 = ldpc_decode(
            LdpcSubframe::kSF2,
            std::span<const float>(deint, kSb2Length),
            ldpc_iterations,
            std::span<uint8_t>(workspace.sb2_msg),
            workspace.ldpc_ws);
        result.sb2.ldpc_converged = (ldpc_sb2 == LdpcStatus::kOk);

        // SB3: 1740 → 870
        const auto ldpc_sb3 = ldpc_decode(
            LdpcSubframe::kSF3,
            std::span<const float>(deint + kSb2Length, kSb3Length),
            ldpc_iterations,
            std::span<uint8_t>(workspace.sb3_msg),
            workspace.ldpc_ws);
        result.sb3.ldpc_converged = (ldpc_sb3 == LdpcStatus::kOk);

        // SB4: 1740 → 870
        const auto ldpc_sb4 = ldpc_decode(
            LdpcSubframe::kSF4,
            std::span<const float>(deint + kSb2Length + kSb3Length, kSb4Length),
            ldpc_iterations,
            std::span<uint8_t>(workspace.sb4_msg),
            workspace.ldpc_ws);
        result.sb4.ldpc_converged = (ldpc_sb4 == LdpcStatus::kOk);
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Any hardware fault in LDPC → escalate
        if (ldpc_sb2 == LdpcStatus::kFaultDetected ||
            ldpc_sb3 == LdpcStatus::kFaultDetected ||
            ldpc_sb4 == LdpcStatus::kFaultDetected) {
            status = FrameDecodeStatus::kFaultDetected;
        }
    }

    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 5: LDPC decoded
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 5: CRC-24Q validation ──────────────────────────────────────
    // Only verify CRC on converged subframes — running CRC on unconverged LDPC
    // output could produce a false pass (~2^-24) on junk data.
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        if (result.sb2.ldpc_converged) {
            result.sb2.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb2_msg));
        }
        if (result.sb3.ldpc_converged) {
            result.sb3.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb3_msg));
        }
        if (result.sb4.ldpc_converged) {
            result.sb4.crc_ok = crc24_verify(std::span<const uint8_t>(workspace.sb4_msg));
        }

        LUNALINK_CRITICAL_ENTER;
        cfi_count = cfi_count + 1U;  // Milestone 6: CRC checked
        LUNALINK_CRITICAL_EXIT;
    }

    // ── Stage 6: Strip CRC, copy data to output buffers ──────────────────
    // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
    if (status == FrameDecodeStatus::kOk) {
        if (result.sb2.crc_ok) {
            if (!strip_crc_copy(workspace.sb2_msg, workspace.sb2_data, kSb2DataBits)) {
                status = FrameDecodeStatus::kFaultDetected;
            }
        }
        if (result.sb3.crc_ok) {
            if (!strip_crc_copy(workspace.sb3_msg, workspace.sb3_data, kSb3DataBits)) {
                status = FrameDecodeStatus::kFaultDetected;
            }
        }
        if (result.sb4.crc_ok) {
            if (!strip_crc_copy(workspace.sb4_msg, workspace.sb4_data, kSb4DataBits)) {
                status = FrameDecodeStatus::kFaultDetected;
            }
        }
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
        const bool all_ldpc = result.sb2.ldpc_converged &&
                              result.sb3.ldpc_converged &&
                              result.sb4.ldpc_converged;
        const bool all_crc  = result.sb2.crc_ok &&
                              result.sb3.crc_ok &&
                              result.sb4.crc_ok;

        if (!all_ldpc) {
            status = FrameDecodeStatus::kLdpcFailed;
        } else if (!all_crc) {
            status = FrameDecodeStatus::kCrcFailed;
        }
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
