#pragma once
#include <cstdint>
#include <span>
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief 11-stage LFSR based Gold code generator (LSIS Figure C-1).
 */
class GoldGenerator {
public:
    static constexpr uint16_t kLfsrSize = 11;
    static constexpr uint16_t kG1Poly   = 0x402U; // x^11 + x^2 + 1 (binary: 100 0000 0010)
    static constexpr uint16_t kG2Poly   = 0x492U; // x^11 + x^8 + x^5 + x^2 + 1 (binary: 100 1001 0010)

    GoldGenerator() noexcept = default;

    /**
     * @brief Generate full 2046-chip sequence for a given PRN. [LSIS-AFS-101]
     */
    [[nodiscard]] PrnStatus generate(PrnId prn_id, std::span<uint8_t> out) noexcept;

private:
    uint16_t g1_reg_ = 0x7FFU; // Initial: all ones
    uint16_t g2_reg_ = 0x7FFU;

    [[nodiscard]] uint8_t shift() noexcept;
};

} // namespace lunalink::signal
