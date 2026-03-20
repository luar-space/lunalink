// MISRA C++:2023 Rule 7.0.6 (Required): Implicit widening of a multiplication
// result shall not occur.
// Expected diagnostic: bugprone-implicit-widening-of-multiplication-result

#include <cstdint>

int64_t tvs_widening_mult(int32_t a, int32_t b) {
  int64_t result = a * b; // Implicit widening of multiplication — target violation
  return result;
}
