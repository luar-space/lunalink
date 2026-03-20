// MISRA C++:2023 Rule 10.4.1 (Required): The asm declaration shall not be used.
// Expected diagnostic: hicpp-no-assembler

void tvs_asm_nop() {
  asm("nop"); // Inline assembly — target violation
}
