// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>

namespace lunalink::signal {

/** @brief Status codes for power-on self-test (POST). [SW-SAF-04] SEU-hardened with high Hamming
 * distance bit patterns. */
enum class PostStatus : uint8_t {
  kPass = 0x5AU,          ///< All POST steps completed successfully.
  kFaultDetected = 0x99U, ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/**
 * @brief Result of run_post(): status and the step number that failed (0 = none).
 */
struct PostResult {
  PostStatus status;
  uint8_t failed_step; // 0 = all passed; 1–5 = step that failed
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
[[nodiscard("PostResult.status reports the first failing POST step (SCI CRC, LUT CRCs, "
            "TMR self-test, stack sentinel, CfiGuard); the signal-chain FSM must not "
            "enter OPERATIONAL unless this is kPass (SW-SAF-04)")]] PostResult
run_post() noexcept;

} // namespace lunalink::signal
