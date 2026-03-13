Software Development Plan
==========================

**Document ID**: LUNALINK-SDP-001
**Specification Reference**: NPR 7150.2D SWE-013
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

This Software Development Plan (SDP) defines the lifecycle model, roles and
responsibilities, development tools, and entry/exit criteria applied to LunaLink.
It formalises the practices described informally in ``developing.rst`` to satisfy
NPR 7150.2D SWE-013.

Lifecycle Model
---------------

LunaLink uses an **incremental delivery** lifecycle structured around the competition's
eight-gateway milestone sequence.  Each gateway constitutes a formal increment:

.. list-table::
   :header-rows: 1
   :widths: 10 25 30 35

   * - Phase
     - Entry Criterion
     - Exit Criterion
     - Primary Deliverables
   * - Requirements
     - LSIS V1.0 specification received; gateway decomposition agreed
     - Compliance matrix baselined; SRS entries drafted for gateway scope
     - ``compliance_matrix.rst``, Software Safety Plan (initial)
   * - Design
     - Requirements phase exit criterion met
     - Architecture documented; FMEA complete for in-scope functions;
       interface contracts defined
     - ``signal/architecture.rst``, ``fmea.rst``, ``signal/sisicd.rst``
   * - Implementation
     - Design phase exit criterion met
     - All gateway functions implemented; pre-commit hook passes (full CI locally)
     - Source code in ``cpp/signal/``; Python bindings; tests
   * - Verification
     - Implementation phase exit criterion met
     - All CI workflows green; SWIFI 100%; MC/DC 100%; zero static-analysis
       violations; WCET within tolerance
     - CI artifacts: ``coverage.rst``, ``fault_coverage.rst``,
       ``static_analysis.rst``, ``wcet_profile.rst``
   * - Baseline & Release
     - Verification phase exit criterion met
     - Git tag signed; Software Version Description produced; golden test vectors
       committed
     - Git tag; SVD; ``tests/vectors/vX.Y/``

Roles and Responsibilities
---------------------------

In the competition context, a single Lead Developer performs all roles.  The role
definitions below reflect the responsibilities that would be distributed across a
team in a flight programme.

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Role
     - Responsibilities
   * - **Lead Developer**
     - System design; safety analysis (FMEA); coding standards enforcement;
       pull request approval; release tagging; configuration management authority
   * - **Developer**
     - Feature implementation; unit and integration tests; SWIFI tests for new
       hardening patterns; documentation
   * - **IV&V Engineer** *(flight programme only)*
     - Independent verification of CI artifacts; review of FMEA and Safety Plan;
       sign-off on baseline establishment
   * - **CCB Chair** *(flight programme only)*
     - Approval of changes to formal baselines; waiver authority for process
       deviations

Development Tools
-----------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Tool
     - Version
     - Purpose
   * - GCC / Clang
     - Version logged to CI artifact (``toolchain.lock`` at flight adoption)
     - C++20 compilation; ``-fno-exceptions -fno-rtti -fno-fast-math`` flight flags
   * - CMake + Ninja
     - ≥ 3.15; CI runner pinned
     - C++ build system
   * - scikit-build-core
     - Pinned in ``uv.lock``
     - Python extension build backend
   * - uv
     - Pinned in CI runner
     - Python environment and dependency management
   * - Task (Taskfile)
     - Current release
     - Task runner; single source of truth for all common commands
   * - Catch2
     - SHA-pinned in ``CMakeLists.txt``
     - C++ unit and SWIFI test framework
   * - pytest
     - Pinned in ``uv.lock``
     - Python integration tests
   * - clang-tidy
     - CI runner pinned
     - MISRA-aligned HICPP static analysis
   * - cppcheck
     - CI runner pinned
     - Secondary safety-critical static analyser
   * - LLVM coverage (``llvm-cov``)
     - CI runner pinned
     - Source-based MC/DC coverage measurement
   * - ASan / UBSan
     - Compiler-bundled
     - Runtime memory safety and undefined behaviour detection
   * - gcov / gcovr
     - CI runner pinned
     - C++ line coverage reporting
   * - Sphinx
     - Pinned in ``uv.lock``
     - Documentation generation

Coding Standards
-----------------

The coding standards and forbidden constructs are defined in full in
``mission_assurance.rst`` §Design Constraints.  This section summarises the
normative requirements; the detailed rationale is in that document.

**Language**: C++20, compiled with ``-std=c++20``.

**Mandatory rules**:

