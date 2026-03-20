// coverage_canary.hpp — Tool Validation: known-coverage canary functions.
//
// These functions have KNOWN coverage properties. The tool validation suite
// (scripts/validate_coverage.py) runs tests that exercise specific paths
// and then asserts that llvm-cov correctly reports which lines, branches,
// and MC/DC conditions are covered vs uncovered.
//
// This is analogous to DO-330 TQL-5 tool qualification by demonstration
// for the coverage measurement tool (llvm-cov).
//
// Standards traceability:
//   ECSS-Q-ST-80C §5.5   — tool confidence for verification tools
//   DO-330 TQL-5          — tool qualification by demonstration
//   MISRA Compliance:2020 — tool chain confidence

#pragma once

#include <cstdint>

namespace lunalink::canary {

// ──────────────────────────────────────────────────────────────────────
// Canary 1: Line coverage
// Lines 1-3 are exercised by test. Line 4 (the else branch) is NOT.
// Validates: llvm-cov correctly reports covered vs uncovered lines.
// ──────────────────────────────────────────────────────────────────────
inline int32_t line_coverage_canary(int32_t x) {
  int32_t result = 0; // COVERED
  if (x > 0) {        // COVERED (condition evaluated)
    result = x * 2;   // COVERED (true branch taken)
  } else {
    result = -1; // UNCOVERED (false branch never taken)
  }
  return result; // COVERED
}

// ──────────────────────────────────────────────────────────────────────
// Canary 2: Branch coverage
// The switch has 4 cases. Tests exercise cases 0, 1, 2 but NOT 3.
// Validates: llvm-cov correctly reports branch hit/miss.
// ──────────────────────────────────────────────────────────────────────
inline int32_t branch_coverage_canary(int32_t selector) {
  int32_t result = 0;
  switch (selector) {
  case 0:
    result = 10;
    break; // COVERED
  case 1:
    result = 20;
    break; // COVERED
  case 2:
    result = 30;
    break; // COVERED
  case 3:
    result = 40;
    break; // UNCOVERED (never selected)
  default:
    result = -1;
    break; // UNCOVERED (never selected)
  }
  return result;
}

// ──────────────────────────────────────────────────────────────────────
// Canary 3: MC/DC coverage
// Boolean decision: (a && b) || c
// Tests exercise: (T,T,F)=T, (F,T,F)=F, (T,F,F)=F, (F,F,T)=T
// This achieves MC/DC for all 3 conditions.
// One combination NOT tested: (T,T,T) — but MC/DC is still 100% because
// each condition independently affects the outcome in at least one test.
// Validates: llvm-cov MC/DC instrumentation correctly counts conditions.
// ──────────────────────────────────────────────────────────────────────
inline bool mcdc_full_canary(bool a, bool b, bool c) {
  return (a && b) || c;
}

// ──────────────────────────────────────────────────────────────────────
// Canary 4: MC/DC partial coverage
// Boolean decision: (p && q)
// Tests exercise ONLY (T,T)=T. Missing: (F,T)=F, (T,F)=F.
// MC/DC is NOT achieved — neither condition is shown to independently
// affect the outcome.
// Validates: llvm-cov correctly reports partial MC/DC.
// ──────────────────────────────────────────────────────────────────────
inline bool mcdc_partial_canary(bool p, bool q) {
  return p && q;
}

// ──────────────────────────────────────────────────────────────────────
// Canary 5: Fully uncovered function
// No test calls this function at all.
// Validates: llvm-cov correctly reports 0% function/line coverage.
// ──────────────────────────────────────────────────────────────────────
inline int32_t fully_uncovered_canary(int32_t x) {
  if (x > 100) {
    return x - 100;
  }
  return x + 100;
}

} // namespace lunalink::canary
