Software Configuration Management Plan
=======================================

**Document ID**: LUNALINK-SCMP-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.4; NPR 7150.2D SWE-074, SWE-077, SWE-079 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose and Scope
-----------------

This Software Configuration Management Plan (SCMP) defines the processes, tools, and
authority structure used to identify, control, and audit LunaLink software
configurations throughout the development lifecycle.

Scope: all software items under change control, including source code, build scripts,
toolchain manifests, test data (golden vectors), and documentation.

Configuration Items
--------------------

A **Software Configuration Item (SCI)** is any work product that is placed under
change control.  LunaLink SCIs are listed below.

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - SCI Name
     - Location
     - Description
   * - Signal core source
     - ``cpp/{codes,encoding,messages,signal,platform}/``, ``cpp/include/``
     - Safety-critical C++ DSP functions; primary flight software deliverable
   * - Python bindings
     - ``cpp/bindings/``, ``src/lunalink/``
     - nanobind layer and Python API wrapper; Digital Twin deliverable
   * - Test suite
     - ``cpp/tests/``, ``tests/``
     - Catch2 and pytest tests; includes SWIFI fault-injection suite
   * - Golden test vectors
     - ``tests/vectors/``
     - Immutable, versioned JSON test vectors; normative reference for LSIS ecosystem
   * - Build system
     - ``CMakeLists.txt``, ``pyproject.toml``, ``Taskfile.yml``
     - Build definitions and task runner
   * - Toolchain manifest
     - CI build artifact log (``toolchain.lock`` scoped to flight programme adoption)
     - Compiler version logged on every CI run; formal manifest at flight adoption
   * - Dependency lockfile
     - ``uv.lock``
     - SHA-pinned resolution of all Python and Python-build dependencies
   * - CI pipeline
     - ``.github/workflows/``
     - All automated verification and deployment workflows
   * - Documentation
     - ``docs/``
     - Sphinx documentation source; includes all assurance and governance documents

Baseline Definitions
---------------------

