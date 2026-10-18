Mission Assurance
=================

LunaLink is built to **ECSS Category B** norms — self-assessed, not formally
certified — and reaches past them to the stricter Category A / DO-178C Level A
technical controls wherever that is practical, so a later criticality upgrade
costs no new test infrastructure.  The hardening patterns come from NASA
(JPL/Goddard), ESA (ESTEC) and JAXA (Tsukuba).  This document records what each
one buys on the lunar surface, where radiation is the standing threat to
deterministic, fault-tolerant operation.

Core Hardening Philosophy
-------------------------

Space software carries three risks that terrestrial software does not:

1. **Silent Data Corruption (SDC)**: a radiation-induced Single Event Upset
   (SEU) flips a bit in a register or in memory.  Nothing crashes; the state
   is simply wrong, or the behaviour undefined.
2. **Autonomous Survival**: a lunar payload detects and mitigates its own
   faults, with no ground station in the loop.
3. **Timing Jitter**: execution times that move with the data destabilise the
   phase-locked loop in the receiver.

The measures below answer each in turn.

Assurance Document Index
------------------------
- ``docs/assurance/c_plus_plus_guidelines.rst``: C++20 coding standard and patterns.
- ``docs/assurance/traceability_matrix.rst``: Canonical requirements-to-evidence matrix.
- ``docs/assurance/tool_confidence.rst``: Toolchain and evidence trust record.
- ``docs/assurance/build_policy.rst``: Build tiers and compiler policy.
- ``docs/assurance/configuration_baseline.rst``: Release baseline and reproducibility.

Design Constraints (ECSS-E-ST-40C / JPL Rule 1-10)
--------------------------------------------------

1. Zero Dynamic Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~
Strictly zero heap usage (``malloc``/``new``) after the initialization phase.

*   **Explanation**: a terrestrial system can lean on the heap, because someone
    can reboot it when the heap fragments or leaks.  A spacecraft runs unattended
    for years, and there a ``malloc`` failure ends the mission.  Allocation is
    also non-deterministic — the hunt for a free block depends on the state of
    the heap, which puts jitter into real-time Digital Signal Processing (DSP).
    LunaLink allocates statically or on the stack, so the total memory footprint
    is fixed at link time and no allocation can fail in flight.
*   **Benefit**: Eliminates the risk of memory fragmentation and non-deterministic allocation failures.

2. Fixed-Bound Loops & No Recursion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
All loops must have a fixed, verifiable upper bound. Recursion is forbidden.

*   **Explanation**: space-rated hardware rarely carries the memory management
    unit that consumer parts take for granted.  Runaway recursion or an unbounded
    loop overflows the stack, and what follows is either an immediate crash or
    the quieter corruption of an adjacent data segment.  Bounded loops and no
    recursion let static analysis compute the **Worst-Case Stack Depth (WCSD)**
    — a proof that no combination of input data and hardware interrupts can
    drive the stack into the data segment.
*   **Benefit**: Allows formal calculation of the **Worst-Case Stack Depth**.

3. Cyclomatic Complexity & Single Exit Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Complexity**: We enforce a limit of **10** per function.
*   **Single Exit**: All ``[LSIS-AFS-*]``-tagged DSP functions shall have a single
    return point.  Control-plane functions (FSM transitions, POST, PRN accessors)
    are exempt where a guard-clause ``return`` improves fault containment clarity.

*   **Explanation**: cyclomatic complexity counts the independent paths through
    a function, and the cost of covering them all climbs fast.  A limit of 10
    keeps the code auditable by a human and reachable by white-box tests.  The
    **Single Exit Point** rule works alongside it: one ``return`` per function
    means the mandatory cleanup — stack scrubbing, which physically erases
    sensitive signal data, and reciprocal sanity checks — always runs, and no
    stray early ``return`` or ``break`` can skip it.
*   **Benefit**: nothing bypasses the mandatory **stack scrubbing** and **reciprocal checks**.

4. Bidirectional Traceability & Automated Compliance Matrix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every mission-critical function in the core source directories carries its
LSIS requirement ID (e.g., ``[LSIS-AFS-101]``).  The ``lint`` CI gate enforces
tag density; the human-curated ``signal/compliance_matrix.rst`` carries the
bidirectional trace.

*   **Explanation**: Every line of code in a flight-certified system must be
    justifiable against an external standard.  Bidirectional traceability links
    each C++ function directly to a specific requirement ID in the LunaNet (LSIS)
    specification, and each requirement back to the exact test case that verifies it.
*   **Current scope**: ``scripts/verify_traceability.py`` scans the C++ source for
    ``[LSIS-AFS-*]`` and ``[SW-SAF-*]`` tags, checks that every tagged requirement
    has at least one test carrying the same tag, and writes the machine-readable
    matrix to ``assurance/evidence/traceability.json``; the ``traceability`` CI
    workflow fails on a gap.  ``task pics-proforma`` regenerates the PICS from the
    shall inventory, and ``task compliance-report`` grades every requirement from
    the test, coverage and performance results.  :doc:`traceability_matrix` and
    ``signal/shall_inventory.rst`` are the human-readable views.
