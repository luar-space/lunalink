Coverage Exclusion Justification
=================================

:Document ID: LUNALINK-COV-EXCL-001
:Status: Active
:Last Review: 2026-03-21

This document justifies all source files with line coverage below the 80%
per-file threshold. Each exclusion is categorised and must be reviewed at
every milestone.

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

Exclusion Register
------------------

.. list-table::
   :header-rows: 1
   :widths: 30 12 15 43

   * - File
     - Line Coverage
     - Category
     - Justification
   * - ``platform/watchdog.hpp``
     - 0%
     - HAL_STUB
     - ``WindowedWatchdog`` inline methods (tick, reset, evaluate,
       tick_count, upper_bound_exceeded) are integration-time APIs exercised
       by SWIFI and HIL tests. ``install_watchdog()``, ``lock_watchdog_config()``,
       and ``get_installed_watchdog()`` are one-way latches called by POST.
   * - ``platform/watchdog.cpp``
     - ~23%
     - HAL_STUB
     - ``install_watchdog()`` and ``get_installed_watchdog()`` are
       integration-time APIs. ``lock_watchdog_config()`` is a one-way latch
       called by POST; once locked, install is rejected. The lock path is
       covered. Install/get will be covered by HIL on PolarFire SoC.
   * - ``platform/post.cpp``
     - ~71%
     - DEFENCE_IN_DEPTH
     - POST steps 1, 3, 4, 5 failure paths require corrupting SCI header CRC,
       TMR mechanism, .rodata constants, or CfiGuard — all of which are
       radiation effects. Step 2 (LDPC table corruption) is covered by SWIFI
       test ``post_step2_lut_corrupt``. NOCOVER markers added in PR #41.
   * - ``platform/signal_chain_fsm.cpp``
     - ~69%
     - DEFENCE_IN_DEPTH
     - The ``consecutive_fault_count_ >= 2`` escalation path (lines 72-85)
       is architecturally unreachable: the first ``on_frame_fault()`` moves
       the FSM to ``kFrameFault``, and the second call is caught by the
       ``state_ != kOperational`` precondition guard before the counter check.
       The guard provides the safe-side failure (immediate kSafeMode lock).
       The ``transition()`` illegal path and ``on_frame_fault()`` from
       non-operational states are now covered by functional tests with
       ``CaptureTelemetryHal`` telemetry verification. Lines 89-95
       (transition matrix defence check within ``on_frame_fault()``) are
       unreachable because kOperational → kFrameFault is always legal.
   * - ``codes/legendre.cpp``
     - ~72%
     - DEFENCE_IN_DEPTH
     - CRC mismatch paths on Legendre cache initialisation. These fire only
       if the cached sequence is corrupted between generation and verification.
       SWIFI test ``FI-15`` covers the Legendre golden CRC path.
   * - ``encoding/crc.cpp``
     - ~81%
     - DEFENCE_IN_DEPTH
     - CFI terminal count failure paths in CRC-24 accumulation loop. Requires
       loop counter corruption (SEU on stack).
   * - ``codes/weil.cpp``
     - ~82%
     - DEFENCE_IN_DEPTH
     - CFI terminal count and expansion-insert fault paths. Covered at
       pattern level by SWIFI.
   * - ``messages/frame_decode.cpp``
     - ~85%
     - DEFENCE_IN_DEPTH
     - Full RX pipeline now covered by round-trip test (frame_build → bipolar
       → frame_decode → data comparison). Remaining uncovered lines are
       kFaultDetected return paths in each pipeline stage (aliasing, BCH
       fault, deinterleave fault, LDPC fault, strip_crc_copy bounds, CFI
       terminal check) — all require corruption of internal state or
       matrix data. The kBchFailed path (line 87) is now covered by a
       dedicated NaN-LLR test (``bch_decode_soft`` returns ``kInvalidInput``
       on non-finite inputs). The kCrcFailed path (line 195)
       requires LDPC to converge to a wrong codeword — an undetected error
       that CRC-24 is specifically designed to catch.
   * - ``messages/frame.cpp``
     - ~87%
     - DEFENCE_IN_DEPTH
     - Aliasing checks, CFI violation paths, and ``default:`` in BCH status
       switch (retained per MISRA Rule 16.4). ``encode_bch_header()`` error
       paths through ``frame_build()`` now covered by FID/TOI validation
       tests. Remaining lines require interleaver fault (aliasing), CRC
       internal failure, or LDPC internal failure — all radiation effects.
   * - ``encoding/ldpc.cpp``
     - ~86%
     - DEFENCE_IN_DEPTH
     - Saturating arithmetic (``sat_add``/``sat_sub``) overflow paths require
       accumulated Q8.7 fixed-point values exceeding ±32767 — unlikely with
       NMS α=0.75 damping on well-formed inputs. Sparse matrix bounds checks
       (``check_single_row_parity``, ``multiply_sparse_hardened``,
       ``sparse_row_dot_product``) require corrupted row_ptr/col_indices
       arrays (radiation effect on .rodata). Alpha validation edge cases
       covered by existing tests.
   * - ``core/sise.cpp``
     - ~90%
     - DEFENCE_IN_DEPTH
     - 4 NOCOVER markers: (1) RSS post-condition overflow check — requires
       component differences near ``DBL_MAX`` (~1e308); cislunar coordinates
       bounded to ~5×10⁸ m, making overflow impossible in practice (2 lines).
       (2) CFI sentinel failure paths in ``compute_sise()`` and
       ``compute_sise_percentile()`` — volatile counter always matches under
       nominal execution (2 lines).  FMEA: FM-073, FM-074.
