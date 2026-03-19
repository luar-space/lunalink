#include "lunalink/codes/gold.hpp"
#include <array>

namespace lunalink::signal {

namespace {
/**
 * @brief Appendix C Table C-1 through C-5 initialization values for G2 register.
 * LSIS Table C-1: PRN 1 G2 Init = 0x514.
 */
alignas(64) LUNALINK_LUT_SECTION constinit const std::array<uint16_t, 210> kGoldG2Init = {{
    0x514U, 0x59EU, 0x49AU, 0x346U, 0x78BU, 0x341U, 0x17DU, 0x7ABU, 0x301U, 0x32EU, // 1-10
    0x4DEU, 0x29CU, 0x4EDU, 0x506U, 0x30EU, 0x230U, 0x270U, 0x38DU, 0x2DCU, 0x5C8U, // 11-20
    0x61EU, 0x72CU, 0x54BU, 0x77CU, 0x32FU, 0x600U, 0x635U, 0x4B7U, 0x73DU, 0x4BEU, // 21-30
    0x55FU, 0x775U, 0x424U, 0x603U, 0x450U, 0x0ABU, 0x316U, 0x44FU, 0x477U, 0x76FU, // 31-40
    0x1F3U, 0x1B9U, 0x6BEU, 0x6EBU, 0x336U, 0x650U, 0x59CU, 0x74FU, 0x294U, 0x03FU, // 41-50
    0x5ABU, 0x4F2U, 0x647U, 0x494U, 0x521U, 0x53DU, 0x4DFU, 0x4A2U, 0x2C3U, 0x15FU, // 51-60
    0x3A2U, 0x2C9U, 0x329U, 0x214U, 0x532U, 0x733U, 0x697U, 0x6DDU, 0x2FCU, 0x471U, // 61-70
    0x42BU, 0x706U, 0x52FU, 0x77FU, 0x526U, 0x7FBU, 0x220U, 0x13CU, 0x65FU, 0x7E3U, // 71-80
    0x254U, 0x444U, 0x041U, 0x542U, 0x6EFU, 0x4F6U, 0x7C8U, 0x45FU, 0x654U, 0x659U, // 81-90
    0x486U, 0x72AU, 0x191U, 0x68DU, 0x0CDU, 0x5FEU, 0x604U, 0x282U, 0x36DU, 0x231U, // 91-100
    0x655U, 0x1FFU, 0x6FBU, 0x41FU, 0x6ACU, 0x026U, 0x298U, 0x612U, 0x1D0U, 0x2C7U, // 101-110
    0x534U, 0x7BEU, 0x7DAU, 0x51DU, 0x39AU, 0x786U, 0x354U, 0x5A4U, 0x1A4U, 0x529U, // 111-120
    0x4CEU, 0x5F2U, 0x2B0U, 0x30DU, 0x621U, 0x572U, 0x455U, 0x4C5U, 0x51EU, 0x676U, // 121-130
    0x53CU, 0x726U, 0x410U, 0x544U, 0x430U, 0x3ACU, 0x46BU, 0x389U, 0x7DEU, 0x3CDU, // 131-140
    0x7FDU, 0x0BBU, 0x60DU, 0x6F9U, 0x61AU, 0x7EAU, 0x798U, 0x75AU, 0x3C7U, 0x5D1U, // 141-150
    0x541U, 0x6B9U, 0x71EU, 0x33CU, 0x498U, 0x377U, 0x664U, 0x476U, 0x6BBU, 0x55EU, // 151-160
    0x340U, 0x4A0U, 0x1DEU, 0x65DU, 0x3D2U, 0x32CU, 0x2CDU, 0x50AU, 0x31AU, 0x459U, // 161-170
    0x750U, 0x5D2U, 0x4F3U, 0x779U, 0x53BU, 0x719U, 0x146U, 0x1E4U, 0x5E3U, 0x554U, // 171-180
    0x5B4U, 0x28BU, 0x226U, 0x594U, 0x4D4U, 0x48CU, 0x02CU, 0x44EU, 0x43AU, 0x6F0U, // 181-190
    0x7C5U, 0x331U, 0x7BDU, 0x443U, 0x46AU, 0x75BU, 0x357U, 0x22CU, 0x3B5U, 0x643U, // 191-200
    0x64BU, 0x4D3U, 0x2B2U, 0x530U, 0x76CU, 0x6E4U, 0x4B5U, 0x679U, 0x112U, 0x2DEU  // 201-210
}};
} // namespace

PrnStatus GoldGenerator::generate(PrnId prn_id, std::span<uint8_t> out) noexcept {
    PrnStatus status = PrnStatus::kOk;
    
    // CFI: Magic start sentinel
    // cppcheck-suppress variableScope -- CFI counter must be at function scope for terminal check
    uint32_t cfi_count = 0xA5A5A5A5U;

    if (!prn_id.valid()) {
        status = PrnStatus::kInvalidPrn;
    } else if (out.size() < kGoldChipLength) {
        status = PrnStatus::kNullOutput;
    }

    if (status == PrnStatus::kOk) {
        // Initialize registers per LSIS Appendix C.
        g1_reg_ = 0x7FFU; // 11 ones
        const uint8_t repaired = prn_id.repair();
        if (!prn_id.valid()) {
            status = PrnStatus::kFaultDetected;
        } else {
            const size_t idx = static_cast<size_t>(repaired) - 1U;
            g2_reg_ = kGoldG2Init.at(idx);
        }
    }

    if (status == PrnStatus::kOk) {
        // Mirrored loop counter.
        // cppcheck-suppress [variableScope, unreadVariable] -- mirror variable must be at function scope for fault detection
        uint32_t i_mir = 0xFFFFFFFFU;

        for (uint16_t i = 0; i < kGoldChipLength; ++i) {
            i_mir = ~static_cast<uint32_t>(i);

            if (static_cast<uint32_t>(i) != (~i_mir)) [[unlikely]] {
                status = PrnStatus::kFaultDetected;  // FIZ-1
                break;
            }

            out[i] = shift();
            cfi_count++;
            wip_tick();
        }

        // CFI: Terminal verification
        if (cfi_count != (0xA5A5A5A5U + kGoldChipLength)) [[unlikely]] {
            status = PrnStatus::kFaultDetected;  // FIZ-1
        }
    }

    if (status != PrnStatus::kOk) {
        secure_scrub(out);
    }

    return status;
}

uint8_t GoldGenerator::shift() noexcept {
    /**
     * @brief Implementation of dual 11-stage LFSR per LSIS Figure C-1.
     * Logic: Stage 11 of G1 is XORed with Stage 11 of G2 to produce the chip.
     * G1 Polynomial: x^11 + x^2 + 1
     * G2 Polynomial: x^11 + x^8 + x^5 + x^2 + 1
     */
    
    const auto g1_out = static_cast<uint8_t>((static_cast<uint32_t>(g1_reg_) >> 10U) & 1U);
    const auto g2_out = static_cast<uint8_t>((static_cast<uint32_t>(g2_reg_) >> 10U) & 1U);
    const auto chip = static_cast<uint8_t>(g1_out ^ g2_out);

    // G1 feedback: tap 11 and stage 2 (XOR)
    const auto g1_fb = static_cast<uint16_t>(((static_cast<uint32_t>(g1_reg_) >> 10U) ^ 
                                              (static_cast<uint32_t>(g1_reg_) >> 1U)) & 1U);
    g1_reg_ = static_cast<uint16_t>((static_cast<uint32_t>(g1_reg_) << 1U) | 
                                     static_cast<uint32_t>(g1_fb)) & 0x7FFU;

    // G2 feedback: tap 11, 8, 5, 2 (XOR)
    const auto g2_fb = static_cast<uint16_t>(((static_cast<uint32_t>(g2_reg_) >> 10U) ^ 
                                              (static_cast<uint32_t>(g2_reg_) >> 7U) ^ 
                                              (static_cast<uint32_t>(g2_reg_) >> 4U) ^ 
                                              (static_cast<uint32_t>(g2_reg_) >> 1U)) & 1U);
    g2_reg_ = static_cast<uint16_t>((static_cast<uint32_t>(g2_reg_) << 1U) | 
                                     static_cast<uint32_t>(g2_fb)) & 0x7FFU;

    return chip;
}

} // namespace lunalink::signal
