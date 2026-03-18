Toolchain Confidence and Evidence
=================================

Purpose
-------
Record the toolchain and analysis tools used to generate certification-grade
artifacts, along with the rationale for trust in their outputs.

Scope
-----
Applies to all flight build and verification tools used to produce evidence
artifacts (static analysis, coverage, SWIFI, and release builds).

Tool Records
------------

.. list-table:: Tool Confidence Record
   :header-rows: 1
   :widths: 18 12 25 20 25

   * - Tool
     - Version
     - Invocation
     - Evidence Role
     - Trust Justification
   * - Compiler — Clang (CI)
     - LLVM 19 (apt.llvm.org)
     - ``clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror
       -fno-exceptions -fno-rtti -fno-fast-math``
     - Flight binary build; all CI jobs
     - Mainstream LLVM release; version pinned in CI via
       ``LLVM_VERSION: "19"``; reproducible apt cache
   * - Compiler — Clang (local dev)
     - LLVM ≥ 20 (Homebrew)
     - Same flags as CI; ``brew install llvm``
     - Local build and coverage
     - Consistent flag set; tidy CI enforces final gate
   * - clang-tidy
     - LLVM 19 (CI) / Homebrew (local)
     - ``.clang-tidy`` in repo root; ``-warnings-as-errors=*``
     - Static analysis report
     - MISRA-aligned ruleset; config pinned in ``.clang-tidy``
   * - cppcheck
     - System-installed on ubuntu-latest (≥ 2.13)
     - ``--enable=all --inconclusive --error-exitcode=1``
     - Secondary static analysis
     - Conservative rules; complements clang-tidy
   * - LLVM Coverage (llvm-cov)
     - LLVM 19 (CI) / Homebrew (local)
     - ``-fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc``;
       report via ``llvm-cov report``
     - MC/DC and line coverage evidence
     - Toolchain-integrated instrumentation; no separate calibration
       required for source-based coverage
   * - Catch2
     - 3.4.0 (pinned SHA ``f90e8f50`` in CMakeLists.txt)
     - ``ctest --output-on-failure``
     - Test execution; all functional and SWIFI evidence
     - Pinned supply-chain SHA; well-known, widely-deployed framework
   * - CMake
     - ≥ 3.15 (per ``cmake_minimum_required``)
     - ``cmake -B build -DENABLE_LLVM_COVERAGE=ON ...``
     - Build system; invokes all tools
     - Industry standard; version range pinned in CMakeLists.txt

Release Baseline Requirement
----------------------------
For each release build, the tool versions and exact invocations must be
captured in the release record and retained alongside evidence artifacts.
The ``build_manifest.py`` script captures compiler version, git SHA, and
build flags at release time.
