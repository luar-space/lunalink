Coverage Exclusion Justification
=================================

:Document ID: LUNALINK-COV-EXCL-001
:Status: Active
:Last Review: 2026-09-01 (v1.0-submission baseline)
:Measured against: ``docs/assurance/evidence/coverage_summary.json``
   (git ``2381c58``), rendered as :doc:`coverage_evidence`

This document justifies every file carrying coverage exclusions, and in
particular every file whose line coverage is below 80%.

The 80% per-file figure is an internal review trigger, not a CI gate: the
CI-enforced thresholds are whole-library floors defined in
``.github/workflows/coverage-cpp.yml`` and stated in ``metrics_plan.rst``
(≥ 90% region coverage, ≥ 85% line coverage, ≥ 30% MC/DC).  A file below
the trigger produces a **waiver**, recorded here, and the coverage evidence
report renders the overall verdict as ``PASS_WITH_WAIVERS`` rather than
``PASS``.  Each exclusion is categorised and must be reviewed at every
milestone.

**Percentages in this register are transcribed from the generated coverage
evidence at the revision named above.**  When coverage is regenerated
(``task coverage-report``), re-check this register against
:doc:`coverage_evidence`; the review at each milestone exists to catch the
two drifting apart.

Effective coverage
------------------

Line coverage alone cannot distinguish "defence-in-depth path that only fires
under a bit flip" from "code nobody tested".  ``scripts/effective_coverage.py``
makes that distinction enforceable: it measures coverage of *coverable* lines
only, and **fails if any uncovered line is not inside a NOCOVER region**.  An
unmarked uncovered line is an error, not a lower percentage.

Every file in ``cpp/codes/`` — the spreading-code generators — is at
**100.0% effective coverage with zero unmarked uncovered lines**:

.. list-table::
   :header-rows: 1
   :widths: 34 22 22 22

   * - File
     - Raw line coverage
     - NOCOVER lines
     - Effective coverage
   * - ``codes/gold.cpp``
     - 88.24%
     - 6
     - 100.0%
   * - ``codes/legendre.cpp``
     - 75.00%
     - 24
     - 100.0%
   * - ``codes/matched_code.cpp``
     - 88.31%
     - 9
     - 100.0%
   * - ``codes/prn_codes.cpp``
     - 100.00%
     - 0
     - 100.0%
   * - ``codes/weil.cpp``
     - 75.51%
     - 24
     - 100.0%

This is the metric that answers the Gateway 1 success criterion *"100% test
coverage for code generators"*: every line a test can reach is reached, and
every line it cannot is categorised below.

.. code-block:: bash

   task coverage-effective

**Repository-wide the same measure is 98.24%** (2510/2555 coverable lines),
with 45 uncovered lines across 13 files not yet carrying a marker.  Those are
a known gap, not a claim of completeness: they are a mix of marker placement
lost to line-wrapping, platform-specific fallbacks (the Apple vDSP null-setup
path in ``fft.hpp``), and small helpers that genuinely want a test
(``spans_overlap`` null/zero-size guards in ``safety.hpp``).  Each needs an
individual disposition — marking an untested line ``NOCOVER`` to reach a
number would defeat the purpose of the metric — so ``coverage-effective`` is
**not** currently a blocking gate in ``task ci-checks``.  Closing those 45
lines and promoting it to a gate is tracked as post-submission work
(:doc:`../roadmap`).

Categories
----------

- **DEFENCE_IN_DEPTH** — hardening code that only fires under radiation-induced
  bit flips (SEU/SET). The nominal code path is covered by functional tests;
  the fault-detection path is covered by SWIFI at the pattern level. Specific
  bit-flip combinations required to reach these lines cannot be deterministically
  reproduced in a host-target test environment.

- **HAL_STUB** — hardware abstraction layer stub. The function signature exists
  to support embedded-target integration but the implementation
  is a no-op or trivial on the host. Will be covered by hardware-in-the-loop
  (HIL) testing at TRL-5.

