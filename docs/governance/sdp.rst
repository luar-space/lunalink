Software Development Plan
==========================

**Document ID**: LUNALINK-SDP-001
**Specification Reference**: ECSS-E-ST-40C Rev.1 §5.2; NPR 7150.2D SWE-013 (secondary)
**Revision**: 1.1
**Date**: 2026-10-18
**Status**: Released — v1.0.0 baseline

Purpose
-------

The lifecycle model, roles, development tools and entry/exit criteria that
govern LunaLink.  It formalises what ``developing.rst`` describes informally,
as ECSS-E-ST-40C Rev.1 §5.2 (NPR 7150.2D SWE-013) requires.

Document Set and Omissions
--------------------------

.. note::

   **No standalone Software Safety Plan was produced for this release.**

   The safety-planning role that ECSS-Q-ST-80C Rev.2 §5.3/§5.5 and
   NPR 7150.2D SWE-027 assign to such a document is discharged here by three
   others, as recorded in :doc:`/standards/nasa_compliance_addendum`:
   :doc:`/assurance/fmea` analyses what can fail and how each failure is
   detected and contained, :doc:`/assurance/fault_coverage` reconciles every
   failure mode against the test that exercises it, and
   :doc:`/assurance/mission_assurance` specifies the fault-containment
   architecture those two assume.

   Wherever this plan or the configuration baseline register refers to safety
   review or safety acceptance criteria, those three documents are the
   criteria.  A separate safety plan becomes necessary if LunaLink is ever
   taken into a flight programme with an independent safety authority, and is
   named as future work rather than quietly assumed.

   Three further documents named in this plan exist only in a flight-programme
   context and are not part of this release: a Software Version Description per
   baseline, a problem-reporting record, and the full MISRA Guideline
   Compliance Summary.  Each is marked where it is referenced, with the
   artefact that stands in its place.

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
     - ``compliance_matrix.rst``, ``signal/shall_inventory.rst``
   * - Design
     - Requirements phase exit criterion met
     - Architecture documented; FMEA complete for in-scope functions;
       interface contracts defined
     - ``signal/architecture.rst``, ``fmea.rst``, ``signal/sisicd.rst``
   * - Implementation
     - Design phase exit criterion met
     - All gateway functions implemented; pre-commit hook passes (full CI locally)
     - Source code in ``cpp/{codes,encoding,messages,signal,platform}/``; Python bindings; tests
   * - Verification
     - Implementation phase exit criterion met
     - All CI workflows green; SWIFI 100%; MC/DC at or above the enforced CI
       floor (design target 100% on safety-tagged decision points — see
       ``assurance/metrics_plan.rst``); zero static-analysis violations; WCET
       within tolerance
     - CI artifacts: ``coverage.rst``, ``fault_coverage.rst``,
       ``static_analysis.rst``, ``wcet_profile.rst``
   * - Baseline & Release
     - Verification phase exit criterion met
     - Git tag signed; Software Version Description produced; golden test vectors
       committed
     - Git tag; SVD; ``test-vectors/``

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
     - Independent verification of CI artifacts; review of the FMEA and the
       fault coverage report; sign-off on baseline establishment
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
     - Pinned in ``[build-system] requires`` (``>= 1.1.1``)
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
   * - Sphinx
     - Pinned in ``uv.lock``
     - Documentation generation

Coding Standards
-----------------

``mission_assurance.rst`` §Design Constraints defines the coding standards and
forbidden constructs in full.  What follows is the normative summary; the
rationale lives there.

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

*   Zero Clang-Tidy (HICPP) violations in the core source directories.
*   Zero Cppcheck errors or warnings at ``--enable=all``.
*   No forbidden compiler flags (asserted by ``scripts/check_build_flags.py``).

**Coverage gates** (blocking CI):

*   MC/DC: design target 100% on safety-tagged (``[LSIS-AFS-*]``) decision
    points; the CI gate currently enforces a 30% whole-library floor
    (``.github/workflows/coverage-cpp.yml``); the measured value at submission
    is recorded in the coverage evidence and badge.
*   ≥ 90% C++ line coverage (CI-enforced).
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
     - SWIFI 100%; MC/DC at or above the enforced CI floor (design target
       100% on safety-tagged decision points); zero static-analysis
       violations; WCET in tolerance; ``task conformance`` passes against
       the pinned shared test vectors
   * - Baseline → Release
     - Verification exit criteria met
     - Signed git tag created; SVD produced; binary MD5 recorded

Pre-commit Hook
----------------

The pre-commit hook (``.githooks/pre-commit``) delegates to ``task pre-commit``, which
runs the fast checks in order:

1. ``build`` — compile C++ extension
2. ``lint-check`` — pyright, cppcheck, docstring sync, ruff format + ruff check
3. ``tidy-staged`` — clang-tidy static analysis on the staged C++ files
4. ``test-cpp`` — Catch2 tests via CTest
5. ``test-py`` — pytest with coverage

Any step failure blocks the commit.  The full gate — LLVM coverage, the
sanitizers, whole-tree clang-tidy, fault injection, traceability and the
documentation build — is ``task ci-checks``, mirrored by the CI pipeline below.  ``--no-verify`` bypasses the hook; a
commit that uses it must say why in its message.

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
     - Zero HICPP violations in the core source directories
   * - ``coverage-cpp``
     - LLVM source-based coverage; ≥ 90% region, ≥ 85% line and ≥ 30% MC/DC required
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
2. Review against the fault-containment criteria in ``fmea.rst`` and
   ``fault_coverage.rst`` (see *Document Set and Omissions* above).
3. Lead Developer approval (CCB approval for flight programme context).
4. A note in the Software Version Description for the affected release.

No deviation or waiver may lower the SWIFI fault detection coverage below 100%,
lower the CI-enforced coverage floors (``.github/workflows/coverage-cpp.yml``), or
abandon the 100% MC/DC design target for ``[LSIS-AFS-*]``-tagged functions without
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
   competition Digital Twin phase, self-certification is used; the organisational
   roles this leaves unfilled are identified in §Roles and Responsibilities above
   and in the NASA Compliance Addendum §Organisational Scope Note
   (``docs/standards/nasa_compliance_addendum.rst``).
