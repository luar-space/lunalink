#include "lunalink/messages/frame.hpp"

#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/messages/message_parser.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace lunalink::signal {

// Sync pattern: CC63F74536F49E04A (hex), 68 bits MSB-first.
// LSIS V1.0 §2.4.1, Table 12.
alignas(64) LUNALINK_LUT_SECTION constinit const std::array<uint8_t, kSyncLength> kSyncPattern = {
    1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1,
    0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0,
};

FrameStatus frame_build_partial(Fid fid,
                                Toi toi,
                                std::span<uint8_t, 6000ULL> out,
                                FrameBuildWorkspace& workspace) noexcept {
  // [LSIS-AFS-601] CFI sentinel for partial frame builder (§5).
  constexpr uint32_t kCfiMagicPartial = 0xD2D2D2D2U;

  FrameStatus status = FrameStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicPartial;
  bool ok = true;

  // Aliasing check: caller-provided staging must not overlap output frame.
  if (spans_overlap(
          workspace.staging.data(), workspace.staging.size(), out.data(), out.size_bytes())) {
    ok = false;
  }

  // ESA Safety Policy: Zero-initialise entire frame (skip if aliasing detected).
  // 1. Sync pattern: 68 symbols (inside aliasing guard — P0-2).
  if (ok) {
    std::ranges::fill(out, 0U);
    std::ranges::copy(kSyncPattern, out.begin());
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // milestone 1: sync copied
    LUNALINK_CRITICAL_EXIT;
  }

  // 2. SB1 (FID + TOI): 52 symbols BCH(51,8).
  {
    const auto bch_status = bch_encode(fid, toi, out.subspan<kSyncLength, kSb1Length>());
    if (bch_status != BchStatus::kOk) [[unlikely]] {
      ok = false;
      switch (bch_status) {
      case BchStatus::kInvalidFid:
        status = FrameStatus::kInvalidFid;
        break;
      case BchStatus::kInvalidToi:
        status = FrameStatus::kInvalidToi;
        break;
      default: // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): DEAD_BY_DESIGN — MISRA Rule 16.4 default case
        status = FrameStatus::kBchFailed;
        break; // NOCOVER_END
      }
    } else {
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U; // milestone 2: SB1 done
      LUNALINK_CRITICAL_EXIT;
    }
  }

  // 3. SB2-SB4 payload: Interleaved block (5880 symbols).
  // Note: Builders for SB2-SB4 are implemented in Gateway 3.
  // We apply the interleaver to the zeroed buffer to ensure the structure is ready.
  if (ok) {
    // Staging buffer for non-interleaved data (SB2+SB3+SB4).
    // Caller-allocated via FrameBuildWorkspace to keep stack bounded.
    std::ranges::fill(workspace.staging, 0U);

    const auto int_status = block_interleave(
        workspace.staging,
        out.subspan<kSyncLength + static_cast<size_t>(kSb1Length), kPayloadLength>());
    if (int_status != InterleaverStatus::kOk) [[unlikely]] {
      ok = false;                           // NOCOVER(DEFENCE_IN_DEPTH): interleaver fault path
      status = FrameStatus::kFaultDetected; // FIZ-2
    } else {
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U; // milestone 3: interleave done
      LUNALINK_CRITICAL_EXIT;
    }
  }
  // Always scrub caller-owned staging (§7) — unconditional to prevent
  // leaking intermediate data from prior calls on error paths.
  secure_scrub(workspace.staging);

  // CFI terminal check: all 3 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicPartial + 3U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected; // FIZ-2 (CFI violation) // NOCOVER(DEFENCE_IN_DEPTH):
                                            // CFI terminal check
    }
  }

  // Cleanup: scrub frame on fault (§7).
  if (status != FrameStatus::kOk) {
    secure_scrub(out);
  }

  return status;
}

FrameStatus commit_frame(std::span<const uint8_t> frame_buf) noexcept {
  // SW-SAF-01: No frame shall be transmitted unless the post-generation CRC-24Q check passes.
  // The last 24 symbols of frame_buf must be the CRC-24Q of the preceding symbols.
  if (!crc24_verify(frame_buf)) {
    return FrameStatus::kFaultDetected; // FIZ-2
  }
  return FrameStatus::kOk;
}

// ---------------------------------------------------------------------------
// Gateway 3: Subframe builders and full frame assembler
// ---------------------------------------------------------------------------

