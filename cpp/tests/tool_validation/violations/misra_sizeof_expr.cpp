// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// Bugprone: Suspicious sizeof expression — sizeof on a pointer type when the
// array size was likely intended.
// Expected diagnostic: bugprone-sizeof-expression

#include <cstddef>

size_t tvs_sizeof_expr(int* p) {
  return sizeof(p) / sizeof(p[0]); // sizeof pointer, not array — target violation
}
