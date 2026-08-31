// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 28.6.3 (Required): An object shall not be read after it
// has been moved from.
// Expected diagnostic: bugprone-use-after-move

#include <string>
#include <utility>

std::string tvs_use_after_move() {
  std::string s = "hello";
  std::string t = std::move(s);
  return s; // Use after move — target violation
}
