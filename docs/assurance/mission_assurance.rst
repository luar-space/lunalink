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

*   **Explanation**: In long-duration space missions, software must operate without human intervention for years. Terrestrial systems often rely on dynamic memory allocation because they can be rebooted if the heap fragments or leaks. On a spacecraft, a ``malloc`` failure resulting from memory fragmentation is a critical, mission-ending event. Furthermore, dynamic allocation is non-deterministic; the time required to find a free block depends on the state of the heap, which can cause timing jitter in real-time Digital Signal Processing (DSP). LunaLink strictly uses static allocation and stack-based storage, ensuring that the total memory footprint is known at link-time and that no allocation-related failures can occur during flight.
*   **Benefit**: Eliminates the risk of memory fragmentation and non-deterministic allocation failures.

2. Fixed-Bound Loops & No Recursion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
All loops must have a fixed, verifiable upper bound. Recursion is forbidden.

*   **Explanation**: Space-rated hardware often lacks the sophisticated memory management units (MMUs) found in consumer electronics. If a program enters an infinite recursion or an unbounded loop, it will eventually overflow the stack, silently corrupting adjacent data segments or causing an immediate system crash. By enforcing strictly bounded loops and forbidding recursion, we enable static analysis tools to calculate the **Worst-Case Stack Depth (WCSD)**. This mathematical proof ensures that under no combination of input data or hardware interrupts will the stack ever collide with the data segment, guaranteeing system stability in the extreme radiation environment of the Lunar surface.
*   **Benefit**: Allows formal calculation of the **Worst-Case Stack Depth**.

3. Cyclomatic Complexity & Single Exit Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Complexity**: We enforce a limit of **10** per function.
*   **Single Exit**: Functions utilize a single ``return`` statement.

*   **Explanation**: High cyclomatic complexity (the number of independent paths through code) leads to "hidden" execution branches that are exponentially difficult to test with 100% path coverage. In flight software, we enforce a limit of 10 to ensure the code remains human-auditable and that all logical outcomes can be verified through white-box testing. Complementing this, the **Single Exit Point** mandate ensures that functions return from only one location. This is critical for mission assurance because it guarantees that mandatory cleanup logic—such as stack scrubbing (physically erasing sensitive signal data) and reciprocal sanity checks—is always executed and cannot be bypassed by an accidental early ``return`` or ``break`` in the middle of a function.
*   **Benefit**: Ensures that mandatory **Stack Scrubbing** and **Reciprocal Checks** are never bypassed.

4. Bidirectional Traceability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every mission-critical function in ``cpp/signal/`` is tagged with its 
corresponding LSIS requirement ID (e.g., ``[LSIS-AFS-101]``).

*   **Explanation**: Every line of code in a flight-certified system must be justifiable against an external standard. Bidirectional traceability links each C++ function directly to a specific requirement ID in the LunaNet (LSIS) specification. This serves a dual purpose: it ensures that every requirement in the spec has a verified implementation (preventing missing features), and it ensures that every line of code in the implementation corresponds to a valid requirement (preventing "feature creep" or hidden functionality). During a professional audit by agencies like ESA or NASA, this traceability allows reviewers to map the physical signal processing logic back to the mathematical definitions in the standard with 100% confidence.
*   **Benefit**: Ensures the implementation can be audited for 100% functional compliance.

Mission-Critical Coding Patterns
--------------------------------

