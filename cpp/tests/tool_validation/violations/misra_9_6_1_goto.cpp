// MISRA C++:2023 Rule 9.6.1 (Advisory, treated as Required by LunaLink):
// The goto statement should not be used.
// JPL Rule 11, Power of Ten Rule 1.
// Expected diagnostic: cppcoreguidelines-avoid-goto

int tvs_goto_sum_until(const int* data, int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        if (data[i] < 0) {
            goto done;  // target violation
        }
        total += data[i];
    }
done:
    return total;
}
