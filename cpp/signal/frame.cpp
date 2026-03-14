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

  // [LSIS-AFS-601] CFI sentinel for partial frame builder (§5).
  constexpr uint32_t kCfiMagicPartial = 0xD2D2D2D2U;

  FrameStatus status = FrameStatus::kFaultDetected;  // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicPartial;
  bool ok = true;

  // ESA Safety Policy: Zero-initialise entire frame.
  std::ranges::fill(out, 0U);

  // 1. Sync pattern: 68 symbols.
  std::ranges::copy(kSyncPattern, out.begin());
  cfi_count = cfi_count + 1U;  // milestone 1: sync copied

  // 2. SB1 (FID + TOI): 52 symbols BCH(51,8).
  {
    const auto bch_status = bch_encode(fid, toi,
                                       out.subspan<kSyncLength, kSb1Length>());
    if (bch_status != BchStatus::kOk) [[unlikely]] {
      ok = false;
      switch (bch_status) {
        case BchStatus::kInvalidFid: status = FrameStatus::kInvalidFid; break;
        case BchStatus::kInvalidToi: status = FrameStatus::kInvalidToi; break;
        default:                     status = FrameStatus::kBchFailed;  break;
      }
    } else {
      cfi_count = cfi_count + 1U;  // milestone 2: SB1 done
    }
  }

  // 3. SB2-SB4 payload: Interleaved block (5880 symbols).
  // Note: Builders for SB2-SB4 are implemented in Gateway 3.
  // We apply the interleaver to the zeroed buffer to ensure the structure is ready.
  if (ok) {
      // Temporary staging buffer for non-interleaved data (SB2+SB3+SB4)
      // Mandate: Zero dynamic allocation. Use stack.
      alignas(16) std::array<uint8_t, kPayloadLength> staging{};
      std::ranges::fill(staging, 0U);

      const auto int_status = block_interleave(staging,
          out.subspan<kSyncLength + static_cast<size_t>(kSb1Length),
                       kPayloadLength>());
      if (int_status != InterleaverStatus::kOk) [[unlikely]] {
          ok = false;
          status = FrameStatus::kFaultDetected;  // FIZ-2
      } else {
          cfi_count = cfi_count + 1U;  // milestone 3: interleave done
      }
      secure_scrub(staging);
  }

  // CFI terminal check: all 3 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicPartial + 3U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected;  // FIZ-2 (CFI violation)
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
    return FrameStatus::kFaultDetected;  // FIZ-2
  }
  return FrameStatus::kOk;
}

// ---------------------------------------------------------------------------
// Gateway 3: Subframe builders and full frame assembler
// ---------------------------------------------------------------------------

