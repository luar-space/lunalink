#pragma once
#include "lunalink/core/safety.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for Legendre sequence cache operations. SEU-hardened with high Hamming
 * distance bit patterns. */
enum class LegendreStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kOutputTooSmall = 0xCCU, ///< Output buffer too small for sequence.
  kInvalidPrime = 0x33U,   ///< Prime not in supported set {1499, 3607, 10223, 10243}.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/**
 * @brief Calculate Legendre symbol (a/p) mapped to [0, 1] per LSIS.
 * LSIS D-1: L(t)=1 if residue, L(t)=0 if non-residue or t=0.
 *
 * Uses Class A Hardening: CFI sentinels and mirrored calculation.
 */
[[nodiscard("LegendreStatus reports kInvalidPrime (not in {1499, 3607, 10223, 10243}), "
            "kOutputTooSmall, and CFI/TMR faults; ignoring it lets a zero/partial "
            "Legendre sequence propagate to Weil construction")]] LegendreStatus
generate_legendre_sequence(uint16_t prime, std::span<uint8_t> out) noexcept;

/**
 * @brief Explicitly initialise the Legendre sequence cache.
 *
 * Generates all four Legendre sequences (p=1499, p=3607, p=10223, p=10243) and verifies
 * them against build-time golden CRCs.  Must be called before any call to
 * get_cached_legendre().  Designed to be called from the POST sequence
 * to eliminate hidden __cxa_guard dynamic initialisation.
 *
 * @return LegendreStatus::kOk on success; kFaultDetected if golden CRC
 *         mismatch (corrupted generation).
 */
[[nodiscard("legendre_cache_init returns kFaultDetected on golden-CRC mismatch — a "
            "corrupted Legendre table at boot is a POST gate; ignoring it would let "
            "OPERATIONAL be entered with a corrupt cache (SW-SAF-04)")]] LegendreStatus
legendre_cache_init() noexcept;

/**
 * @brief Get a pointer to the hardened, pre-generated Legendre sequence.
 * This is the high-performance path for Weil code generation.
 *
 * @pre legendre_cache_init() must have returned kOk.
 * @return Non-empty span on success; empty span if not initialised or
 *         golden CRC verification fails.
 */
[[nodiscard]] std::span<const uint8_t> get_cached_legendre(uint16_t prime) noexcept;

/**
 * @brief Compute CRC-32 checksum of the cached Legendre sequence for prime 10223.
 *
 * Used by POST step 2 and the golden-checksum computation task.
 * Algorithm: CRC-32 (IEEE 802.3, poly 0xEDB88320, reflected).
 */
[[nodiscard]] uint64_t legendre_10223_checksum() noexcept;

/**
 * @brief Compute CRC-32 checksum of the cached Legendre sequence for prime 1499.
 */
[[nodiscard]] uint64_t legendre_1499_checksum() noexcept;

/**
 * @brief Compute CRC-32 checksum of the cached Legendre sequence for prime 3607.
 *
 * BeiDou B1C secondary codes use Weil-1800 with Legendre prime p=3607
 * (BDS-SIS-ICD-B1C-1.0 §5.2.2).
 */
[[nodiscard]] uint64_t legendre_3607_checksum() noexcept;

/**
 * @brief Compute CRC-32 checksum of the cached Legendre sequence for prime 10243.
 *
 * BeiDou B1C uses Weil-10230 with Legendre prime p=10243 (BDS-SIS-ICD-B1C-1.0).
 */
[[nodiscard]] uint64_t legendre_10243_checksum() noexcept;

} // namespace lunalink::signal
