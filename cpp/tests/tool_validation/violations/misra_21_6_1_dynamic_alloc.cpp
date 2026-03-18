// MISRA C++:2023 Rule 21.6.1 (Advisory, treated as Required by LunaLink):
// Dynamic memory should not be used.
// JPL Rule 5, Power of Ten Rule 3.
// Expected diagnostic: cppcoreguidelines-no-malloc

#include <cstdlib>
#include <cstddef>

void* tvs_malloc_alloc(int size) {
    return malloc(static_cast<size_t>(size));  // malloc — target violation
}
