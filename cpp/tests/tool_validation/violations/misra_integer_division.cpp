// Bugprone: Integer division result used in floating-point context loses
// precision.
// Expected diagnostic: bugprone-integer-division

double tvs_integer_division(int a, int b) {
  return a / b; // Integer division in floating-point context — target violation
}
