// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// Power of Ten Rule 4 / JPL Rule 25: Functions should be no longer than 60
// lines of text.
// Expected diagnostic: readability-function-size

int tvs_long_function(int x) {
  int a = x + 1;
  int b = a + 1;
  int c = b + 1;
  int d = c + 1;
  int e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  c = b + 1;
  d = c + 1;
  e = d + 1;
  a = e + 1;
  b = a + 1;
  return b; // 65+ lines — target violation
}
