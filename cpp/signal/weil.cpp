#include "lunalink/signal/weil.hpp"
#include "lunalink/signal/legendre.hpp"
#include "lunalink/signal/weil_parameters.hpp"
#include <array>

namespace lunalink::signal {

namespace {
/**
 * @brief Expansion sequence [0 1 1 0 1 0 0] per LSIS Section 2.3.5.2.
 */
inline constexpr std::array<uint8_t, 7> kExpansionSequence = {0, 1, 1, 0, 1, 0, 0};
} // namespace

PrnStatus WeilGenerator::generate_primary(PrnId prn_id, std::span<uint8_t> out) noexcept {
    if (!prn_id.valid()) {
        return PrnStatus::kInvalidPrn;
    }
    if (out.size() < kWeil10230ChipLength) {
        return PrnStatus::kNullOutput;
    }

    const size_t idx = static_cast<size_t>(prn_id.repair()) - 1U;
    const auto params = kPrimaryWeilParams.at(idx);

    return construct_weil(10223, params.k, params.p, out);
}

PrnStatus WeilGenerator::generate_tertiary(PrnId prn_id, std::span<uint8_t> out) noexcept {
    if (!prn_id.valid()) {
        return PrnStatus::kInvalidPrn;
    }
    if (out.size() < kWeil1500ChipLength) {
        return PrnStatus::kNullOutput;
    }

    const size_t idx = static_cast<size_t>(prn_id.repair()) - 1U;
    const uint16_t k = kTertiaryWeilIndices.at(idx);
    
    // LSIS Figure E-1: Tertiary insertion index is always 1 (prepend).
    const auto status = construct_weil(1499, k, 1, out.subspan(0, 1499));
    if (status == PrnStatus::kOk) {
        out[1499] = 0U; // Append zero per Figure E-1.
    }
    return status;
}

PrnStatus WeilGenerator::construct_weil(
    uint16_t           prime,
    uint16_t           weil_index,
    uint16_t           insertion_index,
    std::span<uint8_t> out) noexcept {
    
    const auto legendre = get_cached_legendre(prime);
    if (legendre.empty()) [[unlikely]] {
        return PrnStatus::kFaultDetected;  // FIZ-1
    }

    uint32_t cfi_count = 0xA5A5A5A5U;
    uint16_t out_idx = 0;
    uint16_t t_mir = 0xFFFFU;

    for (uint16_t t = 0; t < prime; ++t) {
        t_mir = static_cast<uint16_t>(~t);
        if (t != static_cast<uint16_t>(~t_mir)) [[unlikely]] {
            return PrnStatus::kFaultDetected;  // FIZ-1
        }

        if (out_idx == (insertion_index - 1U)) {
            for (uint8_t chip : kExpansionSequence) {
                if (out_idx < out.size()) {
                    out[out_idx++] = chip;
                }
            }
        }

        const auto shifted_t = static_cast<uint16_t>((static_cast<uint32_t>(t) + weil_index) % prime);
        const auto weil_chip = static_cast<uint8_t>(legendre[t] ^ legendre[shifted_t]);
        
        if (out_idx < out.size()) {
            out[out_idx++] = weil_chip;
        }
        
        cfi_count++;
        wip_tick();
    }

    if (cfi_count != (0xA5A5A5A5U + prime)) [[unlikely]] {
        return PrnStatus::kFaultDetected;  // FIZ-1
    }

    return PrnStatus::kOk;
}

} // namespace lunalink::signal
