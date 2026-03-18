// MISRA C++:2023 Rule 8.2.10 (Required): Functions shall not call themselves.
// JPL Rule 4, Power of Ten Rule 1.
// Expected diagnostic: misc-no-recursion
// This file must trigger misc-no-recursion to prove the check is active.

int tvs_factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * tvs_factorial(n - 1);  // Recursive call — target violation
}
