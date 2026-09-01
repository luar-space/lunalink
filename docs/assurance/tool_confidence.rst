Toolchain Confidence and Evidence
=================================

Purpose
-------
The toolchain and analysis tools behind every certification-grade artefact,
and the grounds for trusting what they report.

Scope
-----
Every flight build and verification tool that produces an evidence artefact:
static analysis, coverage, SWIFI, release builds.

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
     - ``--std=c++20 --platform=unix64 --enable=all --inconclusive
       --error-exitcode=1``; config: ``cppcheck.cfg``,
       suppressions: ``cppcheck.suppressions``
     - Secondary static analysis (ECSS-Q-ST-80C §5.5)
     - Conservative rules; complements clang-tidy; suppressions
       documented with justification
   * - Tool Validation Suite (clang-tidy)
     - N/A (project-specific)
     - ``python3 scripts/validate_tidy.py`` (``task validate-tidy``)
     - Tool confidence evidence — proves clang-tidy detects
       claimed MISRA/JPL/P10 violations (DO-330 TQL-5 analogue)
     - 47 deliberately-broken source files in
       ``cpp/tests/tool_validation/violations/``, each containing
       one MISRA/JPL/P10 violation. Script verifies expected
       diagnostic fires. Reports: ``build/reports/tool_validation_report.rst``
       (formal evidence), ``build/reports/tool_validation_report.json``
       (machine-readable)
   * - Tool Validation Suite (llvm-cov)
     - N/A (project-specific)
     - ``python3 scripts/validate_coverage.py`` (``task validate-coverage``)
     - Tool confidence evidence — proves llvm-cov correctly reports
       covered/uncovered lines, branches, and MC/DC conditions
       (DO-330 TQL-5 analogue)
     - 5 canary functions in ``coverage_canary.hpp`` with known coverage
       properties; 7 checks verify line coverage, branch coverage,
       MC/DC full/partial, and uncovered function detection.
       Reports: ``build/reports/coverage_validation_report.rst``
       (formal evidence), ``build/reports/coverage_validation_report.json``
       (machine-readable)
   * - Clang Static Analyzer (scan-build)
     - LLVM 19 (CI) / Homebrew (local)
     - ``scan-build --status-bugs -enable-checker core,deadcode,security,
       unix,cplusplus,nullability cmake --build ...``
     - Path-sensitive static analysis (ECSS-Q-ST-80C §5.5)
     - Detects null dereferences through conditional paths, use-after-free,
       dead stores, and memory leaks that clang-tidy misses.
       CI gate: ``tidy.yml`` (``clang-sa`` job); local: ``task clang-sa``
   * - clang-format
     - LLVM 19 (CI) / Homebrew (local)
     - ``.clang-format`` in repo root; ``--dry-run --Werror``
     - Code formatting enforcement
     - LLVM-based style with flight-software adjustments (100-col,
       Attach braces, no bin-packing). CI gate: ``lint.yml``;
       local: ``task format-cpp-check``
   * - include-what-you-use (IWYU)
     - 0.25 (Homebrew / apt)
     - ``cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=...``
     - Header hygiene analysis (advisory)
     - Verifies minimal ``#include`` sets. Not a CI gate —
       advisory only due to false positives on platform-specific
       headers. Local: ``task iwyu``
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
   * - libFuzzer (fuzz harnesses)
     - LLVM 19 (CI, ``clang`` + ``libclang-rt-dev``) / Homebrew ≥ 20 (local)
     - ``-DENABLE_FUZZING=ON`` (Clang-only); ``task fuzz`` (search),
       ``task fuzz-replay`` (``-runs=0`` seed replay); harnesses in
       ``cpp/fuzz/``; built ``-O1`` with ASan + UBSan
     - Dynamic robustness verification (ECSS-Q-ST-80C §5.5) of the
       externally-fed parser/FEC entry points: nightly coverage-guided
       search (``fuzz.yml``) + PR-time regression gate (``fuzz-smoke.yml``)
     - Coverage-guided search with ASan/UBSan oracles; seed corpus
       generated from a decode-validated frame
       (``scripts/fuzz_seed_corpus.py``); non-flight test code, never linked
       into the flight binary; any finding in ``lunalink_core`` is triaged as
       a flight defect. Run details: ``cpp/fuzz/README.md``

Known Tool Limitations
----------------------
The following checks are claimed in the MISRA GCS but have known detection
gaps in clang-tidy (verified during tool validation suite development):

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Check
     - Limitation
   * - ``bugprone-unreachable-code``
     - Limited pattern coverage in clang-tidy ≤ 22. Simple unreachable
       statements after ``return`` are caught by the compiler's
       ``-Wunreachable-code-return`` instead. MISRA 0.0.1 compliance
       relies on compiler warnings, not the tidy check.
   * - ``bugprone-assert-side-effect``
     - Does not detect side effects in ``assert()`` when using standard
       ``<cassert>`` with clang-tidy ≤ 22. MISRA/JPL assertion discipline
       enforced by code review and the project's use of runtime checks
       (return bool) over ``assert()`` for flight APIs.

Three things cover the gap:

1. Compiler warnings covering the same violation class.
2. The project's coding conventions avoiding the problematic patterns entirely
   (no ``assert()`` in flight code; single-exit functions prevent unreachable paths).
3. Code review as a secondary enforcement mechanism.

Release Baseline Requirement
----------------------------
For each release build, the tool versions and exact invocations must be
captured in the release record and retained alongside evidence artifacts.
The ``build_manifest.py`` script captures compiler version, git SHA, and
build flags at release time.
