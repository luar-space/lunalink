// MISRA C++:2023 Rule 0.2.2 (Required): A named function parameter shall be
// used within the function body.
// Expected diagnostic: misc-unused-parameters

int tvs_unused_param(int x, int y) {
    return x + 1;  // Parameter 'y' is unused — target violation
}
