// MISRA C++:2023 Rule 9.4.2 (Required): A switch-statement shall have a
// default label as the last switch-label.
// Expected diagnostic: bugprone-switch-missing-default-case

int tvs_switch_no_default(int x) {
  int result = 0;
  switch (x) { // Switch without default — target violation
  case 0:
    result = 1;
    break;
  case 1:
    result = 2;
    break;
  }
  return result;
}
