// MISRA C++:2023 Rule 0.1.2 (Required): The value returned by a function
// having a non-void return type that is not an overloaded operator shall be
// used. JPL Rule 14, Power of Ten Rule 7.
// Expected diagnostic: bugprone-unused-return-value

#include <vector>

void tvs_unused_return() {
  std::vector<int> v;
  v.empty(); // Ignoring [[nodiscard]] return value — target violation
}
