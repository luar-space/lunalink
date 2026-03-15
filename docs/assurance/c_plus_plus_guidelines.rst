C++20 Style Guide for ECSS Class B Certifiable Software
=======================================================

Purpose and Scope
-----------------
This guide defines the C++20 subset, patterns, and verification expectations
required for deterministic, fault-tolerant, radiation-hardened flight software.
It is derived from the mission assurance principles documented in
``docs/assurance/mission_assurance.rst`` and targets ECSS-E-ST-40C Category B
software. Where practical, it aligns with stricter Category A / DO-178C Level A
controls to de-risk future upgrades.

Definition of Flight Code
-------------------------
Flight code comprises the production signal-chain library and its public
headers. For this repository, flight code includes:
- ``cpp/signal/`` implementation sources.
- ``cpp/include/lunalink/signal/`` public headers.
Any other targets (tests, tools, benchmarks, host utilities) are non-flight
code and may use broader C++ features as long as they are not linked into flight
binaries.

Core Objectives
---------------
- Determinism: no hidden runtime variability, bounded execution paths.
- Fault containment: all detected faults must be observable and contained.
- Radiation resilience: SEU-safe state encoding, CFI, and data integrity checks.
- Auditability: traceable requirements, reproducible verification evidence.

Language and Build Baseline
---------------------------
1. C++20 only. Compiler extensions are forbidden except for the explicit
   allowlist below, used for ABI stability or memory section placement.
