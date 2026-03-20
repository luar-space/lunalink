// Companion source for misra_19_2_1_no_header_guard.hpp.
// The header deliberately has no include guard; clang-tidy scans this file.
// Expected diagnostic: llvm-header-guard (on the included header)

#include "misra_19_2_1_no_header_guard.hpp"

int tvs_header_func(int x) {
  return x + 1;
}
