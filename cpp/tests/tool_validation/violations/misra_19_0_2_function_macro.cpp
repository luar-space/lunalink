// MISRA C++:2023 Rule 19.0.2 (Required): Function-like macros shall not be
// defined.
// JPL Rule 20, Power of Ten Rule 8.
// Expected diagnostic: cppcoreguidelines-macro-usage

#define TVS_SQUARE(x) ((x) * (x)) // Function-like macro — target violation

int tvs_macro_compute(int n) {
  return TVS_SQUARE(n);
}
