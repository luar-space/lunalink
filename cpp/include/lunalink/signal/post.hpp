#pragma once
#include <cstdint>

namespace lunalink::signal {

/**
 * @brief Power-On Self-Test result status codes.
 * High Hamming distance patterns resist SEU bit-flips.
 */
enum class PostStatus : uint8_t {
    kPass         = 0x5AU,  // 01011010
    kFaultDetected = 0x99U, // 10011001
};

/**
 * @brief Result of run_post(): status and the step number that failed (0 = none).
 */
struct PostResult {
    PostStatus status;
    uint8_t    failed_step;  // 0 = all passed; 1–5 = step that failed
};

/**
 * @brief Run the 5-step Power-On Self-Test sequence. [SW-SAF-04]
 *
 * Steps:
 *   1. SCI header CRC-32 verification (binary integrity)
 *   2. LUT CRCs: LDPC, BCH, Weil, Legendre codebook integrity
 *   3. TMR mechanism self-test
 *   4. Stack sentinel integrity
 *   5. CfiGuard baseline self-test
 *
 * Returns {kPass, 0} on full pass, or {kFaultDetected, N} where N is the
 * first failing step. The signal chain FSM must not enter OPERATIONAL until
 * run_post() returns kPass.
 */
[[nodiscard]] PostResult run_post() noexcept;

}  // namespace lunalink::signal
