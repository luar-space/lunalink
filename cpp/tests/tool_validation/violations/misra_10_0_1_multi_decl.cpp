// MISRA C++:2023 Rule 10.0.1 (Required): Each variable shall be declared in a
// separate declaration statement.
// Expected diagnostic: readability-isolate-declaration

int tvs_multi_decl() {
  int a = 1, b = 2; // Multiple declarations per statement — target violation
  return a + b;
}
