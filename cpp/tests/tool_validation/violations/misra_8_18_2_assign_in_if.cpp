// MISRA C++:2023 Rule 8.18.2 (Required): Assignment operators shall not be
// used in sub-expressions.
// Expected diagnostic: bugprone-assignment-in-if-condition

int tvs_assign_in_if(int x) {
    int y = 0;
    if (y = x) {  // Assignment in if condition — target violation
        return y;
    }
    return 0;
}