A **Software Baseline** is a formally identified, change-controlled snapshot of one
or more SCIs.  Baselines are established at lifecycle milestones and are immutable
once established.

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Baseline Name
     - Git Tag Pattern
     - Description
   * - Development Baseline
     - ``dev/*``
     - Working snapshots; not formally controlled; used for internal progress
       tracking
   * - Functional Baseline (SRR)
     - ``v*-srr``
     - Established at System Requirements Review; captures approved requirements and
       architecture; changes require CCB approval
   * - Allocated Baseline (PDR)
     - ``v*-pdr``
     - Established at Preliminary Design Review; captures allocated software
       requirements and preliminary design
   * - Design Baseline (CDR)
     - ``v*-cdr``
     - Established at Critical Design Review; captures final design; code complete
       against this baseline
   * - Product Baseline (Flight)
     - ``v*`` (semver)
     - Established after successful System Acceptance Review; the binary from this
       baseline is the flight deliverable; no changes permitted without formal
       waiver

All formal baselines are tagged in git with a signed tag (``git tag -s``).  The tag
annotation records the baseline name, date, and the name of the approving authority.

Branch Policy
--------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Branch
     - Policy
   * - ``main``
     - **Protected**.  All merges require: passing CI (all workflows green);
       pull request review approval; no unresolved review comments.  Direct
       push is prohibited.
   * - ``feature/*``
     - Development branches.  Branched from ``main``; merged back via PR.
       CI must pass on the PR branch before merge is permitted.
   * - ``hotfix/*``
     - Emergency fix branches for post-baseline defects.  Require CCB approval
       before merge to ``main`` and re-tagging of the affected baseline.
   * - ``release/*``
     - Release preparation branches.  No new features; only bug fixes and
       documentation.  Merged to ``main`` and tagged at release.

Change Request and Approval Process
-------------------------------------

**For changes to the Development Baseline (pre-SRR)**:

1. Developer creates a ``feature/*`` or ``fix/*`` branch.
2. Developer opens a Pull Request targeting ``main``.
3. CI pipeline must pass (all workflows green).
4. At least one reviewer must approve the PR.
5. Merge is performed by the PR author or a designated integrator.

**For changes to a Formal Baseline (SRR, PDR, CDR, or Flight)**:

1. Developer raises a Problem Report via GitHub Issues with severity classification.
2. Change is reviewed for safety impact against ``fmea.rst``.
3. If safety-critical: CCB review and approval required before work begins.
4. A ``hotfix/*`` branch is created from the affected baseline tag.
5. Fix is implemented, verified, and subject to full CI (including SWIFI suite).
6. CCB approves the fix; a new tag is created (e.g., ``v1.0.1``).
7. A Software Version Description is generated documenting the change.

Note: In the current competition context, the CCB function is performed by the Lead
Developer.  A flight programme would establish a formal CCB with an authority matrix.

Merge Authority Matrix
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - Target
     - Approver
     - Requirements
   * - ``main`` (pre-SRR)
     - Any team member with repo write access
     - CI pass; PR review approval
   * - Formal baseline tag
     - Lead Developer
     - CI pass; PR approval; explicit tag sign-off
   * - Flight baseline tag
     - Programme Manager (flight programme context)
     - Full verification pass; IV&V sign-off; CCB approval

Reproducible Build and Binary Identity
----------------------------------------

Every merge to ``main`` produces a deterministic binary whose identity is recorded in
the embedded **SCI header** (``mission_assurance.rst`` §Supply Chain §3):

*   Git commit SHA-1
*   LSIS specification version
*   Build timestamp (UTC, seconds since J2000)
*   CRC32 of the preceding fields

The binary MD5 hash is recorded in the CI run log for every passing build.  A Physical
Configuration Audit may verify the flight binary by running ``task build --release``
from the flight baseline tag and comparing the resulting MD5 against the CI-recorded
value.

Configuration Audit
--------------------

**Functional Configuration Audit (FCA)**: Verifies that the software satisfies its
functional requirements.  Performed by:

*   Passing the Golden Test Vector Suite (all nominal + corner case vectors pass)
*   Passing the SWIFI fault-injection suite (100% single-bit fault detection)
*   Passing the MC/DC coverage check (100% on ``[LSIS-AFS-*]``-tagged functions)

**Physical Configuration Audit (PCA)**: Verifies that the flight binary matches the
baselined source.  Performed by:

1. Checking out the flight baseline tag from git.
2. Running ``task build --release`` on a clean environment with the pinned toolchain.
3. Comparing the MD5 of the resulting binary against the value recorded in the CI run
   that produced the baselined artifact.

Tools Under Configuration Control
-----------------------------------

The following tools are themselves under configuration control because their version
directly affects the binary produced:

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - Tool
     - Pinning Mechanism
     - Notes
   * - C++ compiler (GCC/Clang)
     - CI build artifact log; formal ``toolchain.lock`` at flight adoption
     - Any upgrade is a formal change-controlled event requiring full re-verification
   * - CMake
     - Minimum version in ``CMakeLists.txt``; CI runner image pinned
     - Build system changes require PR review
   * - uv (Python package manager)
     - Version pinned in CI runner image
     - ``uv.lock`` is the dependency control artifact
   * - clang-tidy / cppcheck
     - CI runner image version pinned
     - Analyser upgrades may change violation counts; require re-baselining

Version Numbering
-----------------

LunaLink uses Semantic Versioning (SemVer 2.0.0): ``MAJOR.MINOR.PATCH``.

*   ``MAJOR``: Incompatible changes to the LSIS signal interface or HAL API.
*   ``MINOR``: New functionality added in a backward-compatible manner.
*   ``PATCH``: Bug fixes and documentation updates; no interface changes.

Pre-release identifiers (e.g., ``1.0.0-rc.1``) may be used for release candidates.
All releases are tagged in git and produce a Software Version Description.
