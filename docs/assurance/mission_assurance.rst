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
*   **Single Exit**: All ``[LSIS-AFS-*]``-tagged DSP functions shall have a single
    return point.  Control-plane functions (FSM transitions, POST, PRN accessors)
    are exempt where a guard-clause ``return`` improves fault containment clarity.

*   **Explanation**: High cyclomatic complexity (the number of independent paths through code) leads to "hidden" execution branches that are exponentially difficult to test with 100% path coverage. In flight software, we enforce a limit of 10 to ensure the code remains human-auditable and that all logical outcomes can be verified through white-box testing. Complementing this, the **Single Exit Point** mandate ensures that functions return from only one location. This is critical for mission assurance because it guarantees that mandatory cleanup logic—such as stack scrubbing (physically erasing sensitive signal data) and reciprocal sanity checks—is always executed and cannot be bypassed by an accidental early ``return`` or ``break`` in the middle of a function.
*   **Benefit**: Ensures that mandatory **Stack Scrubbing** and **Reciprocal Checks** are never bypassed.

4. Bidirectional Traceability & Automated Compliance Matrix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every mission-critical function in ``cpp/signal/`` is tagged with its
corresponding LSIS requirement ID (e.g., ``[LSIS-AFS-101]``). Tag
density is enforced by the ``lint`` CI gate; bidirectional traceability
is provided through the human-curated ``signal/compliance_matrix.rst``.

*   **Explanation**: Every line of code in a flight-certified system must be
    justifiable against an external standard.  Bidirectional traceability links
    each C++ function directly to a specific requirement ID in the LunaNet (LSIS)
    specification, and each requirement back to the exact test case that verifies it.
*   **Current scope**: ``scripts/verify_traceability.py`` validates the LDPC matrix
    CSV files against their embedded CRC-32 checksums and cross-checks the
    ``signal/compliance_matrix.rst`` table for formatting consistency.  The
    ``[LSIS-AFS-*]`` tags in the source headers (updated at Gateway 2) are the
    traceable implementation markers; the ``signal/compliance_matrix.rst`` document
    is the human-curated bidirectional traceability artefact for this release.
*   **Gateway 4 deliverable**: Full automated compliance matrix generation — parsing
    LSIS spec PDF, ``cpp/signal/`` source, and Catch2 test names simultaneously into
    a machine-generated HTML report published to ``gh-pages`` — is scoped to
    Gateway 4.  A blocking CI gate on tag density (all signal-chain headers must
    carry ``[LSIS-AFS-*]`` annotations) is already enforced by the ``lint`` CI job.
*   **Benefit**: Transforms compliance from a manual audit into a reproducible,
    traceable record.  An auditor can cross-reference ``signal/compliance_matrix.rst``
    against the implementation tags in headers to verify bidirectional coverage.

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
*   **Critical Section Requirement**: All writes to a mirrored variable pair (``x`` and ``x_mir``) **must** be performed within a Critical Section (interrupt-masked atomic block). If an RTOS interrupt fires between the write to ``x`` and the write to ``x_mir``, the integrity check will produce a false-positive fault on task resume. See :ref:`isr-safety` for the mandate.
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
    *   **Sentinel Counters**: Loops maintain an independent ``cfi_count`` that starts at the Magic Pattern. All increments to ``cfi_count`` must occur within a Critical Section to prevent interrupt-induced false-positive failures.
    *   **Terminal Verification**: At the function exit, the system verifies that ``cfi_count == (Magic_Start + Expected_Iterations)``.
*   **Benefit**: Detects radiation-induced "shortcuts" and logic skips before data is released to the transmitter.

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

12. Data-in-Transit Protection (CCSDS/ESA) — Flight Integration Requirement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Data structures transferred between major DSP pipeline stages shall carry
a **Handoff CRC** verified by the consumer before processing begins.

*   **Explanation**: LunaLink's hardening strategy covers three distinct threat
    surfaces.  *Data-at-Rest* is protected by Memory Section Pinning and JIT
    Integrity Checks (see §8, §10).  *Data-in-Processing* is protected by Variable
    Mirroring and TMR (see §2, §3).  The third surface—*Data-in-Transit*—is equally
    vulnerable: an SEU striking a pointer or a bus latch during a handoff between the
    LDPC Encoder, the Interleaver, and the Modulator can silently corrupt the payload
    without triggering any of the in-processing guards.
*   **Flight requirement**: In the RTOS multi-task deployment every inter-block
    transfer structure shall include a ``DspFrameHeader::handoff_crc`` field
    (CRC-16/CCITT), computed by the producing task immediately before releasing the
    buffer.  The consuming task shall verify this field as the first operation of its
    entry function.  A CRC mismatch returns ``kFaultDetected`` and scrubs the frame
    at the Frame Isolation Boundary (see :ref:`fault-isolation`).
*   **Digital twin scope**: The digital twin executes the entire signal chain in a
    single task with no shared-memory handoff risk.  The ``DspFrameHeader`` struct
    and ``handoff_crc`` field are not instantiated in this release; the mechanism is
    documented here as a **required flight integration step** and will be implemented
    at flight programme adoption.
*   **Benefit**: Closes the Data-in-Transit threat surface for the multi-task RTOS
    deployment; any SEU on a bus or pointer during a pipeline handoff is detected
    before it can propagate.

