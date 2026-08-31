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

Standards Applicability
-----------------------
This guide is the union of requirements from the following standards, resolved
to the strictest rule where standards overlap:

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Standard
     - Identifier
     - Role
   * - MISRA C++:2023
     - MISRA
     - Primary coding rule set. 179 guidelines (4 Directives + 175 Rules).
       The full Guideline Compliance Summary (``misra_cpp2023_gcs.rst``,
       LUNALINK-MISRA-GCS-001) is maintained in the development tree and is
       not shipped in this release; the per-rule record in this document is
       the shipped compliance record.
   * - JPL Institutional Coding Standard
     - JPL D-60411
     - 31 rules (LOC-1 through LOC-4) for mission-critical C. Applicable to
       C++ by extension.
   * - Power of Ten Rules
     - P10 (Holzmann)
     - 10 foundational rules for safety-critical code. Subset of JPL standard.
   * - NASA GSFC C++ Coding Standards
     - NASA-GSFC
     - Style and safety guidelines for C++ flight software.
   * - N7 Space / ESA C++20 for Flight Software
     - N7S-CG-001
     - 43 general + 5 dynamic memory guidelines. ESA-commissioned study on
       C++20 viability for spacecraft software.
   * - ECSS-E-ST-40C Rev.1
     - ECSS-SW
     - Overarching software engineering process standard. Requires *a* coding
       standard (§5.5.1) — this document satisfies that requirement.
   * - ECSS-Q-ST-80C Rev.2
     - ECSS-PA
     - Software product assurance. Requires static analysis (§5.5) and coding
       standard compliance evidence.

Compatibility Assessment
~~~~~~~~~~~~~~~~~~~~~~~~
All six coding standards are **fully compatible** for LunaLink's scope. The few
minor tensions are resolved as follows:

- **MISRA 6.7.1** (no function-local statics) vs. **P10 Rule 6** (smallest scope):
  LunaLink bans function-local statics in flight paths; uses file-scope ``constinit``
  for persistent data.
- **MISRA 21.6.1** (dynamic memory *should not* be used, Advisory) vs. **JPL Rule 5**
  (dynamic memory *shall not* be used after init): LunaLink's zero-alloc-after-init
  policy is stricter than both.
- **MISRA 9.6.1** (goto *should not* be used, Advisory) vs. **JPL Rule 11** (goto
  *shall not*): LunaLink bans goto entirely, satisfying both.
- **NASA-GSFC §2.15** (multiple returns for efficiency) vs. **LunaLink single-exit
  DSP policy**: Single return for ``[LSIS-AFS-*]`` tagged DSP functions (ensures
  scrubbing); guard-clause returns permitted for control-plane functions.
- **NASA-GSFC §1.9** (use ``NULL``) vs. **MISRA 7.11.1** (use ``nullptr``):
  ``nullptr`` is correct for C++20. NASA doc predates C++11.

Definition of Flight Code
-------------------------
Flight code comprises the production signal-chain library and its public
headers. For this repository, flight code includes:

- ``cpp/{codes,encoding,messages,signal,platform}/`` implementation sources.
- ``cpp/include/lunalink/signal/`` public headers.

Any other targets (tests, tools, benchmarks, host utilities) are non-flight
code and may use broader C++ features as long as they are not linked into flight
binaries.

Core Objectives
---------------
- **Determinism**: no hidden runtime variability, bounded execution paths.
  [P10 Rules 2-3; MISRA Dir 0.3.1; N7S-CG-001 §schedulability]
- **Fault containment**: all detected faults must be observable and contained.
  [ECSS-Q-ST-80C §5.3; MISRA 4.1.3]
- **Radiation resilience**: SEU-safe state encoding, CFI, and data integrity checks.
  [LunaLink-specific; exceeds all referenced standards]
- **Auditability**: traceable requirements, reproducible verification evidence.
  [ECSS-E-ST-40C §5.5.1; NASA SWE-107]

Language and Build Baseline
---------------------------
[MISRA 4.1.1; N7S-CG-001 §language-baseline]

1. C++20 only. Compiler extensions are forbidden except for the explicit
   allowlist below, used for ABI stability or memory section placement.
