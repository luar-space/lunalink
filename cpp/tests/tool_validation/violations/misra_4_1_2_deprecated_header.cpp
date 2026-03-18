// MISRA C++:2023 Rule 4.1.2 (Required): Deprecated headers shall not be used.
// Expected diagnostic: modernize-deprecated-headers

#include <stdlib.h>  // Deprecated C header — target violation

void tvs_deprecated_header() {
    int *p = static_cast<int *>(malloc(sizeof(int)));
    free(p);
}
