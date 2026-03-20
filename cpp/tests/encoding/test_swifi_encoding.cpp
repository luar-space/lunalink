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
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// LDPC matrix bit-flip — verify_integrity() detects it
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: lut_bit_flip_ldpc_matrix — integrity check fails") {
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
