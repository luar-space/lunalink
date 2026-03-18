// MISRA C++:2023 Rule 8.2.3 (Required): A cast shall not remove any const or
// volatile qualification from the type accessed via a pointer or by reference.
// Expected diagnostic: cppcoreguidelines-pro-type-const-cast

void tvs_const_cast_mutate(const int* p) {
    int* q = const_cast<int*>(p);  // target violation
    *q = 42;
}
