// MISRA C++:2023 Rule 8.2.5 (Required): reinterpret_cast shall not be used.
// Expected diagnostic: cppcoreguidelines-pro-type-reinterpret-cast

#include <cstdint>

const uint8_t* tvs_reinterpret_alias(const float* p) {
  return reinterpret_cast<const uint8_t*>(p); // target violation
}
