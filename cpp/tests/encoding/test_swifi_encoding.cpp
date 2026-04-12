/**
 * @file test_swifi_encoding.cpp
 * @brief SWIFI tests for encoding subsystem: LDPC matrix integrity,
 *        return-value injection.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"

#include <array>
#include <cmath>
#include <random>
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-05 — LDPC packed-CSR matrix single-bit flip: CRC-32 mismatch detected
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: FI-05 — lut_bit_flip_ldpc_matrix — integrity check fails") {
  // Baseline: uninjected matrix passes
  REQUIRE(kLdpc_sf2_a.verify_integrity() == true);

  // Inject a single bit flip at byte offset 0
  kLdpc_sf2_a.inject_bit_flip(0U, 0x01U);

  // verify_integrity() should now return false (CRC mismatch)
  const bool detected = !kLdpc_sf2_a.verify_integrity();
  REQUIRE(detected == true);

  // Clean up so subsequent tests are not affected
  kLdpc_sf2_a.clear_fault();
  REQUIRE(kLdpc_sf2_a.verify_integrity() == true);
}

// ---------------------------------------------------------------------------
// FI-27 — Return-value injection: ldpc_encode forced kFaultDetected
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: ldpc_encode_forced_return — fi_force_status returns injected status") {
  // Valid SF2 message (1200 zero bits)
  std::array<uint8_t, 1200> msg{};
  std::array<uint8_t, 2400> out{};
  out.fill(0xFFU); // poison to verify scrub
  LdpcEncodeWorkspace ws{};

  // Baseline: nominal encode succeeds
  REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg, out, ws) == LdpcStatus::kOk);

  // Inject forced return: kFaultDetected
  ws.fi_force_status = static_cast<uint8_t>(LdpcStatus::kFaultDetected);
  out.fill(0xFFU); // re-poison

  const auto status = ldpc_encode(LdpcSubframe::kSF2, msg, out, ws);
  REQUIRE(status == LdpcStatus::kFaultDetected);

  // Output must be scrubbed (all zero)
  for (const auto byte : out) {
    CHECK(byte == 0U);
  }

  // One-shot: next call without injection must succeed
  REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg, out, ws) == LdpcStatus::kOk);
}

// Note: SP decoder has no tunable parameters (no α/β), so there is no
// parameter-corruption SWIFI test. The AMS-specific β corruption test
// lives in the proprietary test suite.

// ---------------------------------------------------------------------------
// FI-41 — TMR phi_sum mirror corruption: verify_mirrors() detects bit flip
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: ldpc_decode_phi_mirror_corruption — TMR detects corrupted phi_sum_mir") {
  // Encode a valid SF2 frame
  std::array<uint8_t, 1200> msg{};
  std::array<uint8_t, 2400> encoded{};
  LdpcEncodeWorkspace enc_ws{};
  REQUIRE(ldpc_encode(LdpcSubframe::kSF2, msg, encoded, enc_ws) == LdpcStatus::kOk);

  // Generate noisy LLRs at 1.5 dB — needs BP iterations (not noiseless short-circuit)
  constexpr double kCodeRate = 0.5;
  const double eb_no_lin = std::pow(10.0, 1.5 / 10.0);
  const double sigma = 1.0 / std::sqrt(2.0 * kCodeRate * eb_no_lin);
  const double llr_scale = 2.0 / (sigma * sigma);
  std::mt19937 gen(42);
  std::normal_distribution<double> noise(0.0, sigma);

  std::array<float, 2400> soft{};
  for (size_t i = 0; i < 2400; ++i) {
    double mod = (encoded[i] == 1) ? -1.0 : 1.0;
    soft[i] = static_cast<float>((mod + noise(gen)) * llr_scale);
  }

  // Baseline: nominal decode converges
  std::array<uint8_t, 1200> decoded{};
  LdpcWorkspace ws{};
  REQUIRE(ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws) == LdpcStatus::kOk);

  // Inject: flip bit 0 of phi_sum_mir (XOR with 1).
  // This corrupts the TMR bitwise complement, triggering verify_mirrors() failure.
  ws.fi_corrupt_phi_mirror = 1;
  const auto status = ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws);
  REQUIRE(status == LdpcStatus::kFaultDetected);
}
