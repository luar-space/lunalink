// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 9.3.1 (Required): The body of an iteration-statement or a
// selection-statement shall be a compound-statement.
// Expected diagnostic: readability-braces-around-statements

int tvs_no_braces_abs(int x) {
  if (x < 0)
    x = -x; // No braces around single-statement body — target violation
  return x;
}