- **DEAD_BY_DESIGN** — defensive unreachable code retained per MISRA Rule 16.4
  (default case in exhaustive switch) or MISRA Rule 0.0.2 (unreachable return
  after exhaustive if/else). Removing these lines would violate MISRA; covering
  them would require corrupting enum values.

- **PLATFORM_SPECIFIC** — code path only reachable on target architecture
  (e.g., big-endian byte swap, R5F interrupt handling). Covered by
  cross-architecture CI (s390x for big-endian).

----

Active waivers — files below the 80% review trigger
---------------------------------------------------

These three files drive the ``PASS_WITH_WAIVERS`` verdict in
:doc:`coverage_evidence`.

.. list-table::
   :header-rows: 1
   :widths: 30 12 15 43

   * - File
     - Line Coverage
     - Category
     - Justification
   * - ``codes/legendre.cpp``
     - 75.00%
     - DEFENCE_IN_DEPTH
     - CRC mismatch paths on Legendre cache initialisation. These fire only
       if the cached sequence is corrupted between generation and verification.
       SWIFI test ``FI-15`` covers the Legendre golden CRC path.  The
       generator itself is fully exercised: every Legendre-derived Weil code
       is compared chip-by-chip against Annex 3 for all 210 PRNs
       (``tests/test_annex3_codes.py``), so the uncovered remainder is the
       corruption-detection scaffolding, not the mathematics.  **Effective
       coverage (coverable lines only) is 100.0%** — see below.
   * - ``platform/signal_chain_fsm.cpp``
     - 64.52%
     - DEFENCE_IN_DEPTH
     - The ``consecutive_fault_count_ >= 2`` escalation path (lines 72-85)
       is architecturally unreachable: the first ``on_frame_fault()`` moves
       the FSM to ``kFrameFault``, and the second call is caught by the
       ``state_ != kOperational`` precondition guard before the counter check.
       The guard provides the safe-side failure (immediate kSafeMode lock).
       The ``transition()`` illegal path and ``on_frame_fault()`` from
       non-operational states are covered by functional tests with
       ``CaptureTelemetryHal`` telemetry verification. Lines 89-95
       (transition matrix defence check within ``on_frame_fault()``) are
       unreachable because kOperational → kFrameFault is always legal.
   * - ``codes/weil.cpp``
     - 75.51%
     - DEFENCE_IN_DEPTH
     - CFI terminal count and expansion-insert fault paths. Covered at
       pattern level by SWIFI.  As with ``legendre.cpp``, the code-generation
       path proper is validated bit-exactly against Annex 3 for all 210 PRNs
       and, independently, against LANS-AFS-SIM (:doc:`../interop_results`
       L1).

----

Recorded exclusions above the review trigger
--------------------------------------------

These files carry ``NOCOVER`` annotations for the reasons below but are
currently **above** the 80% per-file trigger, so they raise no waiver.  They
are retained here as the rationale record for the annotated lines.