namespace {

// CFI magic patterns for subframe builder and frame assembler (§5).
constexpr uint32_t kCfiMagicSb = 0xB4B4B4B4U;
constexpr uint32_t kCfiMagicFrame = 0x4B4B4B4BU;

/// [LSIS-AFS-300] Overwrite the SB2 spare region with the alternating 0/1
/// pattern starting with 0 on the MSB.
///
/// Single enforcement point for LSIS V1.0 §300 ("spare bits shall be a
/// sequence of alternating zeros and ones starting with zero on the
/// MSB"). G-18 moved responsibility from the caller to this helper so
/// that every SB2 frame LunaLink emits is spec-compliant regardless of
/// what the caller supplied in the spare region.
///
/// Behaviour:
///   * No-op for ``LdpcSubframe::kSF3`` / ``kSF4`` (their spare regions
///     are message-type-specific per LSIS V1.0 and cannot be enforced
///     centrally without per-message-type knowledge).
///   * Defensive bounds guard: the helper skips the rewrite if the
///     message span is shorter than ``kSb2SpareOffset + kSb2SpareBits``.
///     The nominal caller (``subframe_build_ldpc`` with SF2 params)
///     always passes a 1200-bit span, so the guard is defence-in-depth
///     against SEU corruption of the constant or of
///     ``expected_msg_bits`` — see FM-103.
///
/// @param type  Subframe type — only ``kSF2`` is rewritten.
/// @param msg   Writable message buffer (post data-copy, pre-CRC). The
///              nominal SF2 caller passes ``kSb2MsgBits`` (1200) bytes;
///              the helper writes indices ``[kSb2SpareOffset,
///              kSb2SpareOffset + kSb2SpareBits)``. Spans shorter than
///              ``kSb2SpareOffset + kSb2SpareBits`` (1176) are silently
///              ignored as a defence-in-depth check (FM-103).
///
/// Single-exit per mission assurance §3: branch eligibility into a flag
/// and gate the rewrite loop, rather than early-returning.
void enforce_spare_bit_pattern(LdpcSubframe type, std::span<uint8_t> msg) noexcept {
  // Defence-in-depth: only SF2 is centrally enforced, and the span must
  // cover the full spare region. Nominal SF2 path passes 1200 bytes.
  const bool eligible = (type == LdpcSubframe::kSF2) &&
                        // NOCOVER(DEFENCE_IN_DEPTH): SEU guard — not reachable nominally
                        (msg.size() >= static_cast<size_t>(kSb2SpareOffset) + kSb2SpareBits);
  if (eligible) {
    for (uint8_t i = 0; i < kSb2SpareBits; ++i) {
      msg[static_cast<size_t>(kSb2SpareOffset) + i] = static_cast<uint8_t>(i % 2U);
    }
  }
}

// Validate that input data and workspace buffers do not alias the output frame.
[[nodiscard]] bool validate_frame_inputs(std::span<const uint8_t> sb2_data,
                                         std::span<const uint8_t> sb3_data,
                                         std::span<const uint8_t> sb4_data,
                                         std::span<uint8_t> out,
                                         const FrameBuildWorkspace& frame_workspace,
                                         const LdpcEncodeWorkspace& ldpc_workspace) noexcept {
  if (spans_overlap(sb2_data.data(), sb2_data.size_bytes(), out.data(), out.size_bytes()) ||
      spans_overlap(sb3_data.data(), sb3_data.size_bytes(), out.data(), out.size_bytes()) ||
      spans_overlap(sb4_data.data(), sb4_data.size_bytes(), out.data(), out.size_bytes())) {
    return false;
  }
  if (spans_overlap(frame_workspace.staging.data(),
                    frame_workspace.staging.size(),
                    out.data(),
                    out.size_bytes()) ||
      spans_overlap(&ldpc_workspace, sizeof(ldpc_workspace), out.data(), out.size_bytes())) {
    return false;
  }
  return true;
}

// Encode BCH header. Returns true on success, sets status on failure.
[[nodiscard]] bool
encode_bch_header(Fid fid, Toi toi, std::span<uint8_t> sb1_out, FrameStatus& status) noexcept {
  const auto bch_status = bch_encode(fid, toi, sb1_out);
  if (bch_status == BchStatus::kOk) {
    return true;
  }
  switch (bch_status) {
  case BchStatus::kInvalidFid:
    status = FrameStatus::kInvalidFid;
    break;
  case BchStatus::kInvalidToi:
    status = FrameStatus::kInvalidToi;
    break;
  default: // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): DEAD_BY_DESIGN — MISRA Rule 16.4 default case
    status = FrameStatus::kBchFailed;
    break; // NOCOVER_END
  }
  return false;
}

// Internal helper: CRC-24 + LDPC encode pipeline for SB2/SB3/SB4. [LSIS-AFS-610]
// Single-exit pattern per mission assurance §3.
// @pre data.size() == expected_data_bits
// @pre out.size() == expected_out_len
// @return FrameStatus::kOk or an error code.
[[nodiscard]] FrameStatus subframe_build_ldpc(LdpcSubframe type,
                                              std::span<const uint8_t> data,
                                              std::span<uint8_t> out,
                                              LdpcEncodeWorkspace& workspace,
                                              uint16_t expected_data_bits,
                                              uint16_t expected_msg_bits,
                                              uint16_t expected_out_len) noexcept {
  FrameStatus status = FrameStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicSb;

  // Message buffer: data + CRC-24.  Max 1200 (SF2). Stack-allocated (§1).
  alignas(16) std::array<uint8_t, 1200> msg_buf{};
  std::array<uint8_t, kCrc24Bits> crc{};

  // --- Precondition checks (C4: bounds validation) ---
  bool ok = (data.size() == static_cast<size_t>(expected_data_bits)) &&
            (out.size() == static_cast<size_t>(expected_out_len)) &&
            (expected_msg_bits == expected_data_bits + kCrc24Bits) &&
            (expected_msg_bits <= 1200U) && !spans_overlap(data, out);

  // Step 1: Copy data into message buffer.
  if (ok) {
    auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
    std::ranges::copy(data, msg.begin());
    LUNALINK_CRITICAL_ENTER;
    enforce_spare_bit_pattern(type, msg);
    cfi_count = cfi_count + 1U; // milestone 1: data copy + LSIS-300 spare rewrite (FM-103)
    LUNALINK_CRITICAL_EXIT;
  }

  // Step 2: Compute CRC-24Q over data and append.
  // cppcheck-suppress duplicateCondition
  if (ok) {
    // CRC-24Q must cover the *post-enforcement* data — not the caller's
    // original span — otherwise receivers will reject the frame after the
    // SB2 spare region is rewritten by `enforce_spare_bit_pattern` (G-18).
    const auto msg_data = std::span<const uint8_t>(msg_buf).subspan(0, data.size());
    const auto crc_status = crc24_calculate(msg_data, crc);
    if (crc_status != CrcStatus::kOk) [[unlikely]] {
      ok = false; // NOCOVER(DEFENCE_IN_DEPTH): CRC failure in subframe_build_ldpc
      status = FrameStatus::kCrcFailed; // FIZ-1
    } else {
      auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
      std::ranges::copy(crc, msg.begin() + static_cast<ptrdiff_t>(data.size()));
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U; // milestone 2: CRC appended
      LUNALINK_CRITICAL_EXIT;
    }
  }

  // Step 3: LDPC encode message → encoded symbols.
  if (ok) {
    auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
    const auto ldpc_status = ldpc_encode(type, msg, out, workspace);
    if (ldpc_status != LdpcStatus::kOk) [[unlikely]] {
      ok = false; // NOCOVER(DEFENCE_IN_DEPTH): LDPC failure in subframe_build_ldpc
      status = FrameStatus::kLdpcFailed; // FIZ-1
    } else {
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U; // milestone 3: LDPC done
      LUNALINK_CRITICAL_EXIT;
    }
  }

  // CFI terminal check: all 3 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicSb + 3U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected; // FIZ-1 (CFI violation) // NOCOVER(DEFENCE_IN_DEPTH):
                                            // CFI terminal check in subframe_build_ldpc
    }
  }

  // Cleanup: scrub output on fault; always scrub temporaries (§7).
  if (status != FrameStatus::kOk) {
    secure_scrub(out); // NOCOVER(DEFENCE_IN_DEPTH): scrub on fault in subframe_build_ldpc
  }
  secure_scrub(msg_buf);
  secure_scrub(crc);

  return status;
}

