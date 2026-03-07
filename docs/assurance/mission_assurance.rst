Mission Assurance
=================

LunaLink is engineered for **Class A Flight Software (FSW)** standards, 
incorporating high-reliability patterns from NASA (JPL/Goddard), ESA (ESTEC), 
and JAXA (Tsukuba). This document details the engineering rigor applied 
to ensure deterministic, safe, and fault-tolerant operation in the extreme 
radiation environment of the Lunar surface.

Core Hardening Philosophy
-------------------------

Space software faces two critical risks not present in terrestrial applications:

1. **Silent Data Corruption**: Radiation-induced Single Event Upsets (SEU) 
   can flip bits in registers or memory, potentially causing Undefined 
   Behavior (UB) or state corruption without crashing the system.
2. **Autonomous Survival**: A lunar payload must be able to detect and 
   mitigate internal faults without ground intervention.

The following measures address these risks systematically.

Mission-Critical Coding Patterns
--------------------------------

1. SEU-Resistant Enumerations (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Standard sequential enums (0, 1, 2...) are vulnerable to single-bit flips. 
LunaLink utilizes **Non-Contiguous Bit Patterns** with high Hamming distance 
for all status codes (e.g., ``BchStatus``, ``FrameStatus``).

* **Implementation**: Patterns like ``0x5A`` (01011010) and ``0xA5`` (10100101).
* **Benefit**: A register bit-flip transforms a valid status into an 
  identifiable "Invalid" state rather than a different, but valid, state.

2. Variable Mirroring / Soft-TMR (JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Critical state variables in performance loops are "mirrored" to detect 
ALU or register file corruption *during* calculation.

* **Implementation**: Variables like ``min_dist`` are paired with 
  a bitwise inverse ``min_dist_mir``.
* **Verification**: At every iteration, the system verifies 
  ``min_dist == ~min_dist_mir``.
* **Benefit**: Detects transient hardware faults in the middle of 
  Digital Signal Processing (DSP) blocks.

3. Control-Flow Integrity (CFI) (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Radiation can cause the Program Counter to jump, leading to skipped 
logic or early loop exits.

* **Implementation**: Safety-critical loops implement independent terminal 
  count verification (Sentinel Counters).
* **Benefit**: Ensures that the entire information space (e.g., 400 BCH 
  codewords) was inspected and no radiation-induced "shortcuts" were taken.

4. Reciprocal Sanity Checks (NASA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Even if search logic completes, the result is mathematically verified 
via a separate execution path.

* **Implementation**: The BCH decoder re-encodes its "winner" and 
  verifies that the Hamming distance matches the search result.
* **Benefit**: Catches "Silent Data Corruption" where logic returns 
  a structurally valid but mathematically incorrect result.

5. Radiation Masking (JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Implementation**: Explicit bit-masking (``& kCodewordMask``) is 
  applied to memory fetches.
* **Benefit**: Prevents bit-flips in "unused" word bits (e.g., bits 53-63 
  of a 64-bit word storing a 52-bit codeword) from polluting calculations.

C++20 Engineering Standards
---------------------------

Standard: C++20 (ISO/IEC 14882:2020)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The project leverages modern C++20 safety features:

* **Strong Typing**: ``enum class Fid`` and ``struct Toi`` prevent 
  dimensional/unit errors (e.g., swapping FID and TOI).
* **Deterministic Initialization**: ``constinit`` guarantees compile-time 
  initialization of global tables (Sync Patterns, PRN LUTs), 
  eliminating boot-time non-determinism.
* **Bounds Safety**: ``std::span<T, N>`` provides the compiler with 
  buffer geometry, allowing formal proof of memory safety.

Deterministic Memory & Execution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **No Heap Allocation**: Strictly zero dynamic memory usage after startup.
* **No Exceptions/RTTI**: Core logic is compiled with ``-fno-exceptions`` 
  and ``-fno-rtti`` to ensure deterministic Worst-Case Execution Time (WCET).
* **Branchless DSP**: Critical paths (BPSK, validation) use branchless 
  arithmetic to eliminate timing side-channels and improve pipelining.

Verification & Tooling
----------------------

1. Static Analysis (Clang-Tidy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enforces High-Integrity C++ rules (MISRA-aligned). 
Checked families: ``bugprone-*``, ``cert-*``, ``cppcoreguidelines-*``, ``hicpp-*``.

2. Runtime Sanitizers (ASan/UBSan)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CI jobs execute the full suite with Address and Undefined Behavior 
sanitizers enabled to catch memory leaks or overflows.

3. Exhaustive Testing & Coverage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Python (src/)**: 100% statement and branch coverage requirement.
* **C++ (cpp/)**: ≥ 90% line coverage requirement (currently 98%).
* **Built-In Test (BIT)**: ``bch_codebook_checksum()`` allows flight 
  executives to verify the integrity of static LUTs in PROM/MRAM.

4. Information Security (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Stack Scrubbing**: All internal temporary signal buffers are 
explicitly zeroed (scrubbed) using ``std::fill`` before the function 
returns to prevent cross-task data leakage.
