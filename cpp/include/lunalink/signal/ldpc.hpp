#pragma once
#include <cstdint>
#include <span>
#include <array>
#include "lunalink/signal/safety.hpp"
#include "lunalink/signal/ldpc_tables.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for LDPC operations.
 */
enum class LdpcStatus : uint8_t {
    kOk            = 0x5AU,  // 01011010
    kInvalidInput  = 0xA5U,  // 10100101
    kOutputTooSmall = 0x33U,  // 00110011
    kFaultDetected = 0x99U,  // 10011001
};

/**
 * @brief LDPC Subframe types supported by LSIS-AFS.
 */
enum class LdpcSubframe : uint8_t {
    kSF2 = 2,
    kSF3 = 3,
    kSF4 = 4  // SF4 reuses SF3 matrices
};

/**
 * @brief Encode a navigation sub-block using LSIS-AFS LDPC(1200,600) or equivalent. [LSIS-AFS-320]
 *
 * Implements two-stage sparse encoding:
 * Stage 1: p1 = B^-1 * (A * s) mod 2
 * Stage 2: p2 = (C * s + D * p1) mod 2
 *
 * @param type Subframe type (SF2, SF3, or SF4).
 * @param msg  Input message bits (200 bits).
 * @param out  Buffer for encoded symbols (6000 for SF2, 4400 for SF3/4).
 *
 * @return LdpcStatus::kOk or error code.
 */
[[nodiscard]] LdpcStatus ldpc_encode(
    LdpcSubframe               type,
    std::span<const uint8_t>   msg,
    std::span<uint8_t>         out) noexcept;

} // namespace lunalink::signal