// Subframe build parameters for the consolidated loop.
struct SubframeBuildParams {
  LdpcSubframe type;
  std::span<const uint8_t> data;
  size_t staging_offset;
  uint16_t data_bits;
  uint16_t msg_bits;
  uint16_t out_len;
};

// Build SB2, SB3, SB4 into staging buffer. Returns true if all succeed.
[[nodiscard]] bool build_subframes(std::span<const SubframeBuildParams> params,
                                   std::span<uint8_t> staging,
                                   LdpcEncodeWorkspace& ldpc_workspace,
                                   volatile uint32_t& cfi_count,
                                   FrameStatus& status) noexcept {
  for (size_t i = 0; i < params.size(); ++i) {
    const auto& p = params[i];
    const auto s = subframe_build_ldpc(p.type,
                                       p.data,
                                       staging.subspan(p.staging_offset, p.out_len),
                                       ldpc_workspace,
                                       p.data_bits,
                                       p.msg_bits,
                                       p.out_len);
    if (s != FrameStatus::kOk) [[unlikely]] {
      status = s; // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): build_subframes failure path
      return false;
    } // NOCOVER_END
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U;
    LUNALINK_CRITICAL_EXIT;
  }
  return true;
}

} // anonymous namespace

// --- Public subframe builders ---

