#include "lunalink/signal/interleaver.hpp"
#include <algorithm>

namespace lunalink::signal {

InterleaverStatus block_interleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept {

    InterleaverStatus status = InterleaverStatus::kOk;
    uint32_t cfi_count = 0xA5A5A5A5U;

    if (in.size() < kInterleaverSize) {
        status = InterleaverStatus::kInputTooSmall;
    } else if (out.size() < kInterleaverSize) {
        status = InterleaverStatus::kOutputTooSmall;
    } else if (in.data() == out.data()) {
        status = InterleaverStatus::kFaultDetected;
    }

    if (status == InterleaverStatus::kOk) {
        // Write row-wise (in[r*98 + c]), read column-wise (out[c*60 + r])
        // MA §6: Mirror index computation to detect ALU corruption.
        // Primary path computes in_idx/out_idx from (r, c).
        // Mirror path recomputes from independent accumulators and cross-checks.
        size_t in_idx_acc = 0;   // Accumulates r*98+c sequentially

        for (uint16_t r = 0; r < kInterleaverRows; ++r) {
            for (uint16_t c = 0; c < kInterleaverCols; ++c) {
                const size_t in_idx  = (static_cast<size_t>(r) * kInterleaverCols) + c;
                const size_t out_idx = (static_cast<size_t>(c) * kInterleaverRows) + r;

                // Cross-check: sequential accumulator must match computed index.
                if (in_idx != in_idx_acc) [[unlikely]] {
                    status = InterleaverStatus::kFaultDetected;
                    break;
                }

                out[out_idx] = in[in_idx];
                in_idx_acc++;
                cfi_count++;
                wip_tick();
            }
            if (status != InterleaverStatus::kOk) {
                break;
            }
        }

        if (status == InterleaverStatus::kOk) {
            // CFI: total elements processed must equal matrix size.
            if (cfi_count != (0xA5A5A5A5U + kInterleaverSize) ||
                in_idx_acc != kInterleaverSize) [[unlikely]] {
                status = InterleaverStatus::kFaultDetected;
            }
        }
    }

    if (status != InterleaverStatus::kOk) {
        secure_scrub(out);
    }

    return status;
}

InterleaverStatus block_deinterleave(
    std::span<const uint8_t> in,
    std::span<uint8_t>       out) noexcept {

    InterleaverStatus status = InterleaverStatus::kOk;
    uint32_t cfi_count = 0xA5A5A5A5U;

    if (in.size() < kInterleaverSize) {
        status = InterleaverStatus::kInputTooSmall;
    } else if (out.size() < kInterleaverSize) {
        status = InterleaverStatus::kOutputTooSmall;
    } else if (in.data() == out.data()) {
        status = InterleaverStatus::kFaultDetected;
    }

    if (status == InterleaverStatus::kOk) {
        // Write column-wise, read row-wise
        // Target: out[r * 98 + c] = in[c * 60 + r]
        size_t in_idx_acc = 0;

        for (uint16_t c = 0; c < kInterleaverCols; ++c) {
            for (uint16_t r = 0; r < kInterleaverRows; ++r) {
                const size_t in_idx  = (static_cast<size_t>(c) * kInterleaverRows) + r;
                const size_t out_idx = (static_cast<size_t>(r) * kInterleaverCols) + c;

                if (in_idx != in_idx_acc) [[unlikely]] {
                    status = InterleaverStatus::kFaultDetected;
                    break;
                }

                out[out_idx] = in[in_idx];
                in_idx_acc++;
                cfi_count++;
                wip_tick();
            }
            if (status != InterleaverStatus::kOk) {
                break;
            }
        }

        if (status == InterleaverStatus::kOk) {
            if (cfi_count != (0xA5A5A5A5U + kInterleaverSize) ||
                in_idx_acc != kInterleaverSize) [[unlikely]] {
                status = InterleaverStatus::kFaultDetected;
            }
        }
    }

    if (status != InterleaverStatus::kOk) {
        secure_scrub(out);
    }

    return status;
}

} // namespace lunalink::signal
