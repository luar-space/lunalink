#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/bch_table.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace lunalink::signal {

namespace {
/// Mask for the 52 active bits in the BCH codeword storage.
inline constexpr uint64_t kCodewordMask = 0x000FFFFFFFFFFFFFULL;
} // namespace

BchStatus bch_encode(Fid fid, Toi toi, std::span<uint8_t, 52ULL> out) noexcept {
    BchStatus status = BchStatus::kOk;
    // Use peek() to allow range validation to see the actual state
    const auto fid_val = fid.storage.peek().value();
    const auto toi_val = toi.storage.peek().value();

    std::ranges::fill(out, 0U);

    if (fid_val > kBchFidMax) [[unlikely]] {
        status = BchStatus::kInvalidFid;
    } else if (toi_val > kBchToiMax) [[unlikely]] {
        status = BchStatus::kInvalidToi;
    } else {
        const auto bit0 = static_cast<uint8_t>((static_cast<uint32_t>(fid_val) >> 1U) & 1U);
        auto state = static_cast<uint8_t>(((static_cast<uint32_t>(fid_val) & 1U) << 7U) | 
                                         (static_cast<uint32_t>(toi_val) & 0x7FU));

        constexpr uint8_t kFeedbackMask = 0b11111001U;
        out[0] = bit0;

        for (uint32_t k = 0; k < 51U; ++k) {
            const auto output = static_cast<uint8_t>((static_cast<uint32_t>(state) >> 7U) & 1U);
            const auto fb     = static_cast<uint8_t>(static_cast<uint32_t>(std::popcount(static_cast<uint32_t>(state & kFeedbackMask))) % 2U);
            state = static_cast<uint8_t>((static_cast<uint32_t>(state) << 1U) | fb);
            out[k + 1U] = static_cast<uint8_t>(output ^ bit0);
            wip_tick();
        }
    }
    return status;
}

BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept {
    uint64_t received = 0;
    for (uint32_t i = 0; i < 52U; ++i) {
        received |= (static_cast<uint64_t>(in[i] != 0U) << i);
        wip_tick();
    }

    uint32_t min_dist = 64U;
    uint32_t best_idx = 0U;
    uint32_t amb_mask = 0U;
    uint32_t cfi_count = 0xA5A5A5A5U;

    for (uint32_t i = 0; i < kBchCodebook.size(); ++i) {
        const uint64_t codeword = kBchCodebook.at(i);
        const auto dist = static_cast<uint32_t>(std::popcount((received ^ codeword) & kCodewordMask));
        
        bool is_new_min = (dist < min_dist);
        amb_mask = is_new_min ? 0U : (dist == min_dist ? 1U : amb_mask);
        best_idx = is_new_min ? i : best_idx;
        min_dist = is_new_min ? dist : min_dist;
        
        cfi_count++;
        wip_tick();
    }

    // Single exit: CFI failure sets status rather than early-returning.
    BchStatus status = BchStatus::kOk;
    if (cfi_count != (0xA5A5A5A5U + static_cast<uint32_t>(kBchCodebook.size()))) [[unlikely]] {
        status = BchStatus::kFaultDetected;  // FIZ-1
    } else if (amb_mask != 0U && min_dist != 0U) {
        status = BchStatus::kAmbiguousMatch;
    } else if (min_dist > 2U) {
        status = BchStatus::kNullOutput;
    }

    const bool fault = (status == BchStatus::kFaultDetected);
    const auto fid = Fid(fault ? 0U : static_cast<uint8_t>(best_idx / 100U));
    const auto toi = Toi(fault ? 0U : static_cast<uint8_t>(best_idx % 100U));
    return BchResult(status, fid, toi, fault ? 255U : static_cast<uint8_t>(min_dist));
}

BchResult bch_decode_soft(std::span<const float, 52ULL> llr) noexcept {
    // Physical saturation limit: no BPSK AWGN channel produces |LLR| > 100.
    // Clamping prevents float accumulation overflow over 52 additions.
    constexpr float kLlrClamp = 100.0F;

    // Input validation: set status flag, do not early-return (single exit mandate).
    // IEEE NaN comparisons are always false; an unguarded NaN would silently bias
    // best_metric, returning an incorrect FID/TOI with kOk.
    BchStatus status = BchStatus::kOk;
    for (uint32_t j = 0; j < 52U; ++j) {
        if (!std::isfinite(llr[j])) [[unlikely]] {
            status = BchStatus::kInvalidInput;
        }
        wip_tick();
    }

    // cppcheck-suppress variableScope
    float    best_metric  = -3.4e+38F; // -FLT_MAX without <cfloat>
    uint32_t best_idx     = 0U;
    uint32_t best_idx_mir = ~0U;  // MA §2 variable mirror: invariant best_idx ^ best_idx_mir == 0xFFFFFFFF
    // cppcheck-suppress variableScope
    uint32_t cfi_count    = 0xA5A5A5A5U;

    if (status == BchStatus::kOk) {
        for (uint32_t i = 0; i < kBchCodebook.size(); ++i) {
            const uint64_t codeword = kBchCodebook.at(i);
            float metric = 0.0F;
            for (uint32_t j = 0; j < 52U; ++j) {
                // BPSK mapping: bit=0 → +1, bit=1 → −1
                const float s  = 1.0F - 2.0F * static_cast<float>((codeword >> j) & 1ULL);
                const float cl = std::clamp(llr[j], -kLlrClamp, kLlrClamp);
                metric += cl * s;
                wip_tick();
            }
            if (metric > best_metric) {
                best_metric  = metric;
                best_idx     = i;
                best_idx_mir = ~i;  // keep mirror in sync
            }
            cfi_count++;
        }

        // CFI Verification
        if (cfi_count != (0xA5A5A5A5U + static_cast<uint32_t>(kBchCodebook.size()))) [[unlikely]] {
            status = BchStatus::kFaultDetected;  // FIZ-1
        }
        // Variable mirror integrity check: detects SEU corruption of best_idx mid-loop.
        if ((best_idx ^ best_idx_mir) != 0xFFFFFFFFU) [[unlikely]] {
            status = BchStatus::kFaultDetected;  // FIZ-1
        }
    }

    const bool fault = (status == BchStatus::kFaultDetected);
    const auto fid = Fid(fault ? 0U : static_cast<uint8_t>(best_idx / 100U));
    const auto toi = Toi(fault ? 0U : static_cast<uint8_t>(best_idx % 100U));
    // 0xFF: soft-decode sentinel — hamming_distance is not a Hamming distance here.
    return BchResult(status, fid, toi, 0xFFU);
}

uint64_t bch_codebook_checksum() noexcept {
    uint32_t crc = 0xFFFFFFFFU;
    for (const auto val : kBchCodebook) {
        for (uint32_t i = 0; i < 8U; i++) {
            crc ^= static_cast<uint8_t>((val >> (i * 8U)) & 0xFFU);
            for (uint32_t j = 0; j < 8U; j++) {
                crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
            }
        }
        wip_tick();
    }
    return ~(static_cast<uint64_t>(crc));
}

} // namespace lunalink::signal