namespace {

// CFI magic patterns for subframe builder and frame assembler (§5).
constexpr uint32_t kCfiMagicSb    = 0xB4B4B4B4U;
constexpr uint32_t kCfiMagicFrame = 0x4B4B4B4BU;

// Internal helper: CRC-24 + LDPC encode pipeline for SB2/SB3/SB4. [LSIS-AFS-610]
// Single-exit pattern per mission assurance §3.
// @pre data.size() == expected_data_bits
// @pre out.size() == expected_out_len
// @return FrameStatus::kOk or an error code.
[[nodiscard]] FrameStatus subframe_build_ldpc(
    LdpcSubframe           type,
    std::span<const uint8_t> data,
    std::span<uint8_t>     out,
    LdpcEncodeWorkspace&   workspace,
    uint16_t               expected_data_bits,
    uint16_t               expected_msg_bits,
    uint16_t               expected_out_len) noexcept {

  FrameStatus status = FrameStatus::kFaultDetected;  // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicSb;

  // Message buffer: data + CRC-24.  Max 1200 (SF2). Stack-allocated (§1).
  alignas(16) std::array<uint8_t, 1200> msg_buf{};
  std::array<uint8_t, kCrc24Bits> crc{};

  // --- Precondition checks (C4: bounds validation) ---
  bool ok = (data.size() == static_cast<size_t>(expected_data_bits)) &&
            (out.size()  == static_cast<size_t>(expected_out_len)) &&
            (expected_msg_bits == expected_data_bits + kCrc24Bits) &&
            (expected_msg_bits <= 1200U) &&
            !spans_overlap(data, out);

  // Step 1: Copy data into message buffer.
  if (ok) {
    auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
    std::ranges::copy(data, msg.begin());
    cfi_count = cfi_count + 1U;  // milestone 1: data copied
  }

  // Step 2: Compute CRC-24Q over data and append.
  if (ok) {
    const auto crc_status = crc24_calculate(data, crc);
    if (crc_status != CrcStatus::kOk) [[unlikely]] {
      ok = false;
      status = FrameStatus::kCrcFailed;  // FIZ-1
    } else {
      auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
      std::ranges::copy(crc,
                        msg.begin() + static_cast<ptrdiff_t>(data.size()));
      cfi_count = cfi_count + 1U;  // milestone 2: CRC appended
    }
  }

  // Step 3: LDPC encode message → encoded symbols.
  if (ok) {
    auto msg = std::span(msg_buf).subspan(0, expected_msg_bits);
    const auto ldpc_status = ldpc_encode(type, msg, out, workspace);
    if (ldpc_status != LdpcStatus::kOk) [[unlikely]] {
      ok = false;
      status = FrameStatus::kLdpcFailed;  // FIZ-1
    } else {
      cfi_count = cfi_count + 1U;  // milestone 3: LDPC done
    }
  }

  // CFI terminal check: all 3 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicSb + 3U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected;  // FIZ-1 (CFI violation)
    }
  }

  // Cleanup: scrub output on fault; always scrub temporaries (§7).
  if (status != FrameStatus::kOk) {
    secure_scrub(out);
  }
  secure_scrub(msg_buf);
  secure_scrub(crc);

  return status;
}

}  // anonymous namespace

// --- Public subframe builders ---

FrameStatus subframe_build_sb2(
    std::span<const uint8_t, kSb2DataBits> data,
    std::span<uint8_t, kSb2Length>          out,
    LdpcEncodeWorkspace&                   workspace) noexcept {
  return subframe_build_ldpc(LdpcSubframe::kSF2, data, out, workspace,
                             kSb2DataBits, kSb2MsgBits, kSb2Length);
}

FrameStatus subframe_build_sb3(
    std::span<const uint8_t, kSb3DataBits> data,
    std::span<uint8_t, kSb3Length>          out,
    LdpcEncodeWorkspace&                   workspace) noexcept {
  return subframe_build_ldpc(LdpcSubframe::kSF3, data, out, workspace,
                             kSb3DataBits, kSb3MsgBits, kSb3Length);
}

FrameStatus subframe_build_sb4(
    std::span<const uint8_t, kSb4DataBits> data,
    std::span<uint8_t, kSb4Length>          out,
    LdpcEncodeWorkspace&                   workspace) noexcept {
  return subframe_build_ldpc(LdpcSubframe::kSF4, data, out, workspace,
                             kSb4DataBits, kSb4MsgBits, kSb4Length);
}

// --- Full frame assembler ---