1. SEU-Resistant Enumerations (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard sequential enums (0, 1, 2...) are vulnerable to single-bit flips. 
LunaLink utilizes **Non-Contiguous Bit Patterns** with high Hamming distance.

*   **Explanation**: Standard enumerations use sequential bits (e.g., 00, 01, 10). In a Lunar radiation environment, a single high-energy particle strike can flip a bit in a status register, silently turning a "SUCCESS" state into a valid but incorrect "ERROR" state, or vice versa. LunaLink utilizes non-contiguous bit patterns with a high Hamming distance (e.g., ``0x5A`` and ``0xA5``). By ensuring that multiple bits must flip simultaneously to reach another valid state, we make it statistically impossible for a single event upset (SEU) to cause an undetected state transition. Any single-bit flip results in an illegal bit pattern that is immediately identified as a fault.
*   **Implementation**: Patterns like ``0x5A`` (01011010) and ``0xA5`` (10100101).
*   **Benefit**: A register bit-flip transforms a valid status into an identifiable "Invalid" state.

2. Variable Mirroring / Soft-TMR (JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical state variables in performance loops are "mirrored" to detect 
ALU or register file corruption *during* calculation.

*   **Explanation**: While system RAM is often protected by parity or Error Correction Code (ECC) hardware, the internal CPU registers and the Arithmetic Logic Unit (ALU) are typically not. A radiation-induced bit flip can occur *during* a calculation or while a value is stored in a local register, leading to incorrect results that bypass standard memory protections. LunaLink implements "Soft-TMR" by mirroring critical state variables (e.g., the minimum Hamming distance in the BCH decoder) with their bitwise inverse. At every step of the calculation, the system verifies that ``variable == ~mirrored_variable``. This allows the software to detect transient hardware faults occurring inside the processor's own silicon, ensuring the integrity of high-stakes Digital Signal Processing (DSP) blocks.
*   **Implementation**: Variables like ``min_dist`` are paired with a bitwise inverse ``min_dist_mir``.
*   **Benefit**: Detects transient hardware faults in the middle of DSP blocks.

3. Parameter Majority Voting (TMR) (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical parameters (e.g., ``PrnId``, ``Fid``) are stored in 
triplets using the ``TmrValue<T>`` template.

*   **Explanation**: Triple Modular Redundancy (TMR) is the foundation of fault tolerance in deep space missions. By storing three identical copies of a mission-critical parameter (such as a PRN ID or a Subframe ID) in distinct memory locations, we dramatically reduce the probability of system failure due to radiation. A single bit flip can only corrupt one copy; when the software requires that value, it performs a majority vote (2-of-3) to determine the correct result. This ensures that even if a memory cell is permanently damaged by a heavy ion strike, the signal chain continues to operate using the remaining two copies, providing 100% availability without manual ground intervention.
*   **Benefit**: If an SEU flips a bit in one copy, the software corrects the value on-the-fly.

4. Active TMR Repair (NASA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Passive majority voting is insufficient for long-duration missions. 

*   **Explanation**: Passive majority voting masks faults during a read operation, but if the corrupted data remains in memory, faults will accumulate over time. If Copy A is corrupted on Monday and Copy B is corrupted on Tuesday, the system loses its majority and will process incorrect data. Active TMR Repair (also known as fault scrubbing) solves this by immediately overwriting the corrupted copy with the majority-voted correct value as soon as a mismatch is detected. In LunaLink, the ``TmrValue<T>::repair()`` logic ensures that single-bit faults are "healed" in RAM, resetting the clock on radiation damage and preventing Single Event Upsets (SEUs) from escalating into catastrophic Multi-Bit Upsets (MBUs).
*   **Implementation**: Done within the ``TmrValue::repair()`` logic.
*   **Benefit**: Prevents the accumulation of Single-Event Upsets (SEUs) over time.

5. Control-Flow Integrity (CFI) (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Radiation can cause the Program Counter to jump, leading to skipped 
logic or early loop exits.

*   **Explanation**: In a high-radiation environment, a single "Heavy Ion" strike can flip a bit in the processor's Instruction Pointer (IP) register. When this happens, the CPU does not necessarily crash; instead, it "teleports" to a different location in the code, silently skipping critical safety checks, parity calculations, or sanitization logic. Control-Flow Integrity (CFI) ensures the CPU followed the intended path by maintaining an independent record of the execution flow. LunaLink uses high-entropy "Magic Patterns" (e.g., ``0xA5A5A5A5``) and sentinel counters to track progress. If the CPU jumps out of a loop prematurely or enters a function incorrectly, the final terminal verification will fail, allowing the system to transition to a safe state instead of broadcasting "poisoned" navigation signals.
*   **Implementation**: 
    *   **Magic Patterns**: Safety-critical functions utilize high-entropy "Magic Start" and "Magic End" constants (e.g., ``0xA5A5A5A5``).
    *   **Sentinel Counters**: Loops maintain an independent ``cfi_count`` that starts at the Magic Pattern.
    *   **Terminal Verification**: At the function exit, the system verifies that ``cfi_count == (Magic_Start + Expected_Iterations)``.
*   **Benefit**: Detects radiation-induced "shortcuts" and logic skips before data is release to the transmitter.

6. Value Invariants & Saturating Logic (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Value-constrained wrappers (``CheckedRange<T, Min, Max>``) ensure 
corrupted indices remain within safe buffer bounds.

*   **Explanation**: Standard C++ integer arithmetic follows "wrapping" behavior (e.g., ``MAX + 1 = 0``). In a flight environment, if a radiation strike flips a bit in a loop index or a buffer offset, this wrapping can lead to catastrophic out-of-bounds memory accesses or infinite loops. LunaLink utilizes value-constrained wrappers (``CheckedRange<T, Min, Max>``) and **Saturating Arithmetic**. If a corrupted value exceeds its legal range, it is "clamped" to the nearest safe boundary (Min or Max) rather than wrapping. This ensures that the signal processing logic remains within predictable boundaries, maintaining system stability even when individual data parameters are poisoned by SEUs.
*   **Benefit**: Logic uses **Saturating Arithmetic** to prevent catastrophic overflows.

7. Anti-Optimization (Secure Scrubbing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard memory clearing (e.g., ``std::fill``) is vulnerable to 
**Dead Store Elimination (DSE)** by optimizing compilers.

*   **Explanation**: In safety-critical flight software, sensitive signal data and intermediate parity bits must be physically erased from RAM before a function returns to prevent cross-task data leakage or "ghost" data interference. Standard C++ clearing techniques (e.g., ``std::fill`` or ``memset``) are frequently removed by optimizing compilers via **Dead Store Elimination (DSE)**. If the compiler determines that a buffer is "never read again" before it goes out of scope, it may silently delete the zeroing logic to improve performance. In a radiation-prone environment, this leaves uninitialized memory sitting in RAM, which can lead to unpredictable behavior if a bit flip causes an accidental read. LunaLink's ``secure_scrub()`` utility utilizes ``volatile``-qualified pointers to force the compiler to respect the hardware-level mandate to physically overwrite memory, ensuring that data sanitization is bit-for-bit guaranteed in the final flight binary.
*   **Implementation**: The ``secure_scrub()`` utility utilizes a ``volatile`` pointer.
*   **Benefit**: Guarantees that sensitive signal data is physically erased from RAM.

8. Memory Section Pinning (JPL/Goddard)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical Look-Up Tables (LUTs) must not be left to the default 
placement of the linker.

*   **Explanation**: Spacecraft on-board computers (OBCs) typically utilize a heterogeneous mix of physical memory, including volatile SRAM, persistent MRAM, and radiation-hardened PROM. Some of these memory banks are significantly more shielded against Single Event Upsets (SEUs) than others. Large static Look-Up Tables (LUTs), such as the BCH codebook and the Weil spreading sequences, are "high-value targets" for radiation because they represent the reference data for all signal generation. If these tables are corrupted, the entire signal chain is compromised. By using the ``LUNALINK_LUT_SECTION`` macro, we explicitly tag these tables for the linker. This allows the mission-specific linker script to "pin" these critical tables to the most radiation-hardened physical memory banks available on the spacecraft, ensuring that our signal "ground truth" remains untainted throughout the mission lifespan.
*   **Implementation**: Large tables are tagged with the ``LUNALINK_LUT_SECTION`` macro.
*   **Benefit**: Allows mapping tables to the most radiation-hardened memory banks.

9. Temporal WIP Signaling (Watchdog Feed)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Long-running DSP loops must provide "proof of progress" to the system watchdog.

*   **Explanation**: Hardware watchdogs are the last line of defense for autonomous spacecraft. However, a traditional watchdog only detects a total system hang (where the CPU stops responding). In a radiation-heavy environment, it is possible for a bit flip in a jump instruction to trap the CPU in a logical infinite loop that *looks* active to the OS but is not doing useful work. LunaLink implements "proof of progress" by requiring safety-critical loops to call ``wip_tick()``. This increments a volatile **Work-In-Progress** counter that is independently monitored. If the system stops reporting these ticks within a specific temporal window, or if it reports them at an impossible frequency (indicating radiation-induced logic skips), the system triggers a reset. This ensures the spacecraft never remains stuck in a "zombie" state during critical signal transmission.
*   **Implementation**: Safety-critical loops call ``wip_tick()``.
*   **Benefit**: Ensures the hardware watchdog can detect if the CPU is stuck.

10. Self-Validating Data Structures (JIT Integrity)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical Look-Up Tables (LUTs) and sparse matrices include embedded CRC32 
checksums.

*   **Explanation**: Large data structures, such as the 5G NR LDPC sub-matrices used in LunaLink, are the most statistically likely targets for radiation-induced corruption because they occupy the largest contiguous areas of RAM. A single bit flip in these matrices can silently degrade the Bit Error Rate (BER) of the transmitted signal, leading to subtle navigation errors that are extremely difficult to diagnose from the ground. LunaLink embeds CRC32 checksums directly into its ``LdpcCsrMatrix`` structures. Before any Digital Signal Processing (DSP) begins, the software performs a **Just-In-Time (JIT) Integrity Check**. If the calculated checksum does not match the embedded "ground truth," the system identifies that its reference data has been "poisoned" by cosmic rays. It then halts processing and triggers a fault recovery event, ensuring that corrupted reference data never pollutes the broadcast signal.
*   **Implementation**: The ``LdpcCsrMatrix`` structure includes a ``crc32`` field checked by ``verify_integrity()``.
*   **Benefit**: Detects radiation-induced corruption of large tables in RAM before use.

11. Runtime Aliasing Protection (JAXA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mandatory runtime verification that output buffers do not overlap with input 
buffers.

*   **Explanation**: A radiation-induced bit flip in a pointer (Single Event Upset) can cause an output buffer to alias (overlap) with an input buffer. In Digital Signal Processing, this causes the function to overwrite its own source data while processing it, leading to non-deterministic signal corruption that bypasses standard logic checks. While terrestrial code relies on ``assert()`` (which is removed in production builds), Class A FSW mandates **Always-On Logic Gates**.
*   **Implementation**: DSP blocks perform a pointer-range check at the start of the function. If an overlap is detected, the function halts and returns ``kFaultDetected``.
*   **Benefit**: Prevents catastrophic feedback loops and pointer-based memory corruption that standard CFI cannot catch.

Time-Deterministic Execution (NASA/JAXA)
----------------------------------------

1. Constant-Time DSP
~~~~~~~~~~~~~~~~~~~~
LunaLink prioritizes **Branchless Arithmetic** for signal generation.

*   **Explanation**: Real-time signal processing depends on a stable, predictable execution budget. Terrestrial algorithms often optimize for "average case" performance using data-dependent branching (e.g., skipping calculations for zero-value bits). In a flight system, if the execution time varies based on the message content, it can cause "timing jitter" that disrupts high-precision Phase-Locked Loops (PLLs) in the receiver or causes the CPU to miss its real-time deadline. LunaLink utilizes branchless arithmetic for modulation and multiplexing to ensure that the **Worst-Case Execution Time (WCET)** is identical to the average-case execution time. This allows for rigid, deterministic scheduling on Real-Time Operating Systems (RTOS) like RTEMS or VxWorks.
*   **Benefit**: Guarantees a stable execution time regardless of input data.

2. Temporal Windowed Watchdogs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical tasks report their completion to a windowed watchdog.

*   **Explanation**: A standard hardware watchdog only detects if the software has "hung." A windowed watchdog is significantly more robust: it requires the software to report progress within a narrow temporal window—not too late, but also **not too early**. If a signal-generation task finishes too early, it indicates a catastrophic logic skip (e.g., a radiation-induced jump over a loop). If it is too late, it indicates an algorithmic bottleneck or hardware delay. LunaLink's signal chain is designed to support windowed watchdogs by ensuring every frame-generation cycle fits within a fixed, deterministic temporal envelope.
*   **Benefit**: Detects timing faults caused by oscillator jitter or radiation-induced logic skips.

Fault Management & Telemetry
----------------------------

1. Continuous Built-In Test (CBIT) (ESA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink supports periodic background scrubbing of critical Look-Up Tables.

*   **Explanation**: In a high-stakes Lunar mission, detecting errors only during power-on (POST) is insufficient because cosmic rays strike constantly. Continuous Built-In Test (CBIT) refers to background tasks that periodically validate system integrity while operational. LunaLink implements CBIT through functions like ``bch_codebook_checksum()``. This allows a background task to verify that large reference tables stored in RAM have not been "poisoned" by Single-Event Upsets (SEUs) before they are used to generate the next navigation frame.
*   **Benefit**: Detects Single-Bit Upsets in RAM-cached tables before they can pollute signal generation.

2. Frame Commitment & Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
No signal frame is "released" to hardware until it has been structurally validated.

*   **Explanation**: Transmitting a corrupted navigation frame is worse than transmitting no frame at all, as it can lead to incorrect position fixes for rovers. LunaLink enforces a "Frame Commitment" pattern: no symbol stream is released to the hardware DMA or SDR layer until the entire 12-second frame has been fully assembled in local RAM and has passed a final structural validation check. This ensures that if a bit flip occurred during the complex interleaving or LDPC encoding process, the error is caught at the "waterline" before the signal ever leaves the spacecraft.
*   **Requirement**: The final bit-stream must pass a **Post-Generation Checksum** before transmission.

3. Forbidden State Detection (NASA/JPL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Any hit to a ``default`` branch in a state transition is treated as a Critical Fault.

*   **Explanation**: Radiation can corrupt the state machines that control signal generation. If a state variable is flipped into a value that is not defined in our SEU-resistant enumerations, the software must not attempt to "guess" the next logical step. LunaLink treats any hit to a ``default`` case in a state transition as a **Critical Fault Event**. Instead of continuing with unpredictable logic, the system logs a high-priority telemetry packet and transitions to a "Safe Mode," ensuring the constellation's integrity is preserved.
*   **Benefit**: Prevents the software from entering "impossible" logic states caused by register corruption.

4. Fixed-Width Telemetry (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Error logging utilizes fixed-width binary packets to avoid stack corruption.

*   **Explanation**: Logging errors with variable-length strings (e.g., using ``printf``) is a frequent source of stack corruption and buffer overruns in embedded systems. In a flight environment, we must ensure that the act of reporting an error does not create a second, more catastrophic error. LunaLink utilizes fixed-width binary telemetry packs. This ensures that the memory footprint for error reporting is constant and predictable, allowing for robust fault logging even in the most resource-constrained environments.
*   **Benefit**: Ensures that fault reporting is deterministic and cannot cause stack overflows.

Supply Chain Security & Reproducibility (ECSS-Q-ST-80C)
-------------------------------------------------------

1. Immutable Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~
All external dependencies are pinned by **Commit Hash (SHA)**.

*   **Explanation**: Aerospace projects often have decade-long lifespans. Relying on mutable tags (e.g., ``v3.0``) in a package manager is a severe mission risk, as a vendor could maliciously or accidentally overwrite that tag with a breaking change. This could lead to a "silent" behavior shift after a re-build, even if no local code changed. LunaLink pins all external dependencies (such as the Catch2 testing framework) to an exact Git Commit Hash. This ensures that the source code being compiled is bit-for-bit identical to the code that was formally verified, protecting the mission from supply-chain attacks and unexpected library updates.
*   **Benefit**: Guarantees source-code consistency across the entire mission lifespan.

2. Bit-for-Bit Reproducible Builds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system enforces deterministic compilation.

*   **Explanation**: Reproducibility is the ultimate proof of build system integrity. In flight software, it is vital that the binary generated on a developer's workstation is identical to the one generated on the official flight CI server. LunaLink's build system enforces deterministic compilation by stripping absolute file paths from the binary (using ``-ffile-prefix-map``) and disabling non-deterministic compiler features. This allows for independent verification: any agency can re-build the source code and compare the MD5 hash of the resulting binary to the official flight image, ensuring that no build-environment artifacts or hidden logic were introduced during the release process.
*   **Verification**: The MD5 hash of the binary must be identical regardless of the build environment.

Verification & Tooling
----------------------

1. Digital Twin Compatibility (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The core library is strictly decoupled from hardware registers via a HAL.

*   **Explanation**: Access to physical flight hardware is often a bottleneck in satellite development. Digital Twin Compatibility ensures that the signal processing core can be verified on standard workstations (Linux/macOS) while maintaining bit-for-bit parity with the orbiting hardware (e.g., LEON/SPARC). LunaLink achieves this by strictly decoupling DSP algorithms from hardware-specific registers using a Hardware Abstraction Layer (HAL). This allows the **exact same source code** to be verified through millions of automated test vectors on a "Digital Twin" in the cloud, providing a level of verification depth impossible to achieve on physical hardware alone.
*   **Benefit**: Enables high-volume verification on terrestrial infrastructure.

2. Information Security (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
**Stack Scrubbing**: All internal temporary signal buffers are explicitly zeroed.

*   **Explanation**: In a multi-tasking satellite environment, RAM is a shared resource. When a DSP function finishes, its temporary buffers may still contain sensitive navigation data or intermediate parity bits. If these buffers are not explicitly zeroed, a subsequent task might accidentally read this "stale" data. LunaLink utilizes the flight-hardened ``secure_scrub()`` utility to physically overwrite temporary buffers before a function returns. This prevents cross-task data leakage and ensures that the spacecraft's internal state remains "clean," satisfying NASA's FSW Information Security mandates.
*   **Implementation**: Utilizes the ``secure_scrub()`` utility before function exit.

3. Static & Dynamic Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Clang-Tidy**: Enforces MISRA-aligned High-Integrity C++ (HICPP) rules.
*   **Cppcheck**: Secondary safety-critical static analyzer.
*   **ASan/UBSan**: Catch memory safety and undefined behavior at runtime.

*   **Explanation**: Testing alone cannot prove the absence of software bugs. Static analysis tools (Clang-Tidy, Cppcheck) scan the source code for logical errors, memory safety issues, and style violations that may not be triggered by specific test vectors. Dynamic analysis tools (ASan, UBSan) monitor the code at runtime to catch subtle issues like buffer overflows or signed-integer wrapping. By combining these tools with a mandate for ≥ 90% C++ line coverage, LunaLink provides empirical proof that the implementation is robust, predictable, and free of the common C++ pitfalls that plague high-reliability systems.
*   **Coverage**: Mandates ≥ 90% C++ line coverage and 100% Python coverage.

4. Compiler-Level Defenses
~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system mandates strict hardening flags across all platforms.

*   **Explanation**: Modern compilers provide built-in defenses against security exploits and hardware faults. LunaLink mandates strict hardening flags, such as ``-fstack-protector-strong``, which inserts "canaries" into the stack to detect and halt execution if a radiation-induced bit flip corrupts a function return address. Other flags like ``-ffp-contract=off`` ensure that floating-point calculations are bit-for-bit identical across different processors, which is vital for maintaining identical signal tracking performance between ground station simulators and the orbiting spacecraft.
*   **Benefit**: Leverages the compiler to insert automated runtime safety checks.
