// NOLINTBEGIN(hicpp-use-auto) -- explicit types improve readability in CRC polynomial code
#include "lunalink/encoding/crc.hpp"
#include <algorithm>
#include <limits>

namespace lunalink::signal {

namespace {
/**
 * @brief CRC-24Q Polynomial: 0x864CFB.
 * Omitting the implicit X^24 bit.
 */
inline constexpr uint32_t kCrc24Poly = 0x864CFBU;
} // namespace

CrcStatus crc24_calculate(
    std::span<const uint8_t> in,
    std::span<uint8_t, 24ULL> out_crc) noexcept {

    // P0-6: Reject inputs that would truncate the CFI counter cast to uint32_t.
    if (in.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) [[unlikely]] {
        secure_scrub(out_crc);
        return CrcStatus::kFaultDetected;
    }

    CrcStatus status = CrcStatus::kOk;
    uint32_t cfi_count = 0xA5A5A5A5U;

    // Standard bit-by-bit CRC calculation (Galois LFSR)
    uint32_t register_state = 0U; // Seed is 0 per Appendix H

    // Mirrored loop index
    // cppcheck-suppress [variableScope, unreadVariable] -- mirror variable must be at function scope for fault detection
    size_t i_mir = 0xFFFFFFFFFFFFFFFFULL;

    for (size_t i = 0; i < in.size(); ++i) {
        i_mir = ~i;
        if (i != (~i_mir)) [[unlikely]] {
            status = CrcStatus::kFaultDetected;  // FIZ-1
            break;
        }

        const uint8_t bit = in[i] & 1U;
        const auto msb = static_cast<uint8_t>((register_state >> 23U) & 1U);
        
        register_state = (register_state << 1U) & 0xFFFFFFU;
        
        // Branchless: mask is 0xFFFFFFFF if (msb ^ bit) is 1, else 0
        const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(msb ^ bit)));
        register_state ^= (kCrc24Poly & mask);

        cfi_count++;
        wip_tick();
    }

    if (status == CrcStatus::kOk) {
        // CFI terminal check
        if (cfi_count != (0xA5A5A5A5U + static_cast<uint32_t>(in.size()))) [[unlikely]] {
            status = CrcStatus::kFaultDetected;  // FIZ-1
        } else {
            // Unpack 24-bit state to output bit-bytes
            for (uint8_t b = 0; b < 24U; ++b) {
                // MSB first in output sequence
                out_crc[b] = static_cast<uint8_t>((register_state >> (23U - b)) & 1U);
            }
        }
    }

    // Class A: Scrub internal state
    secure_scrub_scalar(register_state);

    if (status != CrcStatus::kOk) {
        secure_scrub(out_crc);
    }

    return status;
}

bool crc24_verify(
    std::span<const uint8_t> data_with_crc) noexcept {
    
    if (data_with_crc.size() < 24U) {
        return false;
    }

    const size_t data_len = data_with_crc.size() - 24U;
    std::array<uint8_t, 24> calc_crc{};
    
    const auto status = crc24_calculate(data_with_crc.subspan(0, data_len), calc_crc);
    
    if (status != CrcStatus::kOk) {
        return false;
    }

    bool match = true;
    for (size_t i = 0; i < 24U; ++i) {
        if (calc_crc.at(i) != (data_with_crc[data_len + i] & 1U)) {
            match = false;
        }
    }

    secure_scrub(calc_crc);
    return match;
}

} // namespace lunalink::signal
// NOLINTEND(hicpp-use-auto)
