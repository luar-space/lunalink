/**
 * @file test_swifi_codes.cpp
 * @brief SWIFI tests for codes subsystem: Legendre golden CRC integrity.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 */
#include "lunalink/codes/codebook_checksums.hpp"
#include "lunalink/codes/legendre.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// ---------------------------------------------------------------------------
// FI-15 — Legendre cache TOCTOU integrity gap closure (FM-005)
// ---------------------------------------------------------------------------
TEST_CASE("SWIFI: legendre_golden_crc — runtime checksums match build-time golden constants") {
  const uint64_t leg10223 = legendre_10223_checksum();
  const uint64_t leg1499 = legendre_1499_checksum();

  // Golden CRC match proves generation was correct (not self-signed).
  REQUIRE(leg10223 == kLegendre10223Checksum);
  REQUIRE(leg1499 == kLegendre1499Checksum);

  // Verify cached sequences are non-trivial (not all-zero).
  const auto seq10223 = get_cached_legendre(10223U);
  const auto seq1499 = get_cached_legendre(1499U);
  REQUIRE(!seq10223.empty());
  REQUIRE(!seq1499.empty());

  // At least one element must be non-zero (Legendre sequences are not trivial).
  bool has_nonzero_10223 = false;
  for (const auto v : seq10223) {
    if (v != 0U) {
      has_nonzero_10223 = true;
      break;
    }
  }
  REQUIRE(has_nonzero_10223);
}
