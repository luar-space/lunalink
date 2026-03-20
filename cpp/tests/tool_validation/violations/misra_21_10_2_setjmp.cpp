// MISRA C++:2023 Rule 21.10.2 (Required): setjmp / longjmp shall not be used.
// Expected diagnostic: cert-err52-cpp

#include <csetjmp>

std::jmp_buf tvs_jump_buffer;

void tvs_setjmp_user() {
  if (setjmp(tvs_jump_buffer) != 0) { // Use of setjmp — target violation
    return;
  }
}