1. Zero dynamic allocation after initialisation (no ``malloc`` / ``new``).
2. No recursion.
3. No exceptions (``-fno-exceptions``).
4. No RTTI (``-fno-rtti``).
5. No floating-point fast-math (``-fno-fast-math``).
6. All safety-critical functions tagged ``[LSIS-AFS-*]`` and branchless by mandate.
7. All ``kFaultDetected`` return paths annotated with their FIZ level (``// FIZ-1``).
8. Every new hardening pattern must have a corresponding SWIFI test case before the
   pattern is considered verified.

**Static analysis gates** (blocking CI):

*   Zero Clang-Tidy (HICPP) violations in ``cpp/signal/``.
*   Zero Cppcheck errors or warnings at ``--enable=all``.
*   No forbidden compiler flags (asserted by ``scripts/check_build_flags.py``).

**Coverage gates** (blocking CI):

*   100% MC/DC on all ``[LSIS-AFS-*]``-tagged functions.
*   ≥ 90% C++ line coverage.
*   100% Python test coverage.
*   WCET profile within ±2% tolerance for all tagged functions.
*   100% SWIFI single-bit fault detection coverage.

Entry/Exit Criteria Summary
----------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 40 45

   * - Transition
     - Entry Criterion
     - Exit Criterion (gates that must be green)
   * - Requirements → Design
     - LSIS spec received; gateway scope agreed
     - Compliance matrix entries for gateway scope drafted and reviewed
   * - Design → Implementation
     - FMEA draft complete for in-scope functions
     - Architecture document updated; interface contracts defined; no open
       design-blocking issues
   * - Implementation → Verification
     - All gateway functions implemented; local ``task ci-checks`` passes
     - Pre-commit hook passes on all commits in branch (full CI locally)
   * - Verification → Baseline
     - All CI workflows green on ``main``
     - SWIFI 100%; MC/DC 100%; zero static-analysis violations; WCET in tolerance;
       golden vectors committed and passing
   * - Baseline → Release
     - Verification exit criteria met
     - Signed git tag created; SVD produced; binary MD5 recorded

Pre-commit Hook
----------------

The pre-commit hook (``.githooks/pre-commit``) delegates to ``task ci-checks``, which
runs the full quality pipeline in order:

1. ``build`` — compile C++ extension
2. ``lint-check`` — ruff format + ruff check + pyright
3. ``test-cpp`` — Catch2 tests via CTest
4. ``coverage-cpp`` — gcov line coverage
5. ``tidy`` — clang-tidy static analysis
6. ``test-py`` — pytest with coverage
7. ``sanitize`` — ASan + UBSan recompile and test
8. ``docs-build`` — Sphinx documentation build

Any step failure blocks the commit.  Bypassing with ``--no-verify`` is strongly
discouraged and must be documented in the commit message if used.

CI Pipeline
-----------

GitHub Actions workflows run on every push to ``main``, on every ``v*`` tag, and on
all pull requests.  Workflows run in parallel for fast feedback:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Workflow
     - Gate
   * - ``lint``
     - ruff + pyright; zero violations required
   * - ``test-py``
     - pytest on Python 3.12, 3.13, 3.14; 100% coverage required
   * - ``test-cpp``
     - Catch2 on GCC and Clang; all tests must pass
   * - ``sanitize``
     - ASan + UBSan; zero errors required
   * - ``clang-tidy``
     - Zero HICPP violations in ``cpp/signal/``
   * - ``coverage-cpp``
     - gcov line coverage; ≥ 90% required
   * - ``docs``
     - Sphinx build; zero warnings required (``-W --keep-going``)

All CI artifacts (coverage, WCET profile, static analysis, SWIFI coverage) are
published to ``gh-pages`` on every passing build against ``main``.

Deviation and Waiver Process
-----------------------------

A **Deviation** is an approved departure from a requirement before implementation.
A **Waiver** is approval to accept a known non-conformance after implementation.

Both require:

1. A GitHub Issue documenting: the non-conformance; the safety impact assessment
   (cross-reference to ``fmea.rst``); the proposed compensating control; the approver.
2. Review against the Software Safety Plan acceptance criteria.
3. Lead Developer approval (CCB approval for flight programme context).
4. A note in the Software Version Description for the affected release.

No deviation or waiver may lower the SWIFI fault detection coverage below 100% or
lower the MC/DC coverage below 100% for any ``[LSIS-AFS-*]``-tagged function without
a full safety case justification.

Approval Record
---------------

This document is approved for the LunaLink competition submission baseline.

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - Role
     - Name
     - Signature / Identifier
     - Date
   * - Author (Lead Developer)
     - LunaLink Development Team
     - Git commit on branch ``assurance`` (see SCI header ``kSciHeader.git_sha``)
     - 2026-03-12
   * - Technical Reviewer
     - Competition Assessment Panel
     - Submission artefact review
     - TBD (at submission)
   * - Programme Authority
     - Self-certified (Digital Twin phase)
     - See ``governance/baseline_register.rst`` BL-002
     - 2026-03-12

.. note::
   In a full flight programme, this section would carry wet or digital signatures from
   the Programme Manager, Chief Engineer, and Software Safety Officer.  For the
   competition Digital Twin phase, self-certification is used per ``certification_delta.rst``
   §Organisational Gaps Acknowledged.