*   **Not done**: generating the matrix from the text of the LSIS PDF itself.  The
    shall inventory that links spec clauses to tags is curated by hand.  A
    blocking CI gate on tag density (all signal-chain headers must carry
    ``[LSIS-AFS-*]`` annotations) is enforced by the ``lint`` CI job.
*   **Benefit**: Transforms compliance from a manual audit into a reproducible,
    traceable record.  An auditor can cross-reference ``signal/compliance_matrix.rst``
    against the implementation tags in headers to verify bidirectional coverage.

Mission-Critical Coding Patterns
--------------------------------

1. SEU-Resistant Enumerations (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard sequential enums (0, 1, 2...) are vulnerable to single-bit flips.
LunaLink uses **non-contiguous bit patterns** held far apart in Hamming distance.

*   **Explanation**: sequential enumerations sit one bit apart — 00, 01, 10.
    One high-energy particle through a status register turns ``SUCCESS`` into a
    valid, wrong ``ERROR``, and nothing announces it.  Values held far apart in
    Hamming distance (``0x5A``, ``0xA5``) close that path: reaching another legal
    value takes several simultaneous flips, so a single upset lands on an illegal
    pattern and reads as a fault.
*   **Implementation**: Patterns like ``0x5A`` (01011010) and ``0xA5`` (10100101).
*   **Benefit**: A register bit-flip transforms a valid status into an identifiable "Invalid" state.

2. Variable Mirroring / Soft-TMR (JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical state variables in performance loops are "mirrored" to detect
ALU or register file corruption *during* calculation.

*   **Explanation**: parity or ECC hardware usually guards system RAM.  The CPU
    registers and the ALU generally get nothing.  A flip that lands *during* a
    calculation, or on a value resting in a local register, produces a wrong
    answer that memory protection never sees.  Soft-TMR mirrors each critical
    variable — the minimum Hamming distance in the BCH decoder, say — against its
    bitwise inverse, and every step checks ``variable == ~mirrored_variable``.
    Transient faults inside the processor's own silicon then surface as
    detections.
*   **Critical Section Requirement**: All writes to a mirrored variable pair (``x`` and ``x_mir``) **must** be performed within a Critical Section (interrupt-masked atomic block). If an RTOS interrupt fires between the write to ``x`` and the write to ``x_mir``, the integrity check will produce a false-positive fault on task resume. See :ref:`isr-safety` for the mandate.
*   **Implementation**: Loop and accumulator variables are paired with a bitwise inverse — ``phi_sum`` / ``phi_sum_mir`` in the LDPC decoder, ``i`` / ``i_mir`` on the hardened loops — and the pair is checked before the value is used.
*   **Benefit**: Detects transient hardware faults in the middle of DSP blocks.

3. Parameter Majority Voting (TMR) (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical parameters (e.g., ``PrnId``, ``Fid``) are stored in
triplets using the ``TmrValue<T>`` template.

*   **Explanation**: Triple Modular Redundancy is the oldest tool in deep-space
    fault tolerance.  Three copies of a mission-critical parameter — a PRN ID, a
    Subframe ID — sit in distinct memory locations.  One bit flip reaches one
    copy; a 2-of-3 majority vote at read time returns the right value anyway.  A
    memory cell that a heavy ion damages permanently costs nothing: the signal
    chain runs on the surviving two, with no ground command needed.
*   **Benefit**: If an SEU flips a bit in one copy, the software corrects the value on-the-fly.

4. Active TMR Repair (NASA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Passive majority voting is insufficient for long-duration missions.

*   **Explanation**: a majority vote masks the fault at read time and leaves it
    in memory, where the next one joins it.  Corrupt copy A on Monday, copy B on
    Tuesday, and the majority now votes for the wrong answer.  Active repair —
    fault scrubbing — overwrites the odd copy with the voted value the moment the
    mismatch appears.  ``TmrValue<T>::repair()`` heals the bit in RAM, resetting
    the clock on radiation damage before single upsets accumulate into multi-bit
    ones.
*   **Implementation**: Done within the ``TmrValue::repair()`` logic.
*   **Benefit**: Prevents the accumulation of Single-Event Upsets (SEUs) over time.

5. Control-Flow Integrity (CFI) (NASA/JAXA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Radiation can cause the Program Counter to jump, leading to skipped
logic or early loop exits.

*   **Explanation**: a heavy ion through the instruction pointer rarely crashes
    the CPU.  It teleports execution somewhere else — past a safety check, a
    parity calculation, a sanitisation step — and the program carries on looking
    healthy.  Control-Flow Integrity keeps an independent record of the path
    actually taken: high-entropy magic patterns (``0xA5A5A5A5``) and sentinel
    counters.  A premature loop exit or a mis-entered function fails the terminal
    check, and the system reaches a safe state instead of broadcasting a poisoned
    navigation signal.
*   **Implementation**:
    *   **Magic Patterns**: safety-critical functions carry high-entropy "Magic Start" and "Magic End" constants (e.g., ``0xA5A5A5A5``).
    *   **Sentinel Counters**: Loops maintain an independent ``cfi_count`` that starts at the Magic Pattern. All increments to ``cfi_count`` must occur within a Critical Section to prevent interrupt-induced false-positive failures.
    *   **Terminal Verification**: At the function exit, the system verifies that ``cfi_count == (Magic_Start + Expected_Iterations)``.
*   **Benefit**: Detects radiation-induced "shortcuts" and logic skips before data is released to the transmitter.

6. Value Invariants & Saturating Logic (ESA/NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Value-constrained wrappers (``CheckedRange<T, Min, Max>``) hold a corrupted
index inside safe buffer bounds.

*   **Explanation**: unsigned C++ arithmetic wraps — ``MAX + 1`` is 0.  Flip a
    bit in a loop index or a buffer offset and that wrap becomes an out-of-bounds
    access or a loop that never ends.  ``CheckedRange<T, Min, Max>`` clamps
    instead: a value driven past its legal range stops at ``Min`` or ``Max``.  The
    signal path stays inside predictable bounds even while one of its parameters
    is poisoned.
*   **Benefit**: Logic uses **Saturating Arithmetic** to prevent catastrophic overflows.

7. Anti-Optimization (Secure Scrubbing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard memory clearing (e.g., ``std::fill``) is vulnerable to
**Dead Store Elimination (DSE)** by optimizing compilers.

*   **Explanation**: sensitive signal data and intermediate parity bits must be
    physically gone from RAM before a function returns; otherwise a later task
    reads them, or a bit flip turns stale bytes into live ones.  ``std::fill`` and
    ``memset`` do not survive the optimiser.  Seeing a buffer that is never read
    again, the compiler deletes the zeroing as a dead store, and the flight binary
    quietly keeps the data.  ``secure_scrub()`` writes through
    ``volatile``-qualified pointers, which the compiler must honour, so the
    overwrite reaches the final binary bit for bit.
*   **Implementation**: ``secure_scrub()`` writes through a ``volatile`` pointer.
*   **Benefit**: Guarantees that sensitive signal data is physically erased from RAM.

8. Memory Section Pinning (JPL/Goddard)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical Look-Up Tables (LUTs) must not be left to the default
placement of the linker.

*   **Explanation**: an on-board computer mixes volatile SRAM, persistent MRAM
    and radiation-hardened PROM, and the banks differ sharply in how well they
    resist upsets.  The large static look-up tables — the BCH codebook, the Weil
    spreading sequences — are the reference data behind every generated symbol, so
    corruption there compromises the whole chain.  The ``LUNALINK_LUT_SECTION``
    macro tags them for the linker, and the mission's linker script pins them to
    the hardest memory the spacecraft carries.
*   **Implementation**: Large tables are tagged with the ``LUNALINK_LUT_SECTION`` macro.
*   **Benefit**: Allows mapping tables to the most radiation-hardened memory banks.

9. Temporal WIP Signaling (Watchdog Feed)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Long-running DSP loops must provide "proof of progress" to the system watchdog.

*   **Explanation**: a hardware watchdog is the last line of defence, but it
    catches only a total hang.  Flip a bit in a jump instruction and the CPU can
    spin in a loop that looks alive to the OS while doing no work at all.
    Safety-critical loops therefore call ``wip_tick()``, incrementing a volatile
    Work-In-Progress counter that an independent monitor watches.  Ticks that stop
    inside the window, or arrive at an impossible rate, trigger a reset — so a
    zombie never survives a transmission slot.
*   **Implementation**: Safety-critical loops call ``wip_tick()``.
*   **Benefit**: Ensures the hardware watchdog can detect if the CPU is stuck.

10. Self-Validating Data Structures (JIT Integrity)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical Look-Up Tables (LUTs) and sparse matrices include embedded CRC32
checksums.

*   **Explanation**: the 5G NR LDPC sub-matrices occupy the largest contiguous
    stretches of RAM in the system, which makes them the likeliest target for a
    strike.  One flipped bit there degrades the transmitted Bit Error Rate
    quietly, and the resulting navigation error is close to undiagnosable from the
    ground.  Each ``LdpcCsrMatrix`` therefore carries an embedded CRC32, and a
    just-in-time integrity check runs before any DSP begins.  A checksum that does
    not match its ground truth halts processing and raises a fault recovery event,
    so poisoned reference data never reaches the broadcast.
*   **Implementation**: The ``LdpcCsrMatrix`` structure includes a ``crc32`` field checked by ``verify_integrity()``.
*   **Benefit**: Detects radiation-induced corruption of large tables in RAM before use.

11. Runtime Aliasing Protection (JAXA/ESA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mandatory runtime verification that output buffers do not overlap with input
buffers.

*   **Explanation**: a flipped bit in a pointer can make an output buffer overlap
    its own input.  The function then overwrites its source while reading it, and
    the resulting corruption is non-deterministic and invisible to the usual logic
    checks.  Terrestrial code leans on ``assert()``, which production builds strip;
    Class B FSW demands **always-on logic gates**.
*   **Implementation**: DSP blocks perform a pointer-range check at the start of the function. If an overlap is detected, the function halts and returns ``kFaultDetected``.
*   **Benefit**: Prevents catastrophic feedback loops and pointer-based memory corruption that standard CFI cannot catch.

12. Data-in-Transit Protection (CCSDS/ESA) — Flight Integration Requirement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Data structures transferred between major DSP pipeline stages shall carry
a **Handoff CRC** verified by the consumer before processing begins.

*   **Explanation**: the hardening strategy covers three threat surfaces.
    Memory section pinning and JIT integrity checks hold *data at rest* (§8, §10);
    variable mirroring and TMR hold *data in processing* (§2, §3).  The third
    surface, *data in transit*, is no safer: an SEU on a pointer or a bus latch
    during a handoff between the LDPC encoder, the interleaver and the modulator
    corrupts the payload without tripping a single in-processing guard.
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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mirrored variables, TMR triplets, and CFI sentinel counters are **not
inherently atomic** and must be protected from preemptive interrupts.

*   **Explanation**: LunaLink targets RTOS environments — RTEMS, VxWorks — with
    preemptive scheduling and hardware interrupt lines.  Let an ISR fire between
    the write to ``x`` and the write to ``x_mir``, and the integrity check that
    runs on resume sees an inconsistent pair and reports a fault that never
    happened.  A false positive costs nearly as much as a missed one: it can drop
    the system into safe mode during a transmission window.  Every atomic
    invariant — mirror-pair writes, TMR triple writes, CFI sentinel increments —
    is therefore wrapped in a **critical section**
    (``LUNALINK_CRITICAL_ENTER`` / ``LUNALINK_CRITICAL_EXIT``).  On the target
    those macros map to the RTOS interrupt-masking primitives
    (``rtems_interrupt_local_disable``); on the host-native digital-twin build
    they map to a compiler barrier (``asm volatile("" ::: "memory")``).
*   **Implementation**: The macros ``LUNALINK_CRITICAL_ENTER`` and
    ``LUNALINK_CRITICAL_EXIT`` are declared in
    ``cpp/include/lunalink/core/safety.hpp``.

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
State variables use the same non-contiguous bit-pattern encoding as
mission-critical enumerations (see §1 of Mission-Critical Coding Patterns).
A single-bit flip in a state register produces an illegal pattern, not a
valid-but-wrong state.

2. Legal Transition Matrix (Formal State Machine)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every state machine in the signal chain is defined by an explicit
**Legal Transition Matrix** — a compile-time table enumerating every
permitted ``(current_state, next_state)`` pair.

*   **Explanation**: upsets corrupt control as readily as data.  A flipped state
    register can drive the signal chain from ``TRANSMITTING`` straight to
    ``INITIALIZING``, skipping ``IDLE`` and commanding the power amplifier into a
    forbidden configuration.  Forbidding undefined states (§3) catches a corrupted
    *value*; it says nothing about a valid state reached over an illegal *edge*.
    The matrix closes that gap: if both ends are valid but
    ``(current_state → next_state)`` is absent from the table, the transition is
    rejected and logged as a critical fault.  Every edge in the graph becomes an
    explicit safety assertion rather than an implicit one.
*   **Implementation**: Each state machine owns a ``constexpr`` transition table of type ``TransitionMatrix<State, N>``. The ``transition()`` method performs a lookup; any miss returns ``kFaultDetected`` and is reported via fixed-width telemetry before the safe-mode transition.
*   **Benefit**: no radiation-induced illegal transition can damage hardware, and every legal system behaviour appears on one auditable map.

3. Forbidden State Detection (NASA/JPL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Any hit to a ``default`` branch in a state transition is treated as a Critical Fault.

*   **Explanation**: when a flip drives a state variable to a value no
    SEU-resistant enumeration defines, guessing the next step is the one
    unacceptable response.  A hit on a ``default`` case is a **critical fault
    event**: the system emits a high-priority telemetry packet and enters safe
    mode rather than continuing on unpredictable logic.
*   **Benefit**: Prevents the software from entering "impossible" logic states caused by register corruption.

.. _fault-isolation:

Fault Containment Architecture (NASA/ESA)
------------------------------------------

Detecting a fault is necessary but insufficient — a Category B system must also
*contain* it. LunaLink defines three nested **Fault Isolation Zones (FIZs)**
that bound the blast radius of any detected fault.

1. Fault Isolation Zones
~~~~~~~~~~~~~~~~~~~~~~~~

*   **Explanation**: with no containment boundary, a fault in the BCH decoder
    reaches the interleaver, corrupts the modulator output, and ends in a watchdog
    reset of the whole OBC.  The cascade does more damage than the fault that
    started it.  A layered model stops it, on the principle of a submarine's
    watertight bulkheads:

    *   **Zone 1 — Block Level**: A fault detected *within* a single DSP block (e.g., a failed mirror check inside ``gather_row()``) is contained to that function. The function returns ``kFaultDetected``, scrubs its local buffers, and does **not** write to the output buffer. The calling frame-assembly logic sees the error code and acts according to Zone 2 policy.
    *   **Zone 2 — Frame Level**: A fault that cannot be recovered within a DSP block is contained at the **Frame Boundary**. The in-progress frame is scrubbed from RAM, the frame counter is advanced, and the signal chain resumes assembly of the **next** frame. The timing system, WIP counters, and RTOS task scheduling are explicitly **not** reset. One corrupted frame therefore never disturbs the broadcast cadence.
    *   **Zone 3 — System Level**: A fault that violates a Zone 2 invariant (e.g., two consecutive frame faults, a failed JIT integrity check, or a Legal Transition Matrix miss in the top-level state machine) triggers a **Safe Mode transition**. The signal chain halts transmission, all buffers are scrubbed, and a fixed-width telemetry fault packet is generated. Recovery from Safe Mode requires a commanded restart from the OBC health management system.

*   **Mandate**: All ``kFaultDetected`` return paths in the core source directories must be annotated with their FIZ level (e.g., ``// FIZ-1``). Any fault that escalates beyond Zone 1 must be logged via the fixed-width telemetry interface before the escalation occurs.
*   **Benefit**: Guarantees that a single radiation event can corrupt at most one 12-second navigation frame, never the timing system or the OBC.

2. Frame Commitment & Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
No signal frame is "released" to hardware until it has been structurally validated.

*   **Explanation**: a corrupted navigation frame is worse than no frame — a rover acts on it and computes the wrong position.  Frame commitment holds the symbol stream back from the DMA or SDR layer until the full 12-second frame is assembled in local RAM and has passed a structural check.  A bit flip during interleaving or LDPC encoding is caught at the waterline, before anything leaves the spacecraft.
*   **Requirement**: The final bit-stream must pass a **Post-Generation Checksum** before transmission. A failed checksum triggers a Zone 2 fault response (see §1 above).

Time-Deterministic Execution (NASA/JAXA)
----------------------------------------

1. Branchless DSP & Data-Independent Execution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink's signal chain is **branchless by mandate** — no data-dependent
branches exist in any ``[LSIS-AFS-*]``-tagged DSP function.

*   **Explanation**: real-time signal processing runs on a fixed execution budget.  Terrestrial code chases the average case with data-dependent branches — ``if (bit == 1) { ... }`` — and pays for it in timing that moves with the message content.  That jitter disturbs the phase-locked loop in the receiver, or misses a deadline outright.  Modulation, multiplexing and the LDPC gather use bitwise masks in place of branches, so ``ldpc_encode()`` takes the same time on an all-zeros input as on an all-ones one.
*   **Benefit**: Guarantees zero timing jitter regardless of navigation message content.

2. Formal WCET Profiling (DO-178C / ECSS-E-ST-40C)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The branchless mandate is not asserted — it is **measured and reported**
as a formal deliverable, published to the documentation site.

*   **Explanation**: Claiming a function is branchless is a design intent; proving it requires measurement. Two Release-only gates do so, both run by ``wcet.yml`` and by ``task performance``:

    *   ``task constant-time`` (``cpp/tests/performance/test_constant_time.cpp``) times the input-independent primitives — the BPSK modulator, CRC-24, the block interleaver and the exhaustive-ML BCH decoder — across adversarial input classes (all-zeros, all-ones, pseudo-random) and fails if the class-to-class spread ``(max − min) / min`` exceeds **14%**, a ceiling set at twice the worst value measured over eight idle runs. A data-dependent branch widens the spread by an order of magnitude, which is what the gate rejects. The LDPC and frame decoders are deliberately excluded: they early-terminate on a satisfied syndrome, so an all-zeros payload genuinely decodes faster, and forbidding that would forbid a correct optimisation.
    *   ``task wcet`` (``test_wcet.cpp``) asserts absolute budgets for the early-terminating stages with the iteration cap forced — ``LDPC decode SF2 (50 iter) < 4 ms``, ``sync_detect < 700 µs``, ``BCH ML decode < 3 µs``, ``frame_decode < 20 ms`` — and produces the **WCET Profile Report** (``wcet_profile.rst``, uploaded as a CI artefact; the raw results are committed as ``docs/assurance/evidence/wcet_results.xml``) with mean, standard deviation and 95% confidence interval per benchmark. A sample whose confidence interval is too wide to support a claim (over 25% of the mean) fails the CI run rather than being reported.

*   **Mandate**: A class-to-class spread above 14% on a branchless primitive, or any budget exceeded, is a **blocking CI failure**. The measured budgets set the windowed watchdog's thresholds (§3 below).
*   **Benefit**: Converts the branchless mandate from a code-review assertion into a measurement-backed CI check, and bounds the data-dependent stages by budget where a spread gate would be wrong.

3. Temporal Windowed Watchdogs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical tasks report their completion to a windowed watchdog.

*   **Explanation**: a plain watchdog notices only that the software has hung.  A windowed one demands progress inside a narrow band — not too late, and **not too early**.  A signal-generation task that finishes early has skipped logic, most likely a radiation-induced jump over a loop; one that finishes late has hit a bottleneck or a hardware delay.  Every frame-generation cycle fits a fixed temporal envelope, and the WCET profile (§2) supplies both thresholds.
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

*   **Explanation**: cosmic rays arrive constantly, so checking only at power-on leaves most of the mission unguarded.  Continuous Built-In Test runs the same checks in the background during operations.  ``bch_codebook_checksum()`` and its kin let a background task confirm that the large reference tables in RAM are still clean before the next navigation frame is built from them.
*   **Benefit**: Detects Single-Bit Upsets in RAM-cached tables before they can pollute signal generation.

3. Fixed-Width Telemetry (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Error logging uses fixed-width binary packets, which cannot corrupt the stack.

*   **Explanation**: variable-length string logging — ``printf`` and its relatives — is a standing source of stack corruption and buffer overruns in embedded code.  Reporting a fault must not manufacture a worse one.  Fixed-width binary packets keep the memory footprint of error reporting constant and known in advance, which holds even on the tightest targets.
*   **Benefit**: fault reporting stays deterministic and cannot overflow the stack.

Supply Chain Security & Reproducibility (ECSS-Q-ST-80C)
-------------------------------------------------------

1. Immutable Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~
All external dependencies are pinned by **Commit Hash (SHA)**.

*   **Explanation**: aerospace projects run for decades.  A mutable tag such as ``v3.0`` can be moved — by an attacker, or by an ordinary mistake — and the next rebuild then behaves differently with no local change to explain it.  Pinning every external dependency, Catch2 among them, to an exact git commit hash keeps the compiled source identical to the source that was verified.
*   **Benefit**: Guarantees source-code consistency across the entire mission lifespan.

2. Toolchain Version Pinning
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system enforces deterministic compilation.

*   **Explanation**: reproducibility is what turns build integrity from a claim into a check.  The binary a developer builds must match the one the flight CI server builds.  ``-ffile-prefix-map`` strips absolute paths, and the non-deterministic compiler features are off.  Any agency can rebuild the source and compare the MD5 of the result against the official flight image, which leaves no room for build-environment artefacts or inserted logic.
*   **Verification**: The MD5 hash of the binary must be identical regardless of the build environment.

Verification & Tooling
----------------------

1. Modified Condition/Decision Coverage (MC/DC) (DO-178C / ECSS-E-ST-40C)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LunaLink mandates **MC/DC** as its primary coverage metric for all
safety-critical DSP logic. Line coverage is a secondary, supplementary metric.

*   **Explanation**: Line coverage proves that a line of code was *executed*. For a safety-critical DSP block containing compound boolean guards (e.g., ``if (seu_detected || range_exceeded)``), line coverage only verifies that the branch was reached. It says nothing about whether each individual condition independently affects the outcome. **Modified Condition/Decision Coverage (MC/DC)**, mandated by DO-178C for Level A software, requires demonstrating that: (1) every condition in a decision has been shown to independently affect the decision's outcome, and (2) every condition has been evaluated as both ``true`` and ``false``. Category B / DO-178C Level B requires only Decision Coverage (DC); LunaLink deliberately targets MC/DC to satisfy the stricter Level A / Category A threshold so that a future upgrade to Category A requires no additional test infrastructure. For LunaLink's SEU detection logic—where the difference between a missed fault and a correct detection is a single boolean evaluation—line coverage cannot carry the safety case at any criticality category.
*   **Mandate**: The design target is **100% MC/DC** on all functions tagged ``[LSIS-AFS-*]`` in the core source directories, as measured by LLVM's source-based coverage instrumentation (``-fprofile-instr-generate -fcoverage-mapping``). The CI gate currently enforces a **30% whole-library MC/DC floor** (``MCDC_COVERAGE_MIN`` in ``.github/workflows/coverage-cpp.yml``); the measured value at submission is recorded in the coverage evidence and badge. The CI pipeline generates an MC/DC report on every passing build.
*   **Structural Coverage**: ≥ 90% C++ *region* coverage (the ECSS Category B primary structural metric) is CI-enforced, with a ≥ 85% C++ line-coverage floor beneath it and 100% Python line coverage. All three are secondary to MC/DC and are reported alongside it. The measured values for this release, per metric and per file with waiver justifications, are in :doc:`coverage_evidence` (``docs/assurance/evidence/coverage_summary.json``); the floors themselves are defined once, in the ``THRESHOLDS`` table of ``scripts/coverage_evidence_report.py``, and consumed from there by ``.github/workflows/coverage-cpp.yml``.
*   **Benefit**: every safety-critical boolean is shown to affect its decision independently. Although Category B only requires Decision Coverage, LunaLink's MC/DC evidence satisfies the stricter DO-178C Level A / Category A threshold, de-risking any future criticality upgrade.

2. Digital Twin Compatibility (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The core library is strictly decoupled from hardware registers via a HAL.

*   **Explanation**: flight hardware is scarce, and waiting for it sets the pace of verification.  A Hardware Abstraction Layer keeps the DSP algorithms clear of hardware registers, so the core runs on an ordinary Linux or macOS workstation at bit-for-bit parity with LEON/SPARC.  The **same source** then meets millions of automated test vectors on a digital twin — a depth of verification no hardware queue would allow.
*   **Benefit**: Enables high-volume verification on terrestrial infrastructure.

3. Information Security (NASA)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
**Stack Scrubbing**: All internal temporary signal buffers are explicitly zeroed.

*   **Explanation**: RAM is shared in a multi-tasking spacecraft.  A finished DSP function leaves navigation data and intermediate parity bits in its temporary buffers, and the next task to receive that memory can read them.  ``secure_scrub()`` overwrites those buffers before the function returns, closing the cross-task leak and meeting NASA's FSW information-security mandate.
*   **Implementation**: calls ``secure_scrub()`` before function exit.

4. Static & Dynamic Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*   **Clang-Tidy**: Enforces MISRA-aligned High-Integrity C++ (HICPP) rules.
*   **Cppcheck**: Secondary safety-critical static analyzer.
*   **ASan/UBSan**: Catch memory safety and undefined behavior at runtime.

*   **Explanation**: no test suite proves the absence of bugs.  Clang-Tidy and Cppcheck read the source for logic errors, memory-safety defects and style violations that no particular test vector would reach.  ASan and UBSan watch the running code for buffer overflows and signed-integer wrapping.  Paired with MC/DC on the safety-critical functions and ≥ 90% C++ region coverage, they turn a claim of predictability into measured evidence.
*   **Coverage**: Targets **100% MC/DC** on ``[LSIS-AFS-*]``-tagged functions (design target; the CI gate enforces a 30% whole-library floor — see §Verification & Tooling), ≥ 90% C++ region coverage and a ≥ 85% C++ line-coverage floor (both CI-enforced), and 100% Python coverage. Measured values: :doc:`coverage_evidence`.

5. Compiler-Level Defenses & Forbidden Optimizations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The build system mandates strict hardening flags and explicitly forbids
compiler optimizations that undermine LunaLink's safety patterns.

*   **Explanation**: a modern compiler offers defences against exploits and hardware faults with one hand and, with the other, optimisations that quietly void the guarantees these hardening patterns rest on.  Two categories of flag are therefore managed:

    *   **Mandatory Hardening Flags**: ``-fstack-protector-strong`` inserts "canaries" into the stack to detect and halt execution if a radiation-induced bit flip corrupts a function return address. ``-ffp-contract=off`` keeps floating-point results bit-for-bit identical across processors, which is what holds signal-tracking performance the same between a ground-station simulator and the spacecraft.

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Every hardening pattern must be **empirically verified** to detect the fault
it claims to detect. Documentation of a pattern is not a safety case.
Demonstration of a pattern is.

*   **Explanation**: A team can implement variable mirroring, TMR, and CFI sentinels with perfect fidelity and still have a latent defect — for example, a critical section that is one instruction too short, allowing a race that the pattern silently misses. The only way to close this gap is **adversarial testing**: deliberately inject the exact fault each pattern is designed to catch, and assert that the detection fires within the expected number of execution cycles. LunaLink maintains a dedicated SWIFI test suite (``cpp/tests/<domain>/test_swifi_*.cpp``) that is executed as a blocking CI stage. Each test follows the same three-phase structure: **Inject** → **Execute** → **Assert Detection**.

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
*   **Mutation testing (planned, not implemented)**: A planned CI addition will
    run mutation testing (via ``mutate-cpp`` or equivalent) against
    ``[LSIS-AFS-*]``-tagged functions.  Mutation testing deliberately introduces
    source-level faults (operator flips, condition negations) and verifies the test
    suite kills every mutant.  This proves that the MC/DC test vectors are not only
    achieving coverage but are semantically sensitive to the logic they cover.
    It is not scheduled; the SWIFI suite provides the primary fault-detection
    coverage evidence for this release.
*   **Benefit**: Converts the hardening specification from a set of engineering claims
    into a set of empirically verified, reproducible safety assertions.  The
    ``fault_coverage.rst`` report records 100% single-bit fault detection coverage
    measured by the SWIFI suite.

7. Golden Test Vector Suite (CCSDS / LunaNet ICD)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The shared test vectors are a **versioned, machine-readable library** that
other LSIS-AFS implementations check themselves against.

*   **Status**: **delivered**, as a separate repository with tagged releases,
    `luar-space/lsis-afs-test-vectors
    <https://github.com/luar-space/lsis-afs-test-vectors>`__.  LunaLink's
    export scripts (``scripts/export_test_vectors_*.py``) produced them.
    Levels 1 and 2 are checked against sources other than LunaLink (Annex 3,
    LANS-AFS-SIM), level 4 is an independent receiver's decode
    (PocketSDR-AFS) of the level 3 signals, and levels 3 and 5 and the FEC
    components are checked for structure and against the other levels.  The
    repository's CI runs those checks.
    ``task conformance`` holds LunaLink to a pinned release with the same
    ``validate.py diff-*`` commands any implementation runs.  The files under
    ``test-vectors/`` in this repository are offline test fixtures, not the
    published set.

*   **Design intent**: a reference implementation owes two things — its own
    correctness, and a way for others to check theirs against it.  The shared
    vectors discharge the second.  LSIS V1.0 is the normative text: when an
    implementation disagrees with a vector, the disagreement is resolved
    against the standard, and a vector found wrong is corrected in a new
    release that LunaLink then has to pass.

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
*   **Benefit**: any hardware or software implementation, LunaLink included,
    checks itself against the same published, machine-readable set.

ECSS Compliance Status
----------------------

The assurance infrastructure is structured against **ECSS-E-ST-40C Rev.1** and
**ECSS-Q-ST-80C Rev.2** (Category B — mission-critical).  NASA NPR 7150.2D
serves as a secondary cross-reference; meeting ECSS covers the corresponding NPR
requirements.

**Satisfied (strong evidence)**:

.. list-table::
   :header-rows: 1
   :widths: 18 15 67

   * - ECSS Clause
     - Requirement
     - Evidence
   * - E-ST-40C §5.5.1
     - Coding Standards
     - Design Constraints section; compiler flag assertions; ``static_analysis.rst`` (CI-generated, published to the docs site)
   * - E-ST-40C §5.3
     - Requirements Traceability
     - Bidirectional traceability mandate; ``compliance_matrix.rst``
   * - Q-ST-80C §5.5
     - Verification (static analysis / inspection)
     - Clang-Tidy + Cppcheck zero-violation reports on ``gh-pages``
   * - E-ST-40C §5.5.3–4
     - Unit & Integration Testing
     - MC/DC design target 100% on safety-tagged functions (30% whole-library
       CI floor; measured value in coverage evidence); SWIFI 100% single-bit
       fault detection; Digital Twin compatibility
   * - Q-ST-80C §5.4
     - Configuration Management
     - SHA-pinned dependencies; reproducible builds; SCI version register; ``scmp.rst``
   * - Q-ST-80C §5.3/§5.5
     - Software FMEA / Safety
     - ``fmea.rst`` — full software FMEA table; ``fault_coverage.rst``
   * - Q-ST-80C §5.7
     - Software Reuse Assessment
     - ``tool_confidence.rst`` — tool confidence / COTS assessment

**Organisational requirements — outside the scope of this reference implementation**:

IV&V (Q-ST-80C §5.5), formal review milestones (E-ST-40C §5.1), a formal CM board
(Q-ST-80C §5.4) and physical configuration audit (Q-ST-80C §5.4) all need programme
office infrastructure; no development team can satisfy them alone.
These items are tailored in the ECSS standards and tailoring record
(LUNALINK-STDS-001, ``docs/standards/esa/ecss_standards.rst`` §Tailoring
Record) and the corresponding NPR cross-references are provided in the NASA
Compliance Addendum (``docs/standards/nasa_compliance_addendum.rst``).

8. Exhaustive Static Analysis Report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Static analysis results are **published as formal artifacts**, not cited as
passing claims.

*   **Explanation**: Stating "we run Clang-Tidy" is a process claim. Publishing the full output with zero violations is a compliance proof. LunaLink's CI pipeline generates three static analysis reports on every passing build and publishes them to the **documentation site** (``gh-pages``), giving each a stable, permanent, publicly accessible URL. They are not committed to the ``main`` branch — generated outputs belong on ``gh-pages``, not in code history:

    *   ``static_analysis.rst``: Full Clang-Tidy output for the core source directories, showing zero MISRA-aligned HICPP violations. Any new violation is a blocking CI failure.
    *   ``cppcheck.rst``: Full Cppcheck output, showing zero errors and zero warnings at the ``--enable=all`` level.
    *   ``coverage.rst``: LLVM coverage report showing MC/DC percentages per function for all ``[LSIS-AFS-*]``-tagged functions, plus branch and line coverage across the core source directories (the CI-enforced floors are stated in ``metrics_plan.rst``).

*   **One Command per Report**: each report is regenerated by a task in the project Taskfile — ``task tidy``, ``task cppcheck`` and ``task coverage-cpp`` respectively. Any reviewer can independently reproduce every number in these reports from a clean checkout.
*   **Benefit**: Transforms analysis from an invisible CI step into a transparent, auditable record — giving reviewers direct evidence of zero violations rather than requiring them to trust the claim.