2. Allowed compiler extensions (explicit allowlist):
   - ``[[gnu::section(\"...\")]]`` for LUT/critical data pinning.
   - ``[[gnu::packed]]`` for binary layout stability (e.g., SCI header).
   - ``alignas(N)`` for explicit alignment guarantees.
   - ``#pragma once`` for include guards.

2. All safety-critical functions must be ``noexcept``.
3. Use ``[[nodiscard]]`` for all status-returning APIs.
4. Prefer ``constexpr`` and ``constinit`` for all static data that can be
   compile-time defined. Avoid dynamic initialization in flight paths.
5. Enforce compiler hardening flags and forbid unsafe optimizations.
6. Exceptions and RTTI are forbidden in flight code. Flight builds must include
   ``-fno-exceptions`` and ``-fno-rtti`` and flight sources must not use
   ``throw``, ``try``/``catch``, ``dynamic_cast``, or ``typeid``. Non-flight
   targets may enable these features explicitly in their build options.

Required build flags
~~~~~~~~~~~~~~~~~~~~
- ``-fstack-protector-strong``
- ``-ffp-contract=off``

Forbidden flags
~~~~~~~~~~~~~~~
- ``-fstrict-aliasing``
- ``-funroll-loops``
- ``-fomit-frame-pointer``
- ``-ffast-math``
- ``-funsafe-math-optimizations``
- ``-fdelete-null-pointer-checks``

Memory and Allocation
---------------------
1. Zero dynamic allocation after initialization. No ``new``, ``delete``,
   ``malloc``, or ``free`` in flight execution paths.
2. Use static storage or stack allocation with bounded size.
3. Validate all buffer sizes and indices at function entry.
4. All temporary buffers must be scrubbed before function exit using
   ``secure_scrub()`` or ``secure_scrub_scalar()``.

Control Flow and Complexity
---------------------------
1. No recursion.
2. All loops must have fixed, verifiable upper bounds.
3. Cyclomatic complexity per function must be <= 10.
4. All ``[LSIS-AFS-*]`` tagged DSP functions shall have a single return point.
5. Control-plane functions may use guard-clause returns when they improve fault
   containment clarity.

Determinism and Timing
----------------------
1. No runtime allocation, lazy initialization, or hidden dynamic initialization
   in flight paths.
2. Avoid function-local static initialization in critical paths. Use explicit
   init steps instead.
3. All algorithms must have bounded execution time based on compile-time or
   validated runtime parameters.

Traceability and Requirement Tagging
------------------------------------
1. Every mission-critical function in ``cpp/signal/`` must be tagged with a
   requirement ID like ``[LSIS-AFS-101]``.
2. A bidirectional compliance matrix must map each requirement to source and
   tests.
3. Tag density is enforced by CI.

Hardening Patterns
------------------
These patterns are mandatory when applicable to the domain logic.

SEU-resistant enumerations
~~~~~~~~~~~~~~~~~~~~~~~~~~
Use non-contiguous bit patterns with high Hamming distance to resist single-bit
state corruption.

Variable mirroring (Soft-TMR)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Critical variables must be mirrored with their bitwise inverse. All updates to
mirror pairs must be performed within a critical section to avoid interrupt-
induced false positives.

Parameter TMR and active repair
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mission-critical parameters must use TMR voting with active repair on majority
vote.

Control-flow integrity (CFI)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Use magic sentinel counters and terminal checks to detect skipped logic. All
CFI counters must be updated inside critical sections.

Saturating logic and checked ranges
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Use value-constrained wrappers (e.g., ``CheckedRange<T, Min, Max>``) to clamp
corrupted values rather than wrapping.

Secure scrubbing
~~~~~~~~~~~~~~~~
Use volatile-based scrubbers to prevent dead-store elimination. Do not rely on
``std::fill`` or ``memset`` for sensitive data.

Memory section pinning
~~~~~~~~~~~~~~~~~~~~~~
All LUTs and large reference data must be tagged with the LUT section macro so
the linker can place them in hardened memory.

Temporal WIP signaling
~~~~~~~~~~~~~~~~~~~~~~
Long-running DSP loops must call ``wip_tick()`` to prove progress to the
watchdog.

Just-in-time integrity
~~~~~~~~~~~~~~~~~~~~~~
Large data structures must include CRC32 (or stronger) and verify integrity
before use.

Runtime aliasing protection
~~~~~~~~~~~~~~~~~~~~~~~~~~~
All DSP functions must validate that output buffers do not overlap input buffers
before any writes occur.

Handoff CRC
~~~~~~~~~~~
All inter-block handoff buffers must include CRCs and be validated by the
consumer before processing.

Legal transition matrices
~~~~~~~~~~~~~~~~~~~~~~~~~
FSMs must define explicit legal transition sets. Illegal transitions must
return a fault and trigger safe-mode escalation and telemetry.

Error Handling and Fault Containment
------------------------------------
1. Use explicit status enums and return codes.
2. All fault detections must be observable and mapped to telemetry.
3. All fault paths must scrub outputs before returning.
4. Reserved telemetry fields must be zero-filled.

HAL and Digital Twin Compatibility
----------------------------------
1. All hardware access must be via HAL interfaces.
2. Core DSP logic must run on the Digital Twin without hardware dependencies.
3. Floating-point behavior must be bit-stable across target and host. Do not
   use unsafe floating-point optimizations.

Testing and Evidence Requirements
---------------------------------
1. ``[LSIS-AFS-*]`` tagged functions must achieve 100 percent MC/DC.
2. Branch coverage must be >= 95 percent across ``cpp/signal/``.
3. Line coverage must be >= 90 percent overall for C++.
4. SWIFI tests must exist for every hardening pattern.
5. Fault injection must prove detection, not just claim it.
6. Static analysis reports must be published as formal artifacts.

Style and API Patterns
----------------------
1. Prefer ``std::span`` for buffer parameters.
2. Use ``std::array`` for fixed-size buffers.
3. Use ``uint8_t`` or ``std::byte`` for raw data explicitly.
4. Prefer ``const`` and ``constexpr`` for all immutable data.
5. Prefer ``enum class`` over plain enums.
6. Public APIs must validate inputs and sizes before work starts.
7. Any unsafe cast must be explicitly annotated and justified.
8. STL usage policy for flight paths:

   - Allowed: ``std::array``, ``std::span``, ``std::optional``, ``std::pair``,
     ``std::tuple`` (fixed-size usage only), ``std::bitset``.
   - Forbidden: ``std::vector``, ``std::string``, ``std::deque``, ``std::list``,
     ``std::map``, ``std::unordered_map``, ``std::set``, ``std::unordered_set``,
     ``std::function``, ``std::shared_ptr``, ``std::unique_ptr``, and any
     allocator-backed container or type that can allocate at runtime.

Pitfalls to Avoid
-----------------
1. Unbounded loops or data-dependent termination.
2. Implicit heap allocation via STL containers in flight paths.
3. Function-local static initialization in safety-critical paths.
4. Missing ``[[nodiscard]]`` on status-returning APIs.
5. Using forbidden compiler flags.
6. Skipping alias checks or buffer overlap validation.
7. Multi-return DSP functions that could bypass scrubbing.
8. Silent failure states without telemetry or safe-mode escalation.

Safety-Critical Function Template
---------------------------------
.. code-block:: cpp

   [[nodiscard]] StatusType do_critical_thing(std::span<const uint8_t> in,
                                              std::span<uint8_t> out) noexcept {
     StatusType status = StatusType::kOk;
     uint32_t cfi = 0xA5A5A5A5U;

     if (out.size() < kExpectedSize || in.empty()) {
       status = StatusType::kInvalidInput;
     }

     if (status == StatusType::kOk) {
       for (size_t i = 0; i < kExpectedSize; ++i) {
         // work...
         LUNALINK_CRITICAL_ENTER;
         cfi++;
         LUNALINK_CRITICAL_EXIT;
         wip_tick();
       }
     }

     if (cfi != (0xA5A5A5A5U + kExpectedSize)) {
       status = StatusType::kFaultDetected;
     }

     if (status != StatusType::kOk) {
       secure_scrub(out);
     }

     return status;
   }
