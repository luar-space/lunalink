// MISRA C++:2023 Rule 4.1.2 / 11.3.1 (Required): C-style arrays shall not be
// used; prefer std::array.
// Expected diagnostic: modernize-avoid-c-arrays

int tvs_c_array() {
    int arr[10];  // C-style array declaration — target violation
    arr[0] = 42;
    return arr[0];
}
