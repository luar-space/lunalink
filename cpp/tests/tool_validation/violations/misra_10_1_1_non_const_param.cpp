// MISRA C++:2023 Rule 10.1.1 (Advisory): A parameter that is not modified
// should be declared const.
// Expected diagnostic: readability-non-const-parameter

int tvs_non_const_param(int* p) { // Pointer-to-non-const but *p not modified — target violation
  return *p + 1;
}
