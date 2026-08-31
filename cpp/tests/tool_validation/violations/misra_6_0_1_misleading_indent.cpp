// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 6.0.1 (Required): Statements shall not have misleading
// indentation.
// Expected diagnostic: readability-misleading-indentation

int tvs_misleading_indent(int x) {
  if (x > 0)
    x = x + 1;
    x = x + 2; // Misleading indentation — looks inside if but is not
  return x;
}
