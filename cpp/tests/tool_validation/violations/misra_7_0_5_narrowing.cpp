// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 7.0.5 (Required): Integral promotion and the usual
// arithmetic conversions shall not change the signedness or the type category
// of an operand.
// Expected diagnostic: bugprone-narrowing-conversions

#include <cstdint>

uint8_t tvs_narrow(uint32_t wide) {
  uint8_t result = wide; // Implicit narrowing — target violation
  return result;
}
