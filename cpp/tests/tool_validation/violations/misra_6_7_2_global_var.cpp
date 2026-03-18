// MISRA C++:2023 Rule 6.7.2 (Required): Global variables shall not be used.
// Expected diagnostic: cppcoreguidelines-avoid-non-const-global-variables

int tvs_mutable_global = 42;  // Non-const global — target violation
