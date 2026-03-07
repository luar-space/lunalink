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
This eliminates the risk of memory fragmentation, leaks, and non-deterministic 
allocation failures during flight.

2. Fixed-Bound Loops & No Recursion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
All loops must have a fixed, verifiable upper bound. Recursion is forbidden. 
This allows formal calculation of the **Worst-Case Stack Depth** and 
ensures the stack will never overflow into data segments.

3. Cyclomatic Complexity & Single Exit Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* **Complexity**: We enforce a limit of **10** per function.
* **Single Exit**: Functions utilize a single ``return`` statement. This 
  ensures that mandatory **Stack Scrubbing** and **Reciprocal Checks** 
  at the end of a function are never bypassed by an early exit.

4. Bidirectional Traceability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every mission-critical function in ``cpp/signal/`` is tagged with its 
corresponding LSIS requirement ID (e.g., ``[LSIS-AFS-101]``). This ensures 
that the implementation can be audited against the standard for 100% 
functional compliance.

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

3. Parameter Majority Voting (TMR) (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical parameters (e.g., ``PrnId``, ``Fid``) are stored in 
triplets. Reading a parameter invokes a **Majority Vote** (2-of-3).

* **Benefit**: If an SEU flips a bit in one copy of the parameter in RAM, 
  the software corrects the value on-the-fly without mission interruption.

4. Control-Flow Integrity (CFI) (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Radiation can cause the Program Counter to jump, leading to skipped 
logic or early loop exits.

* **Implementation**: Safety-critical loops implement independent terminal 
  count verification (**Sentinel Counters**).
* **Benefit**: Ensures that the entire information space (e.g., 400 BCH 
  codewords) was inspected and no radiation-induced "shortcuts" were taken.

5. Value Invariants & Saturating Logic (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Value-constrained wrappers for signal parameters (Phase Offsets, PRN IDs) 
ensure corrupted indices remain within safe buffer bounds. Logic uses 
**Saturating Arithmetic** rather than wrapping to prevent catastrophic 
index overflows.

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
explicitly zeroed (scrubbed) using ``std::fill`` before the function 
returns to prevent cross-task data leakage.

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
