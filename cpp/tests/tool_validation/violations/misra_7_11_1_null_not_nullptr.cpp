// MISRA C++:2023 Rule 7.11.1 (Required): nullptr shall be the only form of the
// null-pointer-constant.
// Expected diagnostic: modernize-use-nullptr

#include <cstddef>

bool tvs_null_is_null(const int* p) {
    return p == NULL;  // target violation — use nullptr instead
}
