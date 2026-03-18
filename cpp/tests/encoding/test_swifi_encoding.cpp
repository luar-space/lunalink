/**
 * @file test_swifi_encoding.cpp
 * @brief SWIFI tests for encoding subsystem: LDPC matrix integrity.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include <catch2/catch_test_macros.hpp>

#include "lunalink/encoding/ldpc_tables.hpp"

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
