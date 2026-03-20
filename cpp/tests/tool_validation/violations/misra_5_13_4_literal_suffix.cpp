// MISRA C++:2023 Rule 5.13.4 (Required): Literal suffixes shall be upper case.
// Expected diagnostic: hicpp-uppercase-literal-suffix

unsigned long tvs_literal_suffix() {
  unsigned long x = 42ul; // Lowercase literal suffix — target violation
  return x;
}
