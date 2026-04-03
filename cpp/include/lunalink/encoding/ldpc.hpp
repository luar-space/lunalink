#pragma once
/// @file ldpc.hpp
/// @brief LDPC encoder and sum-product BP decoder for LSIS-AFS subframes.
///
/// **Hardening rationale:** This open-source decoder uses float64 arithmetic
/// (not flight-suitable for targets without FPU). The structural hardening
/// patterns (CFI sentinels, TMR mirroring, bounds checks, secure scrub) are
/// retained deliberately: they demonstrate the assurance framework that the
/// flight decoder (proprietary AMS variant, Q8.7 integer) inherits. Removing
/// them from the OSS decoder would break the assurance evidence chain — the
/// SWIFI tests, FMEA, and fault_coverage.rst reference these patterns by name.
/// The hardening cost on desktop hardware is negligible (<2% overhead).

#include "lunalink/core/safety.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace lunalink::signal {

/** @brief Status codes for LDPC encode and layered sum-product belief-propagation decode.
 * [LSIS-AFS-320] SEU-hardened with high Hamming distance bit patterns. */
enum class LdpcStatus : uint8_t {
  kOk = 0x5AU,             ///< Operation completed successfully.
  kInvalidInput = 0xA5U,   ///< Input parameters out of range or invalid.
  kOutputTooSmall = 0x33U, ///< Output buffer too small for result.
  kFaultDetected = 0x99U,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
  kNotConverged = 0x66U,   ///< BP iterations exhausted without syndrome convergence.
};

/** @brief Subframe type selector for LDPC codec (SF2: rate-1/2 n=2400, SF3/SF4: rate-1/2 n=1740).
 * [LSIS-AFS-321] SEU-hardened with high Hamming distance bit patterns. */
enum class LdpcSubframe : uint8_t {
  kSF2 = 0x5AU, ///< Subframe 2: n=2400, k=1200, rate 1/2.
  kSF3 = 0xA5U, ///< Subframe 3: n=1740, k=870, rate 1/2.
  kSF4 = 0x33U, ///< Subframe 4: reuses SF3 matrices.
};

/**
 * @brief Scratchpad memory for LDPC encoding to keep large temporaries off stack.
 * SF2 mother code requires the largest buffers (~13.8 KB total).
 * Caller must allocate (heap or dedicated memory section on flight hardware).
 */
struct LdpcEncodeWorkspace {
  std::array<uint8_t, 1200> s{};
  std::array<uint8_t, 480> p1{};
  std::array<uint8_t, 4560> p2{};
  std::array<uint8_t, 480> t_as{};
  std::array<uint8_t, 4560> t_cs{};
  std::array<uint8_t, 4560> t_dp1{};

#ifdef LUNALINK_FAULT_INJECTION
  /// SWIFI: when non-zero, ldpc_encode() returns this status immediately
  /// without computing. Used for return-value injection testing. [SW-SAF-06]
  /// Sentinel 0 is not a valid LdpcStatus value (all status codes use
  /// SEU-hardened bit patterns with Hamming distance >= 3 from 0x00),
  /// so 0 safely means "no injection".
  uint8_t fi_force_status = 0;
#endif
};

/**
 * @brief Encode a navigation subframe using rate 1/2 LDPC. [LSIS-AFS-320]
 *
 * Two-stage sparse encoding: p1 = B^-1·(A·s), p2 = C·s + D·p1 (mod 2).
 * Includes post-encode H·c=0 self-check for fault detection.
 *
 * @param type Subframe type (SF2: 1200->2400, SF3/4: 870->1740).
 * @param msg  Input message bits (including 24-bit CRC).
 * @param out  Output encoded symbols.
 * @param workspace Scratchpad memory for encoder temporaries.
 *
 * @return LdpcStatus::kOk or error code.
 */
[[nodiscard]] LdpcStatus ldpc_encode(LdpcSubframe type,
                                     std::span<const uint8_t> msg,
                                     std::span<uint8_t> out,
                                     LdpcEncodeWorkspace& workspace) noexcept;

/**
 * @brief Scratchpad memory for LDPC decoding to ensure re-entrancy.
 * Large buffers are moved out of the function stack and static memory.
 * Uses float64 for exact sum-product belief propagation.
 *
 * WARNING: ~342 KB total. Must be heap-allocated; do NOT allocate on stack.
 */
struct LdpcWorkspace {
  std::array<double, 6240> L_v{};           // Variable-node LLRs (Mother length)
  std::array<double, 32000> L_cv{};         // Check-to-variable messages (Sparse parts)
  std::array<double, 4560> L_cv_identity{}; // Messages for identity part

#ifdef LUNALINK_FAULT_INJECTION
  /// SWIFI FI-41: when non-zero, XOR this mask into the phi_sum_mir field
  /// of the first row's RowGatherResult after gather. Exercises the TMR
  /// bitwise mirror detection path in verify_mirrors(). [SW-SAF-06]
  uint64_t fi_corrupt_phi_mirror = 0;
#endif
};

/**
 * @brief Decode a navigation sub-block using Layered Sum-Product BP. [LSIS-AFS-321]
 *
 * @param type       Subframe type.
 * @param symbols    Input channel LLRs (log-likelihood ratios).
 *                   For BPSK AWGN: L_ch = 2*y/sigma^2, where y is the
 *                   received sample and sigma^2 is the noise variance.
 *                   Positive = likely bit 0, negative = likely bit 1.
 * @param iterations Number of BP iterations.
 * @param out        Buffer for decoded bits (1200 or 870 bits).
 * @param workspace  Scratchpad memory for this decoding instance.
 *
 * @return LdpcStatus::kOk on convergence, kNotConverged if max iterations reached.
 */
[[nodiscard]] LdpcStatus ldpc_decode(LdpcSubframe type,
                                     std::span<const float> symbols,
                                     uint8_t iterations,
                                     std::span<uint8_t> out,
                                     LdpcWorkspace& workspace) noexcept;

} // namespace lunalink::signal
