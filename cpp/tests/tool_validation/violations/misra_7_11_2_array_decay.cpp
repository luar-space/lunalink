// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 7.11.2 (Required): An array passed as a function argument
// shall not decay to a pointer.
// JPL Rule 26.
// Expected diagnostic: cppcoreguidelines-pro-bounds-array-to-pointer-decay

void tvs_array_decay_process(const int* data, int n) {
  (void)data;
  (void)n;
}

void tvs_array_decay_caller() {
  int buf[10] = {};
  tvs_array_decay_process(buf, 10); // Array-to-pointer decay — target violation
}
