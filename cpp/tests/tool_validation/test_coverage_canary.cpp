// test_coverage_canary.cpp — Tool Validation: coverage canary tests.
//
// These tests exercise SPECIFIC paths in coverage_canary.hpp to create
// KNOWN coverage gaps. The validate_coverage.py script then verifies
// that llvm-cov correctly reports these gaps.
//
// DO NOT "fix" the coverage gaps — they are intentional.

#include "coverage_canary.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lunalink::canary;

// ── Canary 1: Line coverage ──────────────────────────────────────────
// Exercise ONLY the true branch (x > 0). The else branch must remain uncovered.
TEST_CASE("Coverage canary: line coverage — true branch only", "[canary]") {
    REQUIRE(line_coverage_canary(5) == 10);
    REQUIRE(line_coverage_canary(1) == 2);
    REQUIRE(line_coverage_canary(100) == 200);
    // Deliberately NOT testing x <= 0
}

// ── Canary 2: Branch coverage ────────────────────────────────────────
// Exercise cases 0, 1, 2 but NOT case 3 or default.
TEST_CASE("Coverage canary: branch coverage — cases 0,1,2 only", "[canary]") {
    REQUIRE(branch_coverage_canary(0) == 10);
    REQUIRE(branch_coverage_canary(1) == 20);
    REQUIRE(branch_coverage_canary(2) == 30);
    // Deliberately NOT testing selector==3 or default
}

// ── Canary 3: MC/DC full coverage ────────────────────────────────────
// Exercise combinations that achieve MC/DC for (a && b) || c:
//   (T,T,F)=T  — a=true independently affects outcome (compare with F,T,F)
//   (F,T,F)=F  — a=false independently affects outcome
//   (T,F,F)=F  — b=false independently affects outcome (compare with T,T,F)
//   (F,F,T)=T  — c=true independently affects outcome (compare with F,F,F→F)
TEST_CASE("Coverage canary: MC/DC full — all conditions independent", "[canary]") {
    REQUIRE(mcdc_full_canary(true, true, false) == true);
    REQUIRE(mcdc_full_canary(false, true, false) == false);
    REQUIRE(mcdc_full_canary(true, false, false) == false);
    REQUIRE(mcdc_full_canary(false, false, true) == true);
}

// ── Canary 4: MC/DC partial coverage ─────────────────────────────────
// Exercise ONLY (T,T)=T. MC/DC requires showing each condition independently
// affects the outcome, which needs at least (T,F)=F or (F,T)=F.
TEST_CASE("Coverage canary: MC/DC partial — only (T,T) tested", "[canary]") {
    REQUIRE(mcdc_partial_canary(true, true) == true);
    // Deliberately NOT testing (F,T), (T,F), or (F,F)
}

// ── Canary 5: Fully uncovered ────────────────────────────────────────
// NO test for fully_uncovered_canary(). It must show 0% coverage.
