// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 19.2.1 (Required): Header files shall have include
// guards.
// Expected diagnostic: llvm-header-guard
// NOTE: This header deliberately omits an include guard — target violation.

int tvs_header_func(int x);
