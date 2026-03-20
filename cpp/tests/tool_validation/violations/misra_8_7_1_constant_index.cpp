// MISRA C++:2023 Rule 8.7.1 (Required): Array indexing shall only be done
// with constant expressions.
// Expected diagnostic: cppcoreguidelines-pro-bounds-constant-array-index

#include <array>

int tvs_constant_index(int idx) {
  std::array<int, 4> arr = {1, 2, 3, 4};
  return arr[idx]; // Non-constant array index — target violation
}
