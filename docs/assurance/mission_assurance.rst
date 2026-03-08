Mission Assurance
=================

LunaLink is engineered for **Class A Flight Software (FSW)** standards, 
incorporating high-reliability patterns from NASA (JPL/Goddard), ESA (ESTEC), 
and JAXA (Tsukuba). This document details the engineering rigor applied 
to ensure deterministic, safe, and fault-tolerant operation in the extreme 
radiation environment of the Lunar surface.

Core Hardening Philosophy
-------------------------

Space software faces three critical risks not present in terrestrial applications:

1. **Silent Data Corruption (SDC)**: Radiation-induced Single Event Upsets (SEU) 
   can flip bits in registers or memory, potentially causing Undefined 
   Behavior (UB) or state corruption without crashing the system.
2. **Autonomous Survival**: A lunar payload must be able to detect and 
   mitigate internal faults without ground intervention.
3. **Timing Jitter**: Non-deterministic execution times in signal processing 
   can lead to phase-lock-loop (PLL) instability in the receiver.

The following measures address these risks systematically.

Design Constraints (NASA NPR 7150.2D / JPL Rule 1-10)
-----------------------------------------------------

1. Zero Dynamic Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~
Strictly zero heap usage (``malloc``/``new``) after the initialization phase. 

*   **Explanation**: Terrestrial software can often recover from a ``malloc`` failure by restarting. In flight, memory allocation failures due to fragmentation or leakage are mission-ending. Determinism is paramount; the software must know its exact memory footprint at compile time.
*   **Benefit**: Eliminates the risk of memory fragmentation and non-deterministic allocation failures.

2. Fixed-Bound Loops & No Recursion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
All loops must have a fixed, verifiable upper bound. Recursion is forbidden.

*   **Explanation**: Stack overflows are unpredictable and hard to debug without an OS. By forbidding recursion and bounding loops, we can mathematically guarantee that the stack will never collide with global data segments, even under worst-case execution paths.
*   **Benefit**: Allows formal calculation of the **Worst-Case Stack Depth**.

3. Cyclomatic Complexity & Single Exit Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Complexity**: We enforce a limit of **10** per function.
*   **Single Exit**: Functions utilize a single ``return`` statement.

*   **Explanation**: High complexity leads to "hidden" execution paths that are difficult to test with 100% path coverage. Single exit points guarantee that mandatory cleanup logic (like stack scrubbing) is always executed, preventing data leaks between tasks.
*   **Benefit**: Ensures that mandatory **Stack Scrubbing** and **Reciprocal Checks** are never bypassed.

4. Bidirectional Traceability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every mission-critical function in ``cpp/signal/`` is tagged with its 
corresponding LSIS requirement ID (e.g., ``[LSIS-AFS-101]``).

*   **Explanation**: Auditing flight software requires proof that every line of code exists for a reason defined in the requirements. Traceability prevents "feature creep" and ensures that every DSP block is functionally compliant with the interoperability standard.
*   **Benefit**: Ensures the implementation can be audited for 100% functional compliance.

Mission-Critical Coding Patterns
--------------------------------