.. list-table::
   :header-rows: 1
   :widths: 30 12 15 43

   * - File
     - Line Coverage
     - Category
     - Justification
   * - ``encoding/crc.cpp``
     - 81.36%
     - DEFENCE_IN_DEPTH
     - CFI terminal count failure paths in CRC-24 accumulation loop. Requires
       loop counter corruption (SEU on stack).
   * - ``encoding/ldpc.cpp``
     - 88.71%
     - DEFENCE_IN_DEPTH
     - Sparse matrix bounds checks (``check_single_row_parity``,
       ``multiply_sparse_hardened``, ``sparse_row_dot_product``) require
       corrupted row_ptr/col_indices arrays (radiation effect on .rodata).
       CFI sentinel and TMR mirror violation paths require injected
       control-flow faults — exercised by the SWIFI build, which is
       excluded from the coverage build.
   * - ``messages/frame_decode.cpp``
     - 91.35%
     - DEFENCE_IN_DEPTH
     - Full RX pipeline covered by round-trip test (frame_build → bipolar
       → frame_decode → data comparison). Remaining uncovered lines are
       kFaultDetected return paths in each pipeline stage (aliasing, BCH
       fault, deinterleave fault, LDPC fault, strip_crc_copy bounds, CFI
       terminal check) — all require corruption of internal state or
       matrix data. The kBchFailed path (line 87) is covered by a
       dedicated NaN-LLR test (``bch_decode_soft`` returns ``kInvalidInput``
       on non-finite inputs). The kCrcFailed path (line 195)
       requires LDPC to converge to a wrong codeword — an undetected error
       that CRC-24 is specifically designed to catch.
   * - ``messages/frame.cpp``
     - 93.13%
     - DEFENCE_IN_DEPTH
     - Aliasing checks, CFI violation paths, and ``default:`` in BCH status
       switch (retained per MISRA Rule 16.4). ``encode_bch_header()`` error
       paths through ``frame_build()`` are covered by FID/TOI validation
       tests. Remaining lines require interleaver fault (aliasing), CRC
       internal failure, or LDPC internal failure — all radiation effects.
   * - ``core/sise.cpp``
     - 94.12%
     - DEFENCE_IN_DEPTH
     - 4 NOCOVER markers: (1) RSS post-condition overflow check — requires
       component differences near ``DBL_MAX`` (~1e308); cislunar coordinates
       bounded to ~5×10⁸ m, making overflow impossible in practice (2 lines).
       (2) CFI sentinel failure paths in ``compute_sise()`` and
       ``compute_sise_percentile()`` — volatile counter always matches under
       nominal execution (2 lines).  FMEA: FM-073, FM-074.
   * - ``platform/watchdog.cpp``
     - 100.00%
     - HAL_STUB
     - ``install_watchdog()`` and ``get_installed_watchdog()`` are
       integration-time APIs. ``lock_watchdog_config()`` is a one-way latch
       called by POST; once locked, install is rejected.  All three are now
       reached by functional tests; the remaining HAL_STUB designation
       records that their *target* behaviour is verified by HIL on
       PolarFire SoC, not by the host build.
   * - ``include/lunalink/platform/watchdog.hpp``
     - 100.00%
     - HAL_STUB
     - ``WindowedWatchdog`` inline methods (tick, reset, evaluate,
       tick_count, upper_bound_exceeded) are integration-time APIs also
       exercised by SWIFI and HIL tests.

----

Not measured in the functional coverage build
---------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 30 12 15 43

   * - File
     - Line Coverage
     - Category
     - Justification
   * - ``platform/post.cpp``
     - not measured
     - DEFENCE_IN_DEPTH
     - Power-On Self Test.  ``post.cpp`` carries **no coverage mapping in the
       functional test binary**: the only tests that call ``run_post()`` live
       in ``cpp/tests/platform/test_swifi_platform.cpp``, which is built into
       the SWIFI binary, and that build is excluded from the coverage build
       (its injection hooks would distort structural coverage).  The object
       is therefore not linked into ``test_lunalink`` and does not appear in
       :doc:`coverage_evidence`.

       POST verification is by SWIFI instead: ``run_post()`` on a nominal
       system returns ``kPass``, and ``post_step2_lut_corrupt`` drives the
       LDPC-table corruption path to ``{kFaultDetected, 2}``.  Steps 1, 3, 4
       and 5 failure paths require corrupting the SCI header CRC, the TMR
       mechanism, ``.rodata`` constants or the CfiGuard — all radiation
       effects, all carrying NOCOVER markers.

       Run ``task swifi`` for the POST evidence; results are recorded in
       ``docs/assurance/evidence/swifi_results.json`` (LUNALINK-SWIFI-001)
       and traced in :doc:`fault_coverage`.
