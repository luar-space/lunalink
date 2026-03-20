// MISRA C++:2023 Rule 12.3.1 (Required): The union keyword shall not be used.
// Expected diagnostic: cppcoreguidelines-pro-type-union-access

#include <cstdint>

union TvsFloatBits {
  float f;
  uint32_t u;
};

uint32_t tvs_union_float_to_bits(float val) {
  TvsFloatBits fb;
  fb.f = val;
  return fb.u; // Union access — target violation
}