FrameStatus frame_build(
    Fid                                    fid,
    Toi                                    toi,
    std::span<const uint8_t, kSb2DataBits> sb2_data,
    std::span<const uint8_t, kSb3DataBits> sb3_data,
    std::span<const uint8_t, kSb4DataBits> sb4_data,
    std::span<uint8_t, kFrameLength>       out,
    LdpcEncodeWorkspace&                   workspace) noexcept {

  FrameStatus status = FrameStatus::kFaultDetected;  // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicFrame;
  bool ok = true;

  // Aliasing check: input data buffers must not overlap output frame (§11).
  if (spans_overlap(sb2_data.data(), sb2_data.size_bytes(),
                    out.data(), out.size_bytes()) ||
      spans_overlap(sb3_data.data(), sb3_data.size_bytes(),
                    out.data(), out.size_bytes()) ||
      spans_overlap(sb4_data.data(), sb4_data.size_bytes(),
                    out.data(), out.size_bytes())) {
    ok = false;
  }

  // ESA Safety Policy: Zero-initialise entire frame.
  std::ranges::fill(out, 0U);

  // Step 1: Sync pattern (68 symbols, Table 12).
  std::ranges::copy(kSyncPattern, out.begin());
  cfi_count = cfi_count + 1U;  // milestone 1: sync copied

  // Step 2: SB1 header — BCH(51,8) encode FID + TOI.
  {
    const auto bch_status = bch_encode(fid, toi,
                                       out.subspan<kSyncLength, kSb1Length>());
    if (bch_status != BchStatus::kOk) [[unlikely]] {
      ok = false;
      switch (bch_status) {
        case BchStatus::kInvalidFid: status = FrameStatus::kInvalidFid; break;
        case BchStatus::kInvalidToi: status = FrameStatus::kInvalidToi; break;
        default:                     status = FrameStatus::kBchFailed;  break;
      }
    } else {
      cfi_count = cfi_count + 1U;  // milestone 2: SB1 done
    }
  }

  // Steps 3-5: Build SB2, SB3, SB4 into staging buffer.
  // Staging is pre-interleave: SB2(2400) + SB3(1740) + SB4(1740) = 5880.
  // Mandate: Zero dynamic allocation — use stack (§1).
  alignas(16) std::array<uint8_t, kPayloadLength> staging{};

  if (ok) {
    const auto sb2_s = subframe_build_ldpc(
        LdpcSubframe::kSF2, sb2_data,
        std::span(staging).subspan(0, kSb2Length),
        workspace, kSb2DataBits, kSb2MsgBits, kSb2Length);
    if (sb2_s != FrameStatus::kOk) [[unlikely]] {
      ok = false;
      status = sb2_s;  // FIZ-2: propagate block-level error
    } else {
      cfi_count = cfi_count + 1U;  // milestone 3: SB2 done
    }
  }

  if (ok) {
    const auto sb3_s = subframe_build_ldpc(
        LdpcSubframe::kSF3, sb3_data,
        std::span(staging).subspan(kSb2Length, kSb3Length),
        workspace, kSb3DataBits, kSb3MsgBits, kSb3Length);
    if (sb3_s != FrameStatus::kOk) [[unlikely]] {
      ok = false;
      status = sb3_s;  // FIZ-2
    } else {
      cfi_count = cfi_count + 1U;  // milestone 4: SB3 done
    }
  }

  if (ok) {
    const auto sb4_s = subframe_build_ldpc(
        LdpcSubframe::kSF4, sb4_data,
        std::span(staging).subspan(
            static_cast<size_t>(kSb2Length) + kSb3Length, kSb4Length),
        workspace, kSb4DataBits, kSb4MsgBits, kSb4Length);
    if (sb4_s != FrameStatus::kOk) [[unlikely]] {
      ok = false;
      status = sb4_s;  // FIZ-2
    } else {
      cfi_count = cfi_count + 1U;  // milestone 5: SB4 done
    }
  }

  // Step 6: 60×98 block interleave SB2+SB3+SB4 into frame payload.
  if (ok) {
    const auto int_status = block_interleave(
        staging,
        out.subspan<kSyncLength + static_cast<size_t>(kSb1Length),
                     kPayloadLength>());
    if (int_status != InterleaverStatus::kOk) [[unlikely]] {
      ok = false;
      status = FrameStatus::kFaultDetected;  // FIZ-2
    } else {
      cfi_count = cfi_count + 1U;  // milestone 6: interleave done
    }
  }

  // CFI terminal check: all 6 milestones must have been reached (§5).
  if (ok) {
    if (cfi_count == kCfiMagicFrame + 6U) {
      status = FrameStatus::kOk;
    } else {
      status = FrameStatus::kFaultDetected;  // FIZ-2 (CFI violation)
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
