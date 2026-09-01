Software Metrics Plan
======================

**Document ID**: LUNALINK-METRICS-001
**Specification Reference**: ECSS-Q-HB-80-04A; NPR 7150.2D SWE-164 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

The quantitative measures collected during LunaLink development: what each
one is, which tool collects it, the threshold that constitutes compliance, and
what happens when one is breached.

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
       achieved on safety-tagged (``[LSIS-AFS-*]``) decision points
     - LLVM ``llvm-cov`` (source-based instrumentation)
     - Every pull request; every push to ``main``
     - Design target: **100%** on safety-tagged decision points.  The CI gate
       currently enforces a **30%** whole-library floor
       (``MCDC_COVERAGE_MIN`` in ``.github/workflows/coverage-cpp.yml``); the
       measured value at submission is recorded in the coverage evidence and
       badge.
     - CI gate blocks merge below the enforced floor; author notified by
       GitHub check failure; 48 hours to resolve
   * - **Region / Line Coverage (C++)** — percentage of C++ code regions and
       source lines executed by the Catch2 test suite
     - LLVM ``llvm-cov`` (source-based instrumentation)
     - Every pull request; every push to ``main``
     - **≥ 90%** region and **≥ 85%** line, whole-library
       (``REGION_COVERAGE_MIN`` / ``LINE_COVERAGE_MIN`` in
       ``.github/workflows/coverage-cpp.yml``, mirroring the ``THRESHOLDS``
       table in ``scripts/coverage_evidence_report.py``).  Region is the
       ECSS Category B primary structural metric; line is a floor beneath
       it.  No per-file floor is CI-enforced; files below 80% line coverage
       are individually justified in ``coverage_exclusions.rst``.  Measured
       values for this release: :doc:`coverage_evidence`.
     - CI gate blocks merge; author notified; 48 hours to add tests or
       justify exclusion in ``coverage_exclusions.rst``
   * - **SWIFI Detection Rate** — fraction of single-bit fault injections
       that are detected and returned as ``kFaultDetected``
     - LunaLink SWIFI test suite (``cpp/tests/<domain>/test_swifi_*.cpp``)
     - Every pull request; every push to ``main``
     - **100%** — any undetected fault is a safety regression
     - CI gate blocks merge immediately; author must investigate within
       24 hours; escalated to S1 PRACA if root cause is not clear
   * - **clang-tidy violations** — count of HICPP-ruleset violations in
       ``cpp/{codes,encoding,messages,signal,platform}/``
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

Any metric crossing its threshold triggers the same five steps:

1. **CI gate fires** — the relevant GitHub Actions job transitions to
   ``failure``.  The pull request is blocked from merging by the branch
   protection rule.  GitHub automatically requests a re-review from the
   author.

2. **Author notified** — the author receives a GitHub notification.  The
   author is responsible for diagnosing the root cause within **48 hours**
   of the CI gate firing.

3. **48-hour resolution window** — the author must either:

   a. Fix the metric violation and push new commits to the PR, or
   b. Open a PRACA issue classifying the violation and proposing a
      compensating control or threshold waiver.  (The PRACA process record,
      ``praca.rst``, is maintained in the development tree and is not shipped
      in this release; GitHub Issues serve as the visible problem-report
      channel.)

4. **Branch blocked** — if neither (a) nor (b) is completed within 48 hours,
   the branch is marked ``blocked`` in the GitHub issue tracker and no
   further merges from that branch are permitted until resolution.

5. **S1 escalation** — SWIFI detection rate failures and any MC/DC
   regression below the enforced CI floor are immediately classified as
   **S1 (Critical)**, bypassing the 48-hour window and requiring immediate
   Lead Developer involvement.

Metrics History
----------------

A per-commit metrics history table in this document was planned and **was
not built**; the capability it was meant to provide is delivered by the
published badges instead, and this section records that decision rather
than leaving an empty placeholder.

Each metric below is measured on every push to ``main`` and published as a
live badge on the documentation landing page and the repository README,
sourced from the workflow named:

.. list-table::
   :header-rows: 1
   :widths: 26 30 44

   * - Metric
     - Workflow
     - Where the current value is published
   * - Line / region / branch / MC/DC coverage
     - ``coverage-cpp.yml``
     - Four badges on :doc:`../index`; full report in
       :doc:`coverage_evidence`; llvm-cov HTML browser at ``/coverage/``
       on the docs site
   * - Python line coverage
     - ``test-python.yml``
     - Badge on :doc:`../index` (100%, CI-gated)
   * - SWIFI detection rate
     - ``task swifi``
     - ``docs/assurance/evidence/swifi_results.json``;
       :doc:`fault_coverage`
   * - clang-tidy violations
     - ``tidy.yml``
     - Badge on :doc:`../index`; zero violations is a blocking gate
   * - cppcheck violations
     - ``lint.yml``
     - ``cppcheck_report.rst``, published to the docs site

Each badge reflects the current ``main``; the badge endpoints are gist-backed
(``COVERAGE_GIST_ID``), so their revision history is the metrics history.
For a point-in-time record against any git ref, ``task recertify`` runs every
gate and emits a signed-off, milestone-tagged recertification record —
which is the artefact this section originally intended to accumulate.

.. code-block:: bash

   task metrics      # print the current metric set locally
   task recertify    # full gate re-run + signed-off record for a given ref
