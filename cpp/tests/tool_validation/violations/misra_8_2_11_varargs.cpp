// MISRA C++:2023 Rule 8.2.11 (Required): An argument passed via ellipsis shall
// have an appropriate type.
// Rule 21.10.1 (Required): The features of <cstdarg> shall not be used.
// Expected diagnostic: cppcoreguidelines-pro-type-vararg

#include <cstdarg>

int tvs_vararg_sum(int count, ...) { // Variadic function — target violation
  va_list args;
  va_start(args, count);
  int total = 0;
  for (int i = 0; i < count; ++i) {
    total += va_arg(args, int);
  }
  va_end(args);
  return total;
}
