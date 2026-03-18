// MISRA C++:2023 Rule 11.6.1 (Required): All variables shall be initialized
// at the point of declaration.
// Expected diagnostic: cppcoreguidelines-init-variables

int tvs_uninit_var() {
    int x;  // Uninitialized variable — target violation
    x = 42;
    return x;
}
