Software Metrics Plan
======================

**Document ID**: LUNALINK-METRICS-001
**Specification Reference**: ECSS-Q-HB-80-04A; NPR 7150.2D SWE-164 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

This Software Metrics Plan defines the quantitative measures collected during
LunaLink development, the tools used to collect them, the thresholds that
constitute compliance, and the escalation procedure when a threshold is
breached.

It satisfies ECSS-Q-HB-80-04A (NPR 7150.2D SWE-164, "the project shall develop a
Software Metrics Plan that identifies software metrics to be used to manage the
software development effort").

The plan governs all CI-collected metrics that enforce quality gates on pull
requests merging into ``main`` and on every ``v*`` release tag.

Metrics Catalogue
------------------

.. list-table::
   :header-rows: 1
   :widths: 22 14 18 16 30

   * - Metric
     - Tool
     - Collection Frequency
     - Threshold
     - Escalation Action
   * - **MC/DC Coverage** — percentage of Modified Condition/Decision Coverage
       achieved on all ``[LSIS-AFS-*]``-tagged functions
     - LLVM ``llvm-cov`` (source-based instrumentation)
     - Every pull request; every push to ``main``
     - **100%** — no exceptions without a documented safety-case waiver
     - CI gate blocks merge; author notified by GitHub check failure;
       48 hours to resolve or raise waiver in PRACA
   * - **Line Coverage (C++)** — percentage of C++ source lines executed by
       the Catch2 test suite
     - ``gcov`` / ``gcovr``
     - Every pull request; every push to ``main``
     - **≥ 95%** overall; per-file floor of 90%
     - CI gate blocks merge; author notified; 48 hours to add tests or
       justify exclusion via ``// LCOV_EXCL`` annotation in code review
   * - **SWIFI Detection Rate** — fraction of single-bit fault injections
       that are detected and returned as ``kFaultDetected``
     - LunaLink SWIFI test suite (``tests/swifi/``)
     - Every pull request; every push to ``main``
     - **100%** — any undetected fault is a safety regression
     - CI gate blocks merge immediately; author must investigate within
       24 hours; escalated to S1 PRACA if root cause is not clear
   * - **clang-tidy violations** — count of HICPP-ruleset violations in
       ``cpp/signal/``
     - ``clang-tidy`` (HICPP profile, ``-checks=hicpp-*``)
     - Every pull request; every push to ``main``
     - **0** violations
     - CI gate blocks merge; each violation must be fixed or suppressed
       with documented justification in the PR description
   * - **cppcheck violations** — count of errors and warnings at
       ``--enable=all``
     - ``cppcheck``
     - Every pull request; every push to ``main``
     - **0** errors; **0** warnings
     - CI gate blocks merge; same escalation path as clang-tidy
   * - **Build size (flash)** — size of ``liblunalink.a`` text+rodata
       sections as reported by ``nm``
     - ``nm --print-size`` + ``scripts/check_build_size.py``
     - Every pull request; every push to ``main``
     - **≤ 64 KB** (text + rodata combined)
     - CI gate blocks merge; author must profile and reduce size or
       request threshold increase via Lead Developer review
   * - **WCET input-class spread** — intra-run spread ``(max − min) / min``
       across adversarial input classes (all-zeros, all-ones, pseudo-random)
       for ``[LSIS-AFS-*]``-tagged functions; detects data-dependent branches
       within a single build.  Inter-build baseline regression tracking is a
       Gateway 3 deliverable.
     - Catch2 benchmark suite parsed by ``wcet.yml`` CI step
     - Every pull request; every push to ``main``
     - **≤ 2%** spread across input classes (functions with a single timing
       sample emit ``WARN`` only; non-blocking until multi-class variants added)
     - CI gate blocks merge if spread > 2%; ``WARN`` emitted for single-sample
       functions (non-blocking); author must add multi-class variants by
       Gateway 3 exit or raise a PRACA waiver

Escalation Procedure
---------------------

The standard escalation procedure applies when any metric crosses its
threshold:

1. **CI gate fires** — the relevant GitHub Actions job transitions to
   ``failure``.  The pull request is blocked from merging by the branch
   protection rule.  GitHub automatically requests a re-review from the
   author.

2. **Author notified** — the author receives a GitHub notification.  The
   author is responsible for diagnosing the root cause within **48 hours**
   of the CI gate firing.

3. **48-hour resolution window** — the author must either:

   a. Fix the metric violation and push new commits to the PR, or
   b. Open a PRACA issue (``assurance/praca.rst`` §Problem Report Lifecycle)
      classifying the violation and proposing a compensating control or
      threshold waiver.

4. **Branch blocked** — if neither (a) nor (b) is completed within 48 hours,
   the branch is marked ``blocked`` in the GitHub issue tracker and no
   further merges from that branch are permitted until resolution.

5. **S1 escalation** — SWIFI detection rate failures and any MC/DC
   regression below 100% are immediately classified as **S1 (Critical)**
   in the PRACA system, bypassing the 48-hour window and requiring
   immediate Lead Developer involvement.

Metrics History
----------------

.. note::

   This section will be populated automatically by CI once the ``metrics``
   workflow step is activated (planned for Gateway 3 exit).  On every passing
   build against ``main``, the CI ``metrics`` job will append a row to
   ``docs/metrics_history.csv`` and publish a rendered HTML dashboard to
   ``gh-pages``.  The stub table below is a placeholder until that workflow
   step is active.

.. list-table::
   :header-rows: 1
   :widths: 16 14 14 14 14 14 14

   * - Build Date
     - MC/DC %
     - Line Cov %
     - SWIFI %
     - clang-tidy
     - cppcheck
     - Flash (KB)
   * - *(populated by CI)*
     - *(CI)*
     - *(CI)*
     - *(CI)*
     - *(CI)*
     - *(CI)*
     - *(CI)*
