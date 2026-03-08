#include <cstdint>
#include <span>
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status for Legendre operations.
 */
enum class LegendreStatus : uint8_t {
    kOk            = 0x5AU,
    kOutputTooSmall = 0xCCU,
    kInvalidPrime   = 0x33U,
    kFaultDetected  = 0x99U
};

/**
 * @brief Calculate Legendre symbol (a/p) mapped to [0, 1] per LSIS.
 * LSIS D-1: L(t)=1 if residue, L(t)=0 if non-residue or t=0.
 * 
 * Uses Class A Hardening: CFI sentinels and mirrored calculation.
 */
[[nodiscard]] LegendreStatus generate_legendre_sequence(
    uint16_t           prime,
    std::span<uint8_t> out) noexcept;

/**
 * @brief Get a pointer to the hardened, pre-generated Legendre sequence.
 * This is the high-performance path for Weil code generation.
 */
[[nodiscard]] std::span<const uint8_t> get_cached_legendre(uint16_t prime) noexcept;

} // namespace lunalink::signal
