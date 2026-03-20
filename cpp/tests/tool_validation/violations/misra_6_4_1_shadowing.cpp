// MISRA C++:2023 Rule 6.4.1 (Required): A variable declared in an inner scope
// shall not hide a variable declared in an outer scope.
// JPL Rule 13.
// Expected diagnostic: bugprone-shadow

#include <cstdint>

int tvs_shadowing_compute(int x) {
  int val = x + 1;
  {
    int val = x + 2; // Shadows outer val — target violation
    return val;
  }
}