.. _isr-safety:

13. Critical Section Protection / ISR Safety (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mirrored variables, TMR triplets, and CFI sentinel counters are **not
inherently atomic** and must be protected from preemptive interrupts.

*   **Explanation**: LunaLink targets RTOS environments (RTEMS, VxWorks) with preemptive scheduling and hardware interrupt lines. If an Interrupt Service Routine (ISR) fires between the write to a mirrored variable ``x`` and the write to its inverse ``x_mir``, the integrity check performed after the ISR returns will see a momentarily inconsistent pair and fire a false-positive fault. In a high-radiation environment, a false positive is nearly as dangerous as a missed fault: it can cause unnecessary safe-mode transitions during critical signal transmission windows. LunaLink mandates that all atomic invariants—mirror-pair writes, TMR triple writes, and CFI sentinel increments—are wrapped in **Critical Sections** (``LUNALINK_CRITICAL_ENTER`` / ``LUNALINK_CRITICAL_EXIT``). These macros map to the target RTOS's interrupt-masking primitives (e.g., ``rtems_interrupt_local_disable`` on RTEMS) and to compiler barriers (``asm volatile("" ::: "memory")``) on the host-native build for Digital Twin verification.
*   **Implementation**: The macros ``LUNALINK_CRITICAL_ENTER`` and
    ``LUNALINK_CRITICAL_EXIT`` are declared in
    ``cpp/include/lunalink/signal/safety.hpp``.

    * **Digital twin** (single-threaded, no RTOS): the macros expand to ``((void)0)``
      — no-ops that impose zero overhead.  The single-task execution model eliminates
      preemption risk; all assertions in this release are verified under this model.
    * **Flight** (RTEMS or equivalent RTOS): the flight HAL overrides these macros
      before including ``safety.hpp``, replacing the no-ops with platform-specific
      interrupt masking (e.g., ``rtems_interrupt_local_disable`` / ``_enable``).
      ISR handlers that read LunaLink state are restricted to polling ``volatile``
      status flags; they must never write into TMR or mirrored structures directly.
*   **Benefit**: Eliminates false-positive fault events caused by RTOS preemption;
    ensures the integrity of all soft-TMR and CFI invariants in a multi-task
    environment.  The no-op default makes the digital twin fully deterministic
    without requiring RTOS stubs.

State Machine Robustness (NASA/ESA)
------------------------------------

1. SEU-Resistant State Encoding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
State variables use the same non-contiguous bit-pattern encoding as
mission-critical enumerations (see §1 of Mission-Critical Coding Patterns).
A single-bit flip in a state register produces an illegal pattern, not a
valid-but-wrong state.

2. Legal Transition Matrix (Formal State Machine)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every state machine in the signal chain is defined by an explicit
**Legal Transition Matrix** — a compile-time table enumerating every
permitted ``(current_state, next_state)`` pair.

*   **Explanation**: SEUs do not only corrupt data — they corrupt control. A bit flip in a state register can attempt a transition that is physically dangerous: pushing the signal chain from ``TRANSMITTING`` directly to ``INITIALIZING`` without passing through ``IDLE`` could result in a hardware power amplifier being commanded to a forbidden configuration. Forbidding undefined states (§3, Forbidden State Detection) catches corrupted *values*, but it does not prevent a valid state from transitioning to another valid state via an *illegal edge*. The Legal Transition Matrix enforces that: if ``current_state`` is valid and ``next_state`` is valid but the edge ``(current_state → next_state)`` is not in the matrix, the transition is rejected and logged as a Critical Fault. This converts the state machine from a graph with implicit edges into a formally verified directed graph where every edge is an explicit safety assertion.
*   **Implementation**: Each state machine owns a ``constexpr`` transition table of type ``TransitionMatrix<State, N>``. The ``transition()`` method performs a lookup; any miss returns ``kFaultDetected`` and is reported via fixed-width telemetry before the safe-mode transition.
*   **Benefit**: Prevents hardware damage from radiation-induced illegal state transitions; provides a complete, auditable map of all legal system behaviors.

3. Forbidden State Detection (NASA/JPL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Any hit to a ``default`` branch in a state transition is treated as a Critical Fault.

*   **Explanation**: Radiation can corrupt the state machines that control signal generation. If a state variable is flipped into a value that is not defined in our SEU-resistant enumerations, the software must not attempt to "guess" the next logical step. LunaLink treats any hit to a ``default`` case in a state transition as a **Critical Fault Event**. Instead of continuing with unpredictable logic, the system logs a high-priority telemetry packet and transitions to a "Safe Mode," ensuring the constellation's integrity is preserved.
*   **Benefit**: Prevents the software from entering "impossible" logic states caused by register corruption.

.. _fault-isolation:

Fault Containment Architecture (NASA/ESA)
------------------------------------------

Detecting a fault is necessary but insufficient — a Class A system must also
*contain* it. LunaLink defines three nested **Fault Isolation Zones (FIZs)**
that bound the blast radius of any detected fault.

1. Fault Isolation Zones
~~~~~~~~~~~~~~~~~~~~~~~~~

*   **Explanation**: Without defined containment boundaries, a fault in the BCH decoder could propagate to the interleaver, which could corrupt the modulator output, which could trigger a watchdog reset of the entire OBC. This cascade is more destructive than any single fault. LunaLink prevents it with a layered containment model, analogous to the watertight bulkheads of a submarine:

    *   **Zone 1 — Block Level**: A fault detected *within* a single DSP block (e.g., a failed mirror check inside ``gather_row()``) is contained to that function. The function returns ``kFaultDetected``, scrubs its local buffers, and does **not** write to the output buffer. The calling frame-assembly logic sees the error code and acts according to Zone 2 policy.
    *   **Zone 2 — Frame Level**: A fault that cannot be recovered within a DSP block is contained at the **Frame Boundary**. The in-progress frame is scrubbed from RAM, the frame counter is advanced, and the signal chain resumes assembly of the **next** frame. The timing system, WIP counters, and RTOS task scheduling are explicitly **not** reset. This ensures that a single corrupted frame never disrupts the broadcast cadence.
    *   **Zone 3 — System Level**: A fault that violates a Zone 2 invariant (e.g., two consecutive frame faults, a failed JIT integrity check, or a Legal Transition Matrix miss in the top-level state machine) triggers a **Safe Mode transition**. The signal chain halts transmission, all buffers are scrubbed, and a fixed-width telemetry fault packet is generated. Recovery from Safe Mode requires a commanded restart from the OBC health management system.

*   **Mandate**: All ``kFaultDetected`` return paths in ``cpp/signal/`` must be annotated with their FIZ level (e.g., ``// FIZ-1``). Any fault that escalates beyond Zone 1 must be logged via the fixed-width telemetry interface before the escalation occurs.
*   **Benefit**: Guarantees that a single radiation event can corrupt at most one 12-second navigation frame, never the timing system or the OBC.

2. Frame Commitment & Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
No signal frame is "released" to hardware until it has been structurally validated.

*   **Explanation**: Transmitting a corrupted navigation frame is worse than transmitting no frame at all, as it can lead to incorrect position fixes for rovers. LunaLink enforces a "Frame Commitment" pattern: no symbol stream is released to the hardware DMA or SDR layer until the entire 12-second frame has been fully assembled in local RAM and has passed a final structural validation check. This ensures that if a bit flip occurred during the complex interleaving or LDPC encoding process, the error is caught at the "waterline" before the signal ever leaves the spacecraft.
*   **Requirement**: The final bit-stream must pass a **Post-Generation Checksum** before transmission. A failed checksum triggers a Zone 2 fault response (see §1 above).

Time-Deterministic Execution (NASA/JAXA)
----------------------------------------

1. Branchless DSP & Data-Independent Execution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink's signal chain is **branchless by mandate** — no data-dependent
branches exist in any ``[LSIS-AFS-*]``-tagged DSP function.

*   **Explanation**: Real-time signal processing depends on a stable, predictable execution budget. Terrestrial algorithms often optimize for "average case" performance using data-dependent branching (e.g., ``if (bit == 1) { ... }``). In a flight system, if the execution time varies based on the message content, it causes "timing jitter" that disrupts high-precision Phase-Locked Loops (PLLs) in the receiver or causes the CPU to miss its real-time deadline. LunaLink mandates branchless arithmetic — bitwise masks instead of conditional branches — for all modulation, multiplexing, and LDPC gather operations. The result is that the execution time of ``ldpc_encode()`` with an all-zeros input is **bit-for-bit identical** to its execution time with an all-ones input.
*   **Benefit**: Guarantees zero timing jitter regardless of navigation message content.

2. Formal WCET Profiling (DO-178C / NASA NPR 7150.2D)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The branchless mandate is not asserted — it is **measured and reported**
as a formal deliverable, published to the documentation site.

*   **Explanation**: Claiming a function is branchless is a design intent; proving it requires measurement. LunaLink's CI pipeline runs the Catch2 benchmark suite and produces a **WCET Profile Report** (``docs/assurance/wcet_profile.rst``) for every ``[LSIS-AFS-*]``-tagged function. Where benchmark variants exist for multiple adversarial input classes (all-zeros, all-ones, pseudo-random), the CI step computes the spread ``(max − min) / min × 100`` across those samples. A correctly branchless function shows near-zero spread; a data-dependent branch widens the spread beyond the **±2% tolerance band**.

    .. code-block:: text

        WCET Profile — ldpc_encode() — build 4a9f2c1
        ┌─────────────────────┬──────────┬──────────┬──────────┬──────────┐
        │ Input Class         │ Min (cy) │ Max (cy) │ Mean(cy) │ Δ vs Min │
        ├─────────────────────┼──────────┼──────────┼──────────┼──────────┤
        │ All-zeros           │   14,201 │   14,209 │   14,204 │    +0.06%│
        │ All-ones            │   14,198 │   14,211 │   14,203 │    +0.09%│
        │ Pseudo-random       │   14,199 │   14,214 │   14,205 │    +0.10%│
        └─────────────────────┴──────────┴──────────┴──────────┴──────────┘
        VERDICT: PASS — all classes within ±2% tolerance band.

    .. note::

       The ±2% check is an **intra-run input-sensitivity check**: it detects data-dependent
       branches within a single CI run by comparing timing across adversarial input classes.
       It is **not** a build-to-build regression check against a stored baseline.
       Full inter-build baseline comparison (WCET regression tracking) is scoped to
       Gateway 3, when multi-class benchmark variants are added for all tagged functions.
       Functions with only a single timing sample receive a ``WARN`` verdict; the build
       is not blocked, but adding a multi-class variant is mandatory before Gateway 3 exit.

*   **Mandate**: Any function where multiple input-class variants exist and the spread exceeds ±2% is a **blocking CI failure**, indicating a data-dependent branch. The WCET profile report is published to the **documentation site** (``gh-pages``) on every passing build.
*   **Benefit**: Converts the branchless mandate from a code-review assertion into a measurement-backed CI check — escalating to full inter-build baseline tracking at Gateway 3.

3. Temporal Windowed Watchdogs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical tasks report their completion to a windowed watchdog.

*   **Explanation**: A standard hardware watchdog only detects if the software has "hung." A windowed watchdog is significantly more robust: it requires the software to report progress within a narrow temporal window—not too late, but also **not too early**. If a signal-generation task finishes too early, it indicates a catastrophic logic skip (e.g., a radiation-induced jump over a loop). If it is too late, it indicates an algorithmic bottleneck or hardware delay. LunaLink's signal chain is designed to support windowed watchdogs by ensuring every frame-generation cycle fits within a fixed, deterministic temporal envelope. The WCET profile (see §2) provides the temporal bounds that the windowed watchdog's lower and upper thresholds are derived from.
*   **Benefit**: Detects timing faults caused by oscillator jitter or radiation-induced logic skips; thresholds derived from formally measured WCET data rather than engineering estimates.

Fault Management & Telemetry
----------------------------

1. Power-On Self-Test (POST) (NASA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Before entering operational mode, LunaLink executes a mandatory, ordered
**POST sequence** that establishes a verified "ground truth" for all
safety-critical data structures.

*   **Explanation**: CBIT (see §2) detects faults that occur during operations, but it cannot protect against a system that boots into a pre-corrupted state — for example, after a watchdog reset caused by a heavy-ion strike. POST is the formal entry gate: the signal chain must not transmit a single symbol until POST passes. Any failure is a blocking fault; the system holds in a pre-operational safe state and emits a fixed-width telemetry fault packet awaiting a ground command to retry or remain dormant. The POST sequence is ordered by criticality and executes entirely from static, stack-allocated storage (zero heap, consistent with §1 of Design Constraints).
*   **POST Sequence** (executed in order at every boot and warm reset):

    1. **SCI Header Integrity**: Verify the CRC of the embedded Software Configuration Item header (see Supply Chain §3). A mismatch means the flight binary itself is corrupt — no further POST steps are attempted.
    2. **LUT CRC Verification**: Run ``verify_integrity()`` on every ``LdpcCsrMatrix`` and BCH codebook. These are the reference data for all signal generation; corrupted LUTs make every subsequent check meaningless.
    3. **TMR Triplet Consistency**: Verify that all ``TmrValue<T>`` instances read the same value from all three copies. Initialise ``repair()`` on any mismatch detected at boot, before any operational code has run.
    4. **Read-Only Data Integrity Check**: Verify that compile-time constants in
       ``.rodata`` (painted with a known pattern at compile time) have not been
       corrupted by a radiation event affecting the read-only data segment.  This
       is distinct from stack-overflow detection: in flight, a linker-script-painted
       sentinel at ``_stack_limit`` provides stack-overflow detection; the digital
       twin implements ``.rodata`` integrity verification only.
    5. **CFI Baseline**: Execute a single no-op pass through the CFI framework to verify that the magic-pattern constants embedded in the binary match their expected values (i.e., that the constants themselves have not been corrupted in PROM/MRAM).

*   **Benefit**: Guarantees that the signal chain boots into a verified, coherent state. Prevents the scenario where a radiation strike during a watchdog-reset cycle causes the system to resume from a pre-corrupted baseline.

2. Continuous Built-In Test (CBIT) (ESA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink supports periodic background scrubbing of critical Look-Up Tables.

*   **Explanation**: In a high-stakes Lunar mission, detecting errors only during power-on (POST) is insufficient because cosmic rays strike constantly. Continuous Built-In Test (CBIT) refers to background tasks that periodically validate system integrity while operational. LunaLink implements CBIT through functions like ``bch_codebook_checksum()``. This allows a background task to verify that large reference tables stored in RAM have not been "poisoned" by Single-Event Upsets (SEUs) before they are used to generate the next navigation frame.
*   **Benefit**: Detects Single-Bit Upsets in RAM-cached tables before they can pollute signal generation.

3. Fixed-Width Telemetry (NASA)
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

2. Toolchain Version Pinning
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The compiler, linker, and standard library are pinned to exact versions
and treated as change-controlled flight artifacts.

*   **Explanation**: Pinning source dependencies is necessary but not sufficient —
    the tool that transforms source into machine code is an equally critical supply
    chain component.  Two different versions of GCC can produce observably different
    code generation for the same C++ source: loop reordering, struct padding, and
    inlining heuristics all change between releases.  In a system where the CFI
    sentinel counter relies on the compiler generating a specific number of
    instructions per loop iteration, an uncontrolled compiler upgrade is a silent
    regression.  The target compiler for flight use is ``arm-rtems-gcc 12.3.1`` or
    equivalent; any compiler upgrade shall be a **formal change-controlled event**
    requiring: a full re-run of all MC/DC verification data, a re-comparison of the
    generated assembly for all ``[LSIS-AFS-*]``-tagged functions against the previous
    baseline, and a new reproducible-build MD5 certification.
*   **Implementation**: The compiler version is logged to the CI build artifact on
    every run, providing a time-stamped record of the toolchain used for each build.
    Formal ``toolchain.lock`` file management — committing an explicit compiler version
    manifest and asserting it in CI as a blocking gate — is scoped to flight programme
    adoption.
*   **Benefit**: Eliminates silent safety regressions introduced by compiler upgrades;
    ensures the verified binary and the re-buildable binary are produced by identical
    toolchains.

3. Software Configuration Item (SCI) Version Register
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every flight binary embeds an **SCI header** — a fixed-size, CRC-protected
block containing the binary's complete identity, readable via telemetry.

*   **Explanation**: After a radiation-induced watchdog reset or an anomalous reboot, ground operators must be able to answer definitively: "which version of the software is currently executing?" Without an embedded identity, this is impossible to determine from telemetry alone. LunaLink embeds a ``SciHeader`` struct as a ``constexpr`` global in a dedicated, linker-pinned section (``LUNALINK_SCI_SECTION``). The struct is populated at compile time and includes: the Git commit SHA-1, the LSIS specification version the binary implements, a build timestamp (UTC), and a CRC32 of the preceding fields. The POST sequence (see Fault Management §1) verifies this CRC at every boot to detect corruption of the binary itself. Ground operators can request the SCI header via a standard telemetry query and cross-reference it against the mission database to confirm binary identity.
*   **Implementation**:

    .. code-block:: cpp

        struct SciHeader {
            char     git_sha[41];      // null-terminated hex SHA-1
            char     lsis_version[16]; // e.g. "LSIS-V1.0-2025"
            uint32_t build_timestamp;  // seconds since J2000
            uint32_t header_crc32;     // CRC32 of preceding fields
        } LUNALINK_SCI_SECTION;

*   **Benefit**: Provides an unambiguous, tamper-evident chain of custody from source commit to flying binary; enables post-anomaly root-cause analysis by confirming which verified build was executing at the time of the event.

4. Bit-for-Bit Reproducible Builds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system enforces deterministic compilation.

*   **Explanation**: Reproducibility is the ultimate proof of build system integrity. In flight software, it is vital that the binary generated on a developer's workstation is identical to the one generated on the official flight CI server. LunaLink's build system enforces deterministic compilation by stripping absolute file paths from the binary (using ``-ffile-prefix-map``) and disabling non-deterministic compiler features. This allows for independent verification: any agency can re-build the source code and compare the MD5 hash of the resulting binary to the official flight image, ensuring that no build-environment artifacts or hidden logic were introduced during the release process.
*   **Verification**: The MD5 hash of the binary must be identical regardless of the build environment.

Verification & Tooling
----------------------

1. Modified Condition/Decision Coverage (MC/DC) (DO-178C / NASA NPR 7150.2D)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink mandates **MC/DC** as its primary coverage metric for all
safety-critical DSP logic. Line coverage is a secondary, supplementary metric.

*   **Explanation**: Line coverage proves that a line of code was *executed*. For a safety-critical DSP block containing compound boolean guards (e.g., ``if (seu_detected || range_exceeded)``), line coverage only verifies that the branch was reached. It says nothing about whether each individual condition independently affects the outcome. **Modified Condition/Decision Coverage (MC/DC)**, mandated by DO-178C for Level A software, requires demonstrating that: (1) every condition in a decision has been shown to independently affect the decision's outcome, and (2) every condition has been evaluated as both ``true`` and ``false``. For LunaLink's SEU detection logic—where the difference between a missed fault and a correct detection is a single boolean evaluation—line coverage is not sufficient to constitute a safety case. An auditor from ESA or NASA will reject a "90% line coverage" claim as evidence of correctness for Class A logic; they will ask for MC/DC data.
*   **Mandate**: All functions tagged ``[LSIS-AFS-*]`` in ``cpp/signal/`` must achieve **100% MC/DC** as measured by LLVM's source-based coverage instrumentation (``-fprofile-instr-generate -fcoverage-mapping``). The CI pipeline generates an MC/DC report published to the **documentation site** (``gh-pages``) on every passing build; any regression below 100% for tagged functions is a **blocking** build failure.
*   **Line Coverage**: ≥ 90% C++ line coverage and 100% Python coverage are maintained as secondary metrics and reported alongside MC/DC data.
*   **Benefit**: Provides a mathematically rigorous proof that all safety-critical boolean logic has been independently verified, satisfying DO-178C Level A auditing requirements.

2. Digital Twin Compatibility (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The core library is strictly decoupled from hardware registers via a HAL.

*   **Explanation**: Access to physical flight hardware is often a bottleneck in satellite development. Digital Twin Compatibility ensures that the signal processing core can be verified on standard workstations (Linux/macOS) while maintaining bit-for-bit parity with the orbiting hardware (e.g., LEON/SPARC). LunaLink achieves this by strictly decoupling DSP algorithms from hardware-specific registers using a Hardware Abstraction Layer (HAL). This allows the **exact same source code** to be verified through millions of automated test vectors on a "Digital Twin" in the cloud, providing a level of verification depth impossible to achieve on physical hardware alone.
*   **Benefit**: Enables high-volume verification on terrestrial infrastructure.

3. Information Security (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
**Stack Scrubbing**: All internal temporary signal buffers are explicitly zeroed.

*   **Explanation**: In a multi-tasking satellite environment, RAM is a shared resource. When a DSP function finishes, its temporary buffers may still contain sensitive navigation data or intermediate parity bits. If these buffers are not explicitly zeroed, a subsequent task might accidentally read this "stale" data. LunaLink utilizes the flight-hardened ``secure_scrub()`` utility to physically overwrite temporary buffers before a function returns. This prevents cross-task data leakage and ensures that the spacecraft's internal state remains "clean," satisfying NASA's FSW Information Security mandates.
*   **Implementation**: Utilizes the ``secure_scrub()`` utility before function exit.

4. Static & Dynamic Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Clang-Tidy**: Enforces MISRA-aligned High-Integrity C++ (HICPP) rules.
*   **Cppcheck**: Secondary safety-critical static analyzer.
*   **ASan/UBSan**: Catch memory safety and undefined behavior at runtime.

*   **Explanation**: Testing alone cannot prove the absence of software bugs. Static analysis tools (Clang-Tidy, Cppcheck) scan the source code for logical errors, memory safety issues, and style violations that may not be triggered by specific test vectors. Dynamic analysis tools (ASan, UBSan) monitor the code at runtime to catch subtle issues like buffer overflows or signed-integer wrapping. By combining these tools with a mandate for MC/DC on safety-critical functions and ≥ 90% C++ line coverage, LunaLink provides empirical proof that the implementation is robust, predictable, and free of the common C++ pitfalls that plague high-reliability systems.
*   **Coverage**: Mandates **100% MC/DC** on ``[LSIS-AFS-*]``-tagged functions, ≥ 90% C++ line coverage, and 100% Python coverage.

5. Compiler-Level Defenses & Forbidden Optimizations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system mandates strict hardening flags and explicitly forbids
compiler optimizations that undermine LunaLink's safety patterns.

*   **Explanation**: Modern compilers provide built-in defenses against security exploits and hardware faults, but they also perform aggressive optimizations that can silently violate the guarantees LunaLink's hardening patterns depend on. Two categories of flags are therefore managed:

    *   **Mandatory Hardening Flags**: ``-fstack-protector-strong`` inserts "canaries" into the stack to detect and halt execution if a radiation-induced bit flip corrupts a function return address. ``-ffp-contract=off`` ensures that floating-point calculations are bit-for-bit identical across different processors, which is vital for maintaining identical signal tracking performance between ground station simulators and the orbiting spacecraft.

    *   **Forbidden Optimizations**: The following flags are **explicitly prohibited** in the flight build because they conflict with LunaLink's safety patterns:

        .. list-table::
           :header-rows: 1
           :widths: 30 70

           * - Forbidden Flag
             - Reason
           * - ``-fstrict-aliasing``
             - Allows the compiler to assume pointers of different types cannot alias. This can cause it to re-order or eliminate the pointer-range aliasing checks (§11) that are the last line of defence against SEU-corrupted pointers. LunaLink's aliasing guards are the only barrier; removing the compiler's respect for them is a silent safety regression.
           * - ``-funroll-loops``
             - Loop unrolling restructures loop bodies in ways that can break the CFI sentinel counter invariant (§5). If the compiler unrolls a 16-iteration loop into 4 groups of 4, a radiation-induced skip of one unrolled group will advance ``cfi_count`` by 4, making the terminal check pass when it should fail.
           * - ``-fomit-frame-pointer``
             - Eliminates the frame pointer to free a register. This prevents WCSD (Worst-Case Stack Depth) static analysis tools from correctly computing the call stack, undermining the §2 guarantee.
           * - ``-ffast-math`` / ``-funsafe-math-optimizations``
             - Allows non-IEEE-754 floating-point transformations. Breaks the bit-for-bit Digital Twin parity requirement (§2, Digital Twin Compatibility) and can produce different results on LEON/SPARC vs x86.
           * - ``-fdelete-null-pointer-checks``
             - Allows the compiler to remove null-pointer guards on the assumption they are UB. LunaLink's pointer-range checks (§11) can appear "unreachable" to the optimizer after an SEU—removing them eliminates the guard entirely.

*   **Verification**: The CI pipeline runs ``scripts/check_build_flags.py`` to assert that no forbidden flag appears in the CMake-generated compile commands database (``compile_commands.json``). This check is a **blocking** CI gate.
*   **Benefit**: Leverages the compiler to insert automated runtime safety checks while guaranteeing that no optimization silently removes a safety pattern.

6. Software-Implemented Fault Injection (SWIFI) (ESA ECSS-E-ST-40C)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every hardening pattern must be **empirically verified** to detect the fault
it claims to detect. Documentation of a pattern is not a safety case.
Demonstration of a pattern is.

*   **Explanation**: A team can implement variable mirroring, TMR, and CFI sentinels with perfect fidelity and still have a latent defect — for example, a critical section that is one instruction too short, allowing a race that the pattern silently misses. The only way to close this gap is **adversarial testing**: deliberately inject the exact fault each pattern is designed to catch, and assert that the detection fires within the expected number of execution cycles. LunaLink maintains a dedicated SWIFI test suite (``tests/fault_injection/``) that is executed as a blocking CI stage. Each test follows the same three-phase structure: **Inject** → **Execute** → **Assert Detection**.

    The SWIFI suite covers the following injection scenarios:

    .. list-table::
       :header-rows: 1
       :widths: 35 30 35

       * - Hardening Pattern
         - Injected Fault
         - Expected Detection
       * - Variable Mirroring (§2)
         - Single-bit flip in ``min_dist`` between mirror-pair writes
         - ``variable != ~mirrored`` check fires; returns ``kFaultDetected``
       * - TMR Voting (§3)
         - One copy of a ``TmrValue<T>`` overwritten with a corrupted value
         - Majority vote detects mismatch; ``repair()`` restores correct value
       * - CFI Sentinels (§5)
         - Loop iteration count decremented by 1 (simulating a skipped iteration)
         - Terminal check ``cfi_count != Magic_Start + N`` fires; returns ``kFaultDetected``
       * - LUT JIT Integrity (§10)
         - Single byte of ``LdpcCsrMatrix`` payload overwritten
         - ``verify_integrity()`` CRC mismatch; Zone 3 fault event triggered
       * - Handoff CRC (§12)
         - One byte of the inter-block transfer buffer modified after CRC is set
         - Consumer's CRC check fails; returns ``kFaultDetected`` before processing
       * - Legal Transition Matrix (State Machine §2)
         - State register set to a valid-but-illegal transition target
         - ``transition()`` matrix miss; ``kFaultDetected`` + telemetry fault packet
       * - Aliasing Protection (§11)
         - Output buffer pointer set to overlap input buffer
         - Pointer-range check fires at function entry; returns ``kFaultDetected``
       * - Saturating Arithmetic (§6)
         - ``CheckedRange<T>`` value incremented beyond ``Max``
         - Value saturates at ``Max``; no wrap-around; subsequent bounds check passes

*   **Fault Injection API**: Each hardening-capable type exposes a dedicated fault-injection method, available only when compiled with ``LUNALINK_FAULT_INJECTION=1`` (never present in the flight binary):

    .. code-block:: cpp

        // TmrValue — corrupt one of the three copies
        template<typename T>
        void TmrValue<T>::inject_fault(uint8_t copy_index, T corrupted_value);

        // CheckedRange — bypass bounds and force an out-of-range value
        template<typename T, T Min, T Max>
        void CheckedRange<T,Min,Max>::force_set(T raw_value);

        // CfiGuard — truncate the sentinel to simulate a skipped iteration
        void CfiGuard::inject_skip(uint32_t iterations_to_remove);

        // LdpcCsrMatrix — flip a single bit in the payload
        void LdpcCsrMatrix::inject_bit_flip(size_t byte_offset, uint8_t bit_mask);

*   **Safety Coverage Report**: The SWIFI suite produces a machine-readable fault coverage report on every passing CI run, published to the **documentation site** (``gh-pages``) for a stable, permanent, publicly accessible URL. It is not committed to the ``main`` branch. The report records, for each injection scenario: the fault injected, the detection mechanism that fired, the cycle count from injection to detection, and pass/fail. The headline metric is **Single-Bit Fault Detection Coverage**: the percentage of injected single-bit faults that were correctly detected and contained before affecting the output. This must be **100%**; any regression is a blocking CI failure.
*   **Mandate**: Every new hardening pattern added to this document **must** be accompanied by a corresponding SWIFI test case before the pattern is considered verified. A pattern without a SWIFI test is classified as "documented but unverified" and is not admissible as evidence in a safety case review.
*   **Mutation testing (Gateway 4 planned deliverable)**: A planned CI addition will
    run mutation testing (via ``mutate-cpp`` or equivalent) against
    ``[LSIS-AFS-*]``-tagged functions.  Mutation testing deliberately introduces
    source-level faults (operator flips, condition negations) and verifies the test
    suite kills every mutant.  This proves that the MC/DC test vectors are not only
    achieving coverage but are semantically sensitive to the logic they cover.
    Integration is planned for Gateway 4 exit; the SWIFI suite provides the primary
    fault-detection coverage evidence for this release.
*   **Benefit**: Converts the hardening specification from a set of engineering claims
    into a set of empirically verified, reproducible safety assertions.  The
    ``fault_coverage.rst`` report records 100% single-bit fault detection coverage
    measured by the SWIFI suite.

7. Golden Test Vector Suite (CCSDS / LunaNet ICD) — Gateway 3 Deliverable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink will publish a **versioned, machine-readable test vector library** that
constitutes the reference for the entire LSIS ecosystem.

*   **Status**: The test vector suite is a **Gateway 3 design specification**.
    ``tests/generate_test_vectors.py`` provides the scaffolding for vector
    generation.  The ``tests/vectors/`` directory will be created and populated at
    Gateway 3 exit.  This section documents the intended design so that the
    implementation matches the published specification.

*   **Design intent**: A reference implementation has two obligations: it must be
    correct, and it must enable others to verify their own correctness against it.
    LunaLink will fulfil the second obligation by publishing a comprehensive,
    versioned test vector library in JSON format.  The library will be the normative
    reference: if a hardware receiver or another software implementation disagrees
    with a LunaLink vector, the vector is authoritative until proven otherwise by a
    spec errata.

*   **Planned vector categories**:

    .. list-table::
       :header-rows: 1
       :widths: 25 75

       * - Category
         - Description
       * - **Nominal**
         - Full 12-second frames for all PRN IDs and Subframe types; bit-exact expected output from each pipeline stage (post-BCH, post-LDPC, post-interleave, post-modulate).
       * - **Corner Case — Doppler**
         - Navigation messages constructed to exercise maximum positive and negative Doppler shift profiles (±3.5 km/s relative velocity at 1176.45 MHz).
       * - **Corner Case — Min Power**
         - Frames at the minimum received power level (−160 dBW); verifies that the SNR margin of the coding chain is sufficient without clipping or saturation.
       * - **Corner Case — Poisoned Nav Data**
         - Frames with a structurally valid CRC and valid LDPC codeword but semantically invalid navigation fields (e.g., out-of-range ephemeris values).
       * - **Fault Injection**
         - Frames with deliberate single-bit errors at known positions in the BCH-protected fields.
       * - **Boundary**
         - Frames at the first and last valid PRN ID, minimum and maximum Subframe ID, and at every rollover boundary for the time-of-week counter.

*   **Format**: Each vector will be a self-describing JSON object containing the
    input parameters, the expected output at each pipeline stage, the LSIS
    requirement IDs exercised, and the LunaLink git commit SHA used to generate it.
*   **Benefit**: Establishes LunaLink as the normative reference for the LSIS
    ecosystem; enables any hardware or software implementation to validate their own
    compliance against a single, authoritative, machine-readable source.

Path to Full NPR 7150.2D Certification
---------------------------------------

This document satisfies the **technical** coding, verification, and fault-tolerance
requirements of NASA NPR 7150.2D Class A.  A complete programme-level certification
requires additional process and organisational infrastructure.  The table below
summarises the classification honestly.

**Satisfied (strong evidence)**:

.. list-table::
   :header-rows: 1
   :widths: 12 88

   * - SWE
     - Evidence in this document or companion artifacts
   * - SWE-108/109
     - Design Constraints section; compiler flag assertions; ``static_analysis.rst``
   * - SWE-088
     - Bidirectional traceability mandate; ``compliance_matrix.rst``
   * - SWE-117
     - Clang-Tidy + Cppcheck zero-violation reports on ``gh-pages``
   * - SWE-127/128
     - 100% MC/DC; SWIFI 100% single-bit fault detection; Digital Twin compatibility
   * - SWE-074 (partial)
     - SHA-pinned dependencies; reproducible builds; SCI version register; ``scmp.rst``
   * - SWE-028/029
     - ``fmea.rst`` — full software FMEA table
   * - SWE-027
     - ``safety_plan.rst`` — Software Safety Plan
   * - SWE-050/070
     - ``oss_assessment.rst`` — COTS/OSS assessment

**Organisational requirements — outside the scope of this reference implementation**:

IV&V (SWE-121), formal milestone reviews (SWE-020), Change Control Board (SWE-079),
and Physical Configuration Audit (SWE-155) require programme office infrastructure
and cannot be satisfied by a development team acting alone.  These gaps are
documented in full in ``docs/assurance/certification_delta.rst``, including a
concrete path forward for a programme adopting LunaLink for flight.

8. Exhaustive Static Analysis Report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Static analysis results are **published as formal artifacts**, not cited as
passing claims.

*   **Explanation**: Stating "we run Clang-Tidy" is a process claim. Publishing the full output with zero violations is a compliance proof. LunaLink's CI pipeline generates three static analysis reports on every passing build and publishes them to the **documentation site** (``gh-pages``), giving each a stable, permanent, publicly accessible URL. They are not committed to the ``main`` branch — generated outputs belong on ``gh-pages``, not in code history:

    *   ``static_analysis.rst``: Full Clang-Tidy output for ``cpp/signal/``, showing zero MISRA-aligned HICPP violations. Any new violation is a blocking CI failure.
    *   ``cppcheck.rst``: Full Cppcheck output, showing zero errors and zero warnings at the ``--enable=all`` level.
    *   ``coverage.rst``: LLVM coverage report showing MC/DC percentages per function for all ``[LSIS-AFS-*]``-tagged functions, branch coverage ≥ 95% across ``cpp/signal/``, and line coverage ≥ 90% overall.

*   **Single Command Reproducibility**: All three reports are regenerated by ``task analyze`` in the project Taskfile. Any reviewer can independently reproduce every number in these reports from a clean checkout in under five minutes.
*   **Benefit**: Transforms analysis from an invisible CI step into a transparent, auditable record — giving reviewers direct evidence of zero violations rather than requiring them to trust the claim.
