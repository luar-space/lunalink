// MISRA C++:2023 Rule 8.7.1 (Required): Pointer arithmetic shall not be used.
// JPL Rule 26, Power of Ten Rule 9.
// Expected diagnostic: cppcoreguidelines-pro-bounds-pointer-arithmetic

int tvs_pointer_arith(int *p) {
    return *(p + 1);  // Pointer arithmetic — target violation
}
