#pragma once
#include <cstdint>
#include <span>
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Weil code generator (LSIS Section 2.3.5.2).
 */
class WeilGenerator {
public:
    WeilGenerator() noexcept = default;

    /**
     * @brief Generate Primary Weil code (10230 chips). [LSIS-AFS-102]
     */
    [[nodiscard]] PrnStatus generate_primary(PrnId prn_id, std::span<uint8_t> out) noexcept;

    /**
     * @brief Generate Tertiary Weil code (1500 chips). [LSIS-AFS-102]
     */
    [[nodiscard]] PrnStatus generate_tertiary(PrnId prn_id, std::span<uint8_t> out) noexcept;

private:
    /**
     * @brief Internal core Weil construction logic.
     */
    [[nodiscard]] PrnStatus construct_weil(
        uint16_t           prime,
        uint16_t           weil_index,
        uint16_t           insertion_index,
        std::span<uint8_t> out) noexcept;
};

} // namespace lunalink::signal
