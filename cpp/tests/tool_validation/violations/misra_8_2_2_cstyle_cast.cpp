// MISRA C++:2023 Rule 8.2.2 (Required): C-style casts and functional notation
// casts shall not be used.
// Expected diagnostic: cppcoreguidelines-pro-type-cstyle-cast

float tvs_cstyle_cast_convert(int x) {
    return (float)x;  // C-style cast — target violation
}