FrameStatus subframe_build_sb2(std::span<const uint8_t, kSb2DataBits> data,
                               std::span<uint8_t, kSb2Length> out,
                               LdpcEncodeWorkspace& workspace) noexcept {
  return subframe_build_ldpc(
      LdpcSubframe::kSF2, data, out, workspace, kSb2DataBits, kSb2MsgBits, kSb2Length);
}

FrameStatus subframe_build_sb3(std::span<const uint8_t, kSb3DataBits> data,
                               std::span<uint8_t, kSb3Length> out,
                               LdpcEncodeWorkspace& workspace) noexcept {
  return subframe_build_ldpc(
      LdpcSubframe::kSF3, data, out, workspace, kSb3DataBits, kSb3MsgBits, kSb3Length);
}

FrameStatus subframe_build_sb4(std::span<const uint8_t, kSb4DataBits> data,
                               std::span<uint8_t, kSb4Length> out,
                               LdpcEncodeWorkspace& workspace) noexcept {
  return subframe_build_ldpc(
      LdpcSubframe::kSF4, data, out, workspace, kSb4DataBits, kSb4MsgBits, kSb4Length);
}

// --- Full frame assembler ---

FrameStatus frame_build(Fid fid,
                        Toi toi,
                        std::span<const uint8_t, kSb2DataBits> sb2_data,
                        std::span<const uint8_t, kSb3DataBits> sb3_data,
                        std::span<const uint8_t, kSb4DataBits> sb4_data,
                        std::span<uint8_t, kFrameLength> out,
                        LdpcEncodeWorkspace& ldpc_workspace,
                        FrameBuildWorkspace& frame_workspace) noexcept {
  FrameStatus status = FrameStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicFrame;
  bool ok =
      validate_frame_inputs(sb2_data, sb3_data, sb4_data, out, frame_workspace, ldpc_workspace);

  // ESA Safety Policy: Zero-initialise entire frame (skip if aliasing detected).
  // Step 1: Sync pattern (68 symbols, Table 12) — inside aliasing guard (P0-3).
  if (ok) {
    std::ranges::fill(out, 0U);
    std::ranges::copy(kSyncPattern, out.begin());
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // milestone 1: sync copied
    LUNALINK_CRITICAL_EXIT;
  }

  // Step 2: SB1 header — BCH(51,8) encode FID + TOI.
  // cppcheck-suppress duplicateCondition ; single-exit accumulator pattern
  if (ok) {
    ok = encode_bch_header(fid, toi, out.subspan<kSyncLength, kSb1Length>(), status);
    if (ok) {
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U;
      LUNALINK_CRITICAL_EXIT;
    } // milestone 2: SB1 done
  }

  // Steps 3-5: Build SB2, SB3, SB4 into staging buffer.
  auto& staging = frame_workspace.staging;

  if (ok) {
    std::ranges::fill(staging, 0U);
    const SubframeBuildParams sf_params[] = {
        {LdpcSubframe::kSF2, sb2_data, 0, kSb2DataBits, kSb2MsgBits, kSb2Length},
        {LdpcSubframe::kSF3, sb3_data, kSb2Length, kSb3DataBits, kSb3MsgBits, kSb3Length},
        {LdpcSubframe::kSF4,
         sb4_data,
         static_cast<size_t>(kSb2Length) + kSb3Length,
         kSb4DataBits,
         kSb4MsgBits,
         kSb4Length},
    };
    ok = build_subframes(
        std::span(sf_params), std::span(staging), ldpc_workspace, cfi_count, status);
  }

  // Step 6: 60×98 block interleave SB2+SB3+SB4 into frame payload.
  if (ok) {
    const auto int_status = block_interleave(
        staging, out.subspan<kSyncLength + static_cast<size_t>(kSb1Length), kPayloadLength>());
    if (int_status != InterleaverStatus::kOk) [[unlikely]] {
      ok = false; // NOCOVER(DEFENCE_IN_DEPTH): interleaver fault in frame_build
      status = FrameStatus::kFaultDetected; // FIZ-2
    } else {
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U; // milestone 6: interleave done
      LUNALINK_CRITICAL_EXIT;
    }
  }

  // CFI terminal check: all 6 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicFrame + 6U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected; // FIZ-2 (CFI violation) // NOCOVER(DEFENCE_IN_DEPTH):
                                            // CFI terminal check in frame_build
    }
  }

  // Cleanup: scrub frame on fault; always scrub staging (§7).
  if (status != FrameStatus::kOk) {
    secure_scrub(out);
  }
  secure_scrub(staging);

  return status;
}

} // namespace lunalink::signal