1. SEU-Resistant Enumerations (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard sequential enums (0, 1, 2...) are vulnerable to single-bit flips. 
LunaLink utilizes **Non-Contiguous Bit Patterns** with high Hamming distance.

*   **Explanation**: Standard enums use sequential bits (00, 01, 10). A single radiation-induced bit flip can silently turn a "SUCCESS" state into a "CRITICAL_ERROR" state. High Hamming distance ensures that a bit flip results in an "ILLEGAL" pattern that the software can detect.
*   **Implementation**: Patterns like ``0x5A`` (01011010) and ``0xA5`` (10100101).
*   **Benefit**: A register bit-flip transforms a valid status into an identifiable "Invalid" state.

2. Variable Mirroring / Soft-TMR (JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical state variables in performance loops are "mirrored" to detect 
ALU or register file corruption *during* calculation.

*   **Explanation**: While RAM is often parity-protected, the ALU and CPU registers are typically not. Bit flips can occur *during* a calculation. Mirroring a variable with its bitwise inverse allows the CPU to detect if its own internal registers were corrupted while processing data.
*   **Implementation**: Variables like ``min_dist`` are paired with a bitwise inverse ``min_dist_mir``.
*   **Benefit**: Detects transient hardware faults in the middle of DSP blocks.

3. Parameter Majority Voting (TMR) (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical parameters (e.g., ``PrnId``, ``Fid``) are stored in 
triplets using the ``TmrValue<T>`` template.

*   **Explanation**: Storing three copies of a value reduces the probability of a system failure due to a single bit flip to near-zero. Even if one memory cell is permanently damaged by a heavy ion, the software can continue to operate correctly using the remaining two copies.
*   **Benefit**: If an SEU flips a bit in one copy, the software corrects the value on-the-fly.

4. Active TMR Repair (NASA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Passive majority voting is insufficient for long-duration missions. 

*   **Explanation**: If we only read the majority, the "bad" copy stays bad. Over time, a second bit might flip in a different copy, leading to a loss of majority. Active repair "scrubs" the fault as soon as it's detected, resetting the clock on fault accumulation.
*   **Implementation**: Done within the ``TmrValue::repair()`` logic.
*   **Benefit**: Prevents the accumulation of Single-Event Upsets (SEUs) over time.

5. Control-Flow Integrity (CFI) (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Radiation can cause the Program Counter to jump, leading to skipped 
logic or early loop exits.

*   **Explanation**: Radiation can flip bits in the Instruction Pointer, causing the CPU to jump over a critical safety check. Sentinel counters and terminal count verification prove that the loop actually executed the expected number of times.
*   **Implementation**: Safety-critical loops implement **Sentinel Counters**.
*   **Benefit**: Ensures the entire information space was inspected and no "shortcuts" were taken.

6. Value Invariants & Saturating Logic (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Value-constrained wrappers (``CheckedRange<T, Min, Max>``) ensure 
corrupted indices remain within safe buffer bounds.

*   **Explanation**: Standard C++ wrapping behavior (MAX + 1 = 0) can lead to catastrophic index errors. Saturating logic ensures that a corrupted input parameter stays at its maximum safe value, keeping the signal chain in a predictable state instead of causing an out-of-bounds crash.
*   **Benefit**: Logic uses **Saturating Arithmetic** to prevent catastrophic overflows.

7. Anti-Optimization (Secure Scrubbing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard memory clearing (e.g., ``std::fill``) is vulnerable to 
**Dead Store Elimination (DSE)** by optimizing compilers.

*   **Explanation**: Modern compilers may delete zeroing code if they determine the buffer is "never read again" before going out of scope. The ``volatile`` qualifier forces the compiler to respect the hardware-level requirement to physically erase sensitive data from RAM.
*   **Implementation**: The ``secure_scrub()`` utility utilizes a ``volatile`` pointer.
*   **Benefit**: Guarantees that sensitive signal data is physically erased from RAM.

8. Memory Section Pinning (JPL/Goddard)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical Look-Up Tables (LUTs) must not be left to the default 
placement of the linker.

*   **Explanation**: Some physical memory banks (like MRAM or PROM) are significantly more radiation-hardened than others. Pinning critical LUTs ensures they are mapped to the most reliable physical hardware available on the spacecraft.
*   **Implementation**: Large tables are tagged with the ``LUNALINK_LUT_SECTION`` macro.
*   **Benefit**: Allows mapping tables to the most radiation-hardened memory banks.

9. Temporal WIP Signaling (Watchdog Feed)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Long-running DSP loops must provide "proof of progress" to the system watchdog.

*   **Explanation**: A CPU can get stuck in an infinite loop due to a bit flip in a jump instruction. Traditional watchdogs only detect a total system crash. WIP signaling proves the software is making progress through its algorithmic steps, not just spinning.
*   **Implementation**: Safety-critical loops call ``wip_tick()``.
*   **Benefit**: Ensures the hardware watchdog can detect if the CPU is stuck.

10. Self-Validating Data Structures (JIT Integrity)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical Look-Up Tables (LUTs) and sparse matrices include embedded CRC32 
checksums.

*   **Explanation**: Large static matrices are the biggest targets for radiation in RAM. A single bit flip in a matrix entry can degrade the Bit Error Rate (BER) of the entire signal. JIT checks allow the system to detect if its reference data has been "poisoned" by cosmic rays.
*   **Implementation**: The ``LdpcCsrMatrix`` structure includes a ``crc32`` field checked by ``verify_integrity()``.
*   **Benefit**: Detects radiation-induced corruption of large tables in RAM before use.

Time-Deterministic Execution (NASA/JAXA)
----------------------------------------

1. Constant-Time DSP
~~~~~~~~~~~~~~~~~~~~
LunaLink prioritizes **Branchless Arithmetic** for signal generation (BPSK, 
multiplexing). By eliminating data-dependent branching, we ensure that the 
execution time is identical regardless of the input data.

* **Benefit**: Guarantees a stable **Worst-Case Execution Time (WCET)**, 
  crucial for real-time scheduling on RTOS like RTEMS or VxWorks.

2. Temporal Windowed Watchdogs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical tasks report their completion to a windowed watchdog. If a task 
finishes **too early** or **too late**, it indicates a timing fault 
(e.g., oscillator jitter or logic skip).

Fault Management & Telemetry
----------------------------

1. Continuous Built-In Test (CBIT) (ESA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In addition to Power-On Self Tests (POST), LunaLink supports periodic 
background scrubbing of critical Look-Up Tables (LUTs).

* **Implementation**: ``bch_codebook_checksum()`` is designed to be 
  called by a background "scrubber" task.
* **Benefit**: Detects Single-Bit Upsets in RAM-cached tables before 
  they can pollute signal generation.

2. Frame Commitment & Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
No signal frame is "released" to the hardware DMA/SDR layer until it has 
been structurally validated in RAM.

* **Requirement**: The final bit-stream must pass a **Post-Generation Checksum** 
  to ensure no bit-flips occurred during the assembly of the frame in local memory.

3. Forbidden State Detection (NASA/JPL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Any `default` case hit in a state transition (e.g., ``FrameStatus`` check) 
is treated as a **Critical Fault Event**, triggering a transition to a 
"Safe Mode" and logging a high-priority telemetry packet.

4. Fixed-Width Telemetry (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Error logging avoids variable-length strings (which risk stack corruption). 
LunaLink uses **Fixed-Width Binary Telemetry Packs**: 
`[Module ID (8b) | Error Code (8b) | Context (16b)]`.

Supply Chain Security & Reproducibility (ECSS-Q-ST-80C)
-------------------------------------------------------

1. Immutable Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~
All external dependencies (e.g., Catch2) are pinned by **Commit Hash (SHA)**, 
not mutable tags. This protects the mission from "Supply Chain Attacks" 
where an upstream tag is maliciously overwritten.

2. Bit-for-Bit Reproducible Builds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system enforces deterministic compilation.

* **Path Remapping**: ``-ffile-prefix-map`` is used to strip absolute 
  paths (e.g., ``/home/user/code``) from the binary debug symbols.
* **Verification**: The MD5 hash of the binary must be identical whether 
  built on a developer laptop or the flight CI server.

Verification & Tooling
----------------------

1. Digital Twin Compatibility (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The core library is strictly decoupled from hardware registers via a 
Hardware Abstraction Layer (HAL). This allows the **exact same binary** 
to be verified on a Linux Digital Twin and flight hardware (LEON/RISC-V).

2. Information Security (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
**Stack Scrubbing**: All internal temporary signal buffers are 
explicitly zeroed (scrubbed) using the flight-hardened ``secure_scrub()`` 
utility before the function returns to prevent cross-task data leakage.

3. Static & Dynamic Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* **Clang-Tidy**: Enforces MISRA-aligned High-Integrity C++ (HICPP) rules.
* **Cppcheck**: Secondary static analyzer focused on embedded safety-critical bugs (buffer overruns, memory leaks).
* **ASan/UBSan**: Catch memory safety and undefined behavior at runtime.
* **Coverage**: Mandates ≥ 90% C++ line coverage and 100% Python coverage.

4. Compiler-Level Defenses
~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system mandates strict hardening flags across all platforms:

* **Stack Protection**: ``-fstack-protector-strong`` to detect stack corruption.
* **Symbol Hardening**: ``-D_FORTIFY_SOURCE=2`` and ``-fno-common``.
* **Floating-Point Determinism**: ``-ffp-contract=off`` to ensure bit-for-bit parity between platforms.
* **Linker Hardening**: RELRO (Relocation Read-Only) and NX (No-Execute) stack configuration.
