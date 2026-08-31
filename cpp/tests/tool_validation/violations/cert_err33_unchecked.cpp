// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// CERT ERR33-C / MISRA C++:2023 Rule 18.5.2 (Required): Return values from
// C library functions that may fail shall be checked.
// Expected diagnostic: cert-err33-c

#include <cstdio>

void tvs_unchecked_return() {
  std::fclose(stdin); // Unchecked return from fclose — target violation
}