2. Allowed compiler extensions (explicit allowlist):

   - ``[[gnu::section(\"...\")]]`` for LUT/critical data pinning.
   - ``[[gnu::packed]]`` for binary layout stability (e.g., SCI header).
   - ``alignas(N)`` for explicit alignment guarantees.
   - ``#pragma once`` for include guards. [MISRA 19.2.1 — alternative mechanism]

3. All safety-critical functions must be ``noexcept``.
   [MISRA 18.4.1; N7S-CG-001 §exceptions]
4. Use ``[[nodiscard]]`` for all status-returning APIs.
   [MISRA 0.1.2; JPL Rule 14; P10 Rule 7]
5. Prefer ``constexpr`` and ``constinit`` for all static data that can be
   compile-time defined. Avoid dynamic initialization in flight paths.
   [N7S-CG-001 §compile-time; MISRA 6.7.1]
6. Enforce compiler hardening flags and forbid unsafe optimizations.
7. Exceptions and RTTI are forbidden in flight code. Flight builds must include
   ``-fno-exceptions`` and ``-fno-rtti`` and flight sources must not use
   ``throw``, ``try``/``catch``, ``dynamic_cast``, or ``typeid``. Non-flight
   targets may enable these features explicitly in their build options.
   [N7S-CG-001 §exceptions, §rtti; project policy]

Required build flags
~~~~~~~~~~~~~~~~~~~~
[JPL Rule 2; P10 Rule 10; N7S-CG-001 §tooling]

- ``-fstack-protector-strong``
- ``-ffp-contract=off``

Forbidden flags
~~~~~~~~~~~~~~~
- ``-fstrict-aliasing``
- ``-funroll-loops``
- ``-fomit-frame-pointer``
- ``-ffast-math`` [MISRA Dir 0.3.1]
- ``-funsafe-math-optimizations``
- ``-fdelete-null-pointer-checks``

Memory and Allocation
---------------------
[MISRA 21.6.1-5; JPL Rule 5; P10 Rule 3; N7S-CG-001 §dynamic-memory]

1. Zero dynamic allocation after initialization. No ``new``, ``delete``,
   ``malloc``, or ``free`` in flight execution paths.
2. Use static storage or stack allocation with bounded size.
3. Validate all buffer sizes and indices at function entry.
   [JPL Rule 15; P10 Rule 7]
4. All temporary buffers must be scrubbed before function exit using
   ``secure_scrub()`` or ``secure_scrub_scalar()``.

Control Flow and Complexity
---------------------------
[MISRA 8.2.10, 9.6.1; JPL Rules 3-4, 11, 25; P10 Rules 1-2, 4]

1. No recursion. [MISRA 8.2.10 Required; JPL Rule 4; P10 Rule 1]
2. All loops must have fixed, verifiable upper bounds. [JPL Rule 3; P10 Rule 2]
3. Cyclomatic complexity per function must be <= 10.
   [JPL Rule 25; enforced by ``readability-function-cognitive-complexity``]
4. All ``[LSIS-AFS-*]`` tagged DSP functions shall have a single return point.
5. Control-plane functions may use guard-clause returns when they improve fault
   containment clarity. [NASA-GSFC §2.15 compatibility]
6. No ``goto``, ``setjmp``, or ``longjmp``.
   [MISRA 9.6.1; JPL Rule 11; P10 Rule 1; MISRA 21.10.2]
7. Functions should be no longer than 60 lines. [JPL Rule 25; P10 Rule 4]

Determinism and Timing
----------------------
[N7S-CG-001 §schedulability; P10 Rules 2-3]

1. No runtime allocation, lazy initialization, or hidden dynamic initialization
   in flight paths. [MISRA 6.7.1; N7S-CG-001]
2. Avoid function-local static initialization in critical paths. Use explicit
   init steps instead. [MISRA 6.7.1]
3. All algorithms must have bounded execution time based on compile-time or
   validated runtime parameters. [P10 Rule 2]

Type Safety
-----------
[MISRA §4.7; JPL Rule 17; NASA-GSFC §1.8]

1. Use fixed-width integer types (``uint8_t``, ``int32_t``, etc.) instead of
   ``int``, ``short``, ``long``. [MISRA 6.9.2; JPL Rule 17]
2. Use ``enum class`` with explicit underlying type.
   [MISRA 10.2.1, 10.2.2]
3. No C-style casts. Use ``static_cast`` only. [MISRA 8.2.2]
4. No ``reinterpret_cast`` except in HAL layer with formal deviation.
   For type punning between trivially-copyable types of the same size,
   prefer ``std::bit_cast<T>(x)`` (``<bit>``, C++20) over ``reinterpret_cast``
   or ``std::memcpy``: it is ``constexpr``, has defined behaviour, and
   produces no aliasing UB. [MISRA 8.2.5; C++20 §[bit.cast]]
5. No ``const_cast``. [MISRA 8.2.3]
6. No implicit narrowing conversions. Use explicit ``static_cast`` with
   justification. [MISRA 7.0.5, 7.0.6]
7. For comparisons between operands of different signedness, prefer the C++20
   safe-comparison functions ``std::cmp_equal``, ``std::cmp_not_equal``,
   ``std::cmp_less``, ``std::cmp_less_equal``, ``std::cmp_greater``,
   ``std::cmp_greater_equal`` (``<utility>``) over ``static_cast`` bridges.
   For range-narrowing checks prior to a cast, prefer ``std::in_range<T>(x)``
   over manual bounds tests. Rationale: makes MISRA 7.0.5/7.0.6 compliance
   explicit at the call site rather than implicit in reviewer-checked casts;
   the helpers are ``constexpr``, header-only, allocation- and exception-free,
   so they are flight-path safe. [MISRA 7.0.5, 7.0.6; C++20 §[utility.intcmp]]
8. GCC promotes ``uint8_t`` to ``int`` in expressions — always use explicit
   ``static_cast`` on XOR/shift results to avoid ``-Wsign-conversion -Werror``.

Preprocessor Usage
------------------
[MISRA §4.19; JPL Rules 20-23; P10 Rule 8]

1. Limit preprocessor to file inclusion and simple macros. [P10 Rule 8]
2. No function-like macros except for hardening patterns (``LUNALINK_*``).
   [MISRA 19.0.2; deviation documented in GCS]
3. No ``#define`` within a function or block. [JPL Rule 21]
4. ``#include`` directives shall only be preceded by preprocessor directives
   or comments. [MISRA 19.0.3; JPL Rule 31]

STL Usage Policy
----------------
[N7S-CG-001 §standard-library; MISRA 21.6.1]

Flight paths:

- **Allowed**: ``std::array``, ``std::span``, ``std::optional``, ``std::pair``,
  ``std::tuple`` (fixed-size usage only), ``std::bitset``, ``std::variant``,
  ``std::atomic``.
  [N7S-CG-001: types that never allocate]
- **Forbidden**: ``std::vector``, ``std::string``, ``std::deque``, ``std::list``,
  ``std::map``, ``std::unordered_map``, ``std::set``, ``std::unordered_set``,
  ``std::function``, ``std::shared_ptr``, ``std::unique_ptr``, and any
  allocator-backed container or type that can allocate at runtime.
  [N7S-CG-001: types that may allocate; MISRA 21.6.1]
- **Not applicable**: IO, regular expressions, OS-based thread support,
  filesystem. [N7S-CG-001 §stl-parts-of-no-interest]

Ranges (``<ranges>``, ``std::ranges::``):

- **Allowed** on flight paths: the basic algorithms ``std::ranges::fill``,
  ``std::ranges::copy``, ``std::ranges::copy_n``, ``std::ranges::transform``,
  ``std::ranges::find``, ``std::ranges::equal``, ``std::ranges::mismatch``,
  ``std::ranges::for_each`` (with a ``noexcept`` callable). These are
  semantically equivalent to their ``<algorithm>`` counterparts and add safer
  iterator/range validation without allocation or exception paths.
- **Forbidden** on flight paths: ``std::ranges::views::*`` and any view
  adapter (``filter``, ``take``, ``drop``, ``transform_view``, etc.). View
  pipelines rely on lazy evaluation, complex constraint machinery and
  throwing iterator paths incompatible with ``-fno-exceptions``.
  [N7S-CG-001 §stl-parts-of-no-interest; MISRA 18.4.1]

Traceability and Requirement Tagging
------------------------------------
[ECSS-E-ST-40C §5.3; NASA SWE-088]

1. Every mission-critical function in the core source directories must be tagged with a
   requirement ID like ``[LSIS-AFS-101]``.
2. A bidirectional compliance matrix must map each requirement to source and
   tests.
3. Tag density is enforced by CI.

Hardening Patterns
------------------
These patterns are mandatory when applicable to the domain logic. They exceed
all referenced standards and represent LunaLink's radiation-hardening approach.

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

Compile-time initialization (``constinit``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
All ``extern const`` mission-data tables (PRN codebooks, LDPC matrices, BCH
codewords, frame sync patterns, SCI headers, FSM transition tables) must be
declared ``constinit`` at their point of definition. ``constinit`` is a C++20
compile-time assertion that the variable has constant initialization — it
eliminates the static-initialization-order fiasco, guarantees the table is
ready before any dynamic initializer runs, and converts any future regression
that introduces dynamic init into a build failure rather than a latent startup
fault. Combine with ``alignas(64)`` and the LUT section macro:

.. code-block:: cpp

   alignas(64) LUNALINK_LUT_SECTION
       constinit const std::array<uint8_t, kSyncLength> kSyncPattern = { ... };

[ECSS-Q-ST-80C §5.5; C++20 §[dcl.constinit]]

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
[MISRA 18.4.1, 18.5.2; N7S-CG-001 §error-handling]

1. Use explicit status enums and return codes. [MISRA 0.1.2]
2. All fault detections must be observable and mapped to telemetry.
3. All fault paths must scrub outputs before returning.
4. Every status-returning function (status enums, ``Result``/``Status``
   structs, ``bool`` safety predicates) shall be declared
   ``[[nodiscard("<reason>")]]`` with a rationale string that names the
   failure mode the caller suppresses by discarding the value. C++20
   reason-attached ``[[nodiscard]]`` turns the compiler diagnostic itself
   into traceability evidence and removes ambiguity about *why* the result
   must be checked. Bare ``[[nodiscard]]`` is permitted only for non-status
   return values (e.g. pure-function arithmetic results).
   [MISRA 18.4.1; ECSS-E-ST-40C §5.3.5; C++20 §[dcl.attr.nodiscard]]
5. Reserved telemetry fields must be zero-filled.

HAL and Digital Twin Compatibility
----------------------------------
[N7S-CG-001 §rtos-compatibility]

1. All hardware access must be via HAL interfaces.
2. Core DSP logic must run on the Digital Twin without hardware dependencies.
3. Floating-point behavior must be bit-stable across target and host. Do not
   use unsafe floating-point optimizations. [MISRA Dir 0.3.1]

Testing and Evidence Requirements
---------------------------------
[ECSS-E-ST-40C §5.5.3; ECSS-Q-ST-80C §5.5; NASA SWE-127, SWE-128]

1. ``[LSIS-AFS-*]`` tagged functions target 100 percent MC/DC (design
   target; the CI gate enforces a 30 percent whole-library floor — see
   ``metrics_plan.rst``).
   [ECSS Category A practice; NASA SWE-127]
2. Branch coverage must be >= 95 percent across the core source directories.
3. Line coverage must be >= 90 percent overall for C++.
4. SWIFI tests must exist for every hardening pattern.
5. Fault injection must prove detection, not just claim it.
6. Static analysis reports must be published as formal artifacts.
   [ECSS-Q-ST-80C §5.5; NASA SWE-108; P10 Rule 10]

Tooling Enforcement
-------------------
Static analysis is enforced by two independent tools, per ECSS-Q-ST-80C §5.5,
NASA SWE-108, and Power of Ten Rule 10.

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - Tool
     - Configuration
     - Role
   * - **clang-tidy**
     - ``.clang-tidy``
     - Primary MISRA C++:2023 enforcement. Each check is mapped to its
       corresponding MISRA rule in the configuration file. Also enforces
       JPL, CERT, and C++ Core Guidelines checks.
   * - **cppcheck**
     - ``cppcheck.cfg`` + ``cppcheck.suppressions``
     - Complementary bug-finding: buffer overruns, null dereferences,
       uninitialized variables, use-after-free, memory leaks. Independent
       from clang-tidy to reduce false negatives per MISRA Compliance:2020.
   * - **Clang Static Analyzer**
     - ``scan-build`` with checkers: core, deadcode, security, unix,
       cplusplus, nullability
     - Path-sensitive inter-procedural analysis. Detects null dereferences
       through conditional paths, use-after-free, and dead stores that
       pattern-based tools miss. CI gate: ``tidy.yml`` (``clang-sa`` job).
   * - **clang-format**
     - ``.clang-format``
     - Automated code formatting. LLVM-based style with flight-software
       adjustments (100-col, Attach braces, no bin-packing).
       CI gate: ``lint.yml``.
   * - **include-what-you-use**
     - CMake ``CMAKE_CXX_INCLUDE_WHAT_YOU_USE``
     - Header hygiene analysis. Verifies minimal ``#include`` sets.
       Advisory (not a CI gate) due to platform-specific false positives.
   * - **Compiler**
     - CMake flags
     - ``-Wall -Wextra -Wpedantic -Werror -Wsign-conversion -Wconversion
       -Wshadow -Wdouble-promotion -Wformat=2 -Wnull-dereference``

MISRA Compliance
~~~~~~~~~~~~~~~~
MISRA C++:2023 compliance is self-declared per MISRA Compliance:2020. The
formal Guideline Compliance Summary (``misra_cpp2023_gcs.rst``,
LUNALINK-MISRA-GCS-001) — recording the status of all 179 guidelines
(Compliant, Deviated, Disapplied, Manual, or N/A), the deviation register,
and the disapplied guidelines justification — is maintained in the
development tree and is not shipped in this release.  The per-rule content
in this document is the shipped compliance record.

Tool Validation Suite
~~~~~~~~~~~~~~~~~~~~~
Tool confidence is demonstrated by a **tool validation suite** of 47
deliberately-broken C++ source files in ``cpp/tests/tool_validation/violations/``.
Each file contains exactly one coding violation and declares the expected
clang-tidy diagnostic. The runner script (``scripts/validate_tidy.py``,
invoked via ``task validate-tidy``) proves that the tool detects every
claimed violation.

This is analogous to DO-330 TQL-5 tool qualification by demonstration and
satisfies ECSS-Q-ST-80C §5.5 tool confidence requirements. Evidence reports
are generated at ``build/reports/tool_validation_report.rst`` (formal) and
``build/reports/tool_validation_report.json`` (machine-readable).

An equivalent suite validates the coverage tool (llvm-cov): 5 canary functions
with known coverage properties and 7 checks verifying line, branch, and MC/DC
measurement accuracy (``scripts/validate_coverage.py``, ``task validate-coverage``).

Known tool limitations are documented in ``docs/assurance/tool_confidence.rst``.

Style and API Patterns
----------------------
[NASA-GSFC §1-4; MISRA §4.10-4.11]

1. Prefer ``std::span`` for buffer parameters. [MISRA 7.11.2]
2. Use ``std::array`` for fixed-size buffers. [MISRA 11.3.1]
3. Use ``uint8_t`` or ``std::byte`` for raw data explicitly.
   [MISRA 6.9.2; JPL Rule 17]
4. Prefer ``const`` and ``constexpr`` for all immutable data.
   [MISRA 10.1.1; N7S-CG-001]
5. Prefer ``enum class`` over plain enums. [MISRA 10.2.2]
6. Public APIs must validate inputs and sizes before work starts.
   [JPL Rule 15; P10 Rule 7]
7. Any unsafe cast must be explicitly annotated and justified.
   [MISRA 8.2.2-8.2.8]
8. Use ``override`` on all virtual function overrides.
   [MISRA 13.3.1; N7S-CG-001]
9. Use RAII for resource management.
   [N7S-CG-001 §raii]

Pitfalls to Avoid
-----------------
1. Unbounded loops or data-dependent termination. [P10 Rule 2]
2. Implicit heap allocation via STL containers in flight paths.
   [MISRA 21.6.1; N7S-CG-001]
3. Function-local static initialization in safety-critical paths. [MISRA 6.7.1]
4. Missing ``[[nodiscard]]`` on status-returning APIs. [MISRA 0.1.2]
5. Using forbidden compiler flags.
6. Skipping alias checks or buffer overlap validation.
7. Multi-return DSP functions that could bypass scrubbing.
8. Silent failure states without telemetry or safe-mode escalation.
9. Implicit integer promotions on ``uint8_t`` operations. [MISRA 7.0.5]
10. Using deprecated C++ features. [MISRA 4.1.2]

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

This template demonstrates: ``[[nodiscard]]`` return [MISRA 0.1.2],
``noexcept`` [MISRA 18.4.1], single exit [LunaLink DSP policy],
input validation [JPL Rule 15], bounded loop [P10 Rule 2],
CFI counters, critical sections, WIP signaling, and secure scrubbing.
