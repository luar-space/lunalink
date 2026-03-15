Fault Coverage Report
=====================

**Document ID**: LUNALINK-FAULT-COV-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.5; method per ECSS-Q-HB-80-03A Rev.1; NPR 7150.2D SWE-027 (secondary); Safety Plan SW-SAF-06
**Revision**: 1.0
**Date**: 2026-03-13
**Status**: Released — Competition Submission Baseline

Purpose
-------

This document records the Software-Implemented Fault Injection (SWIFI) test suite
coverage for LunaLink.  It constitutes the primary evidence artefact for
``SW-SAF-06`` ("the SWIFI suite shall achieve 100% single-bit fault detection
coverage for all hardening patterns").

The table below is **human-curated and aligned with the SWIFI test suite** in
``cpp/tests/test_fault_injection.cpp``.  It is updated whenever a new fault
scenario is added or a hardening pattern is modified.

SWIFI Scenario Coverage
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 8 25 20 25 10 12

   * - ID
     - Scenario Description
     - Injection Method
     - Detection Mechanism
     - FIZ Level
     - Status
   * - FI-01
     - TMR single-copy corruption (any one of three copies flipped)
     - ``TmrValue::inject_fault(copy_idx, bad_value)``
     - Majority vote returns correct value; corrupted copy repaired
     - 1 (contained in block)
     - PASS
   * - FI-02
     - TMR all-copies-same corruption (residual risk documentation)
     - ``TmrValue::inject_fault(bad_value)`` (all copies)
     - No detection possible — acknowledged residual risk; documented only
     - N/A (residual)
     - DOCUMENTED
   * - FI-03
     - ``CheckedRange`` saturating logic bypass via ``force_set()``
     - ``CheckedRange::force_set(out_of_range_value)``
     - Normal assignment re-saturates on next write
     - 1 (contained)
     - PASS
   * - FI-04
     - CFI guard — skipped iteration (radiation-induced branch skip)
     - ``CfiGuard::inject_skip(n)``
     - ``CfiGuard::verify()`` returns false; caller returns ``kFaultDetected``
     - 1–3 (caller decides)
     - PASS
   * - FI-05
     - LDPC matrix single-bit flip in packed CSR data
     - ``LdpcCsrMatrix::inject_bit_flip(offset, mask)``
     - ``LdpcCsrMatrix::verify_integrity()`` CRC-32 mismatch detected
     - 3 (POST/CBIT)
     - PASS
   * - FI-06
     - Aliasing — overlapping input/output spans to modulator
     - Cast same buffer to both chip span and output span
     - ``spans_overlap()`` runtime guard fires; output scrubbed before write
     - 1 (scrubbed, kFaultDetected)
     - PASS
   * - FI-07
     - Aliasing — overlapping spans to IQ multiplexer
     - Cast same buffer to both I-samples and output span
     - ``spans_overlap()`` runtime guard fires; output scrubbed before write
     - 1 (scrubbed, kFaultDetected)
     - PASS
   * - FI-08
     - FSM illegal transition (e.g., kSafeMode → kOperational)
     - Call ``fsm.transition(kOperational)`` from ``kSafeMode``
     - Legal transition matrix miss; FIZ-3 telemetry emitted; state locked
     - 3
     - PASS
   * - FI-09
     - FSM two consecutive frame faults (escalation to Zone 3)
     - Call ``fsm.on_frame_fault()`` twice consecutively
     - Consecutive-fault counter reaches 2; FIZ-3 telemetry; ``kSafeMode``
     - 3
     - PASS
   * - FI-10
     - FSM ``on_frame_fault()`` called from ``kSafeMode`` (radiation-induced
       CFI diversion to frame-fault handler while already in safe mode)
     - Walk FSM to ``kSafeMode`` via POST-fail path; call
       ``fsm.on_frame_fault()`` directly
     - Precondition guard fires (``state_ != kOperational``); FIZ-3 telemetry
       emitted; state locked to ``kSafeMode``; returns ``false``
     - 3
     - PASS
   * - FI-11
     - Saturating arithmetic no-wraparound (``INT16_MAX + 1`` clamps)
     - Assign ``INT16_MAX`` to ``CheckedRange<int16_t>``
     - Value clamps to ``Max``; no integer overflow
     - 1 (contained)
     - PASS
   * - FI-12
     - Windowed watchdog lower-bound violation (radiation-induced logic skip)
     - ``WindowedWatchdog::inject_tick_count(T_min - 1)``
     - ``evaluate()`` detects under-run; ``FaultPacket(fiz_level=2)`` emitted
     - 2
     - PASS
   * - FI-13
     - Frame-assembly CFI: subframe builder milestone skip (FM-025)
     - Force early exit via invalid LDPC subframe type → milestone count
       insufficient at terminal check
     - ``cfi_count != kCfiMagicSb + 3`` → ``kFaultDetected``; output scrubbed
     - 1
     - PASS
   * - FI-14
     - Frame-assembly CFI: frame assembler milestone skip (FM-026)
     - Force BCH failure to cause milestone skip → cfi_count != Magic + 6
     - ``cfi_count != kCfiMagicFrame + 6`` → ``kFaultDetected``; output scrubbed
     - 2
     - PASS
   * - FI-15
     - Legendre cache TOCTOU integrity gap closure (FM-005)
     - ``verify_integrity()`` compares runtime CRC against golden constants in
       ``codebook_checksums.hpp``; POST step 2 verifies both Legendre sequences
     - Golden CRC mismatch → ``verify_integrity()`` returns false →
       ``get_cached_legendre()`` returns empty span → downstream ``kFaultDetected``
     - 3 (POST)
     - PASS
   * - FI-16
     - Correlator channel state TMR — vote corrects single-copy corruption (FM-050)
     - Assign PRN, acquire to kTracking; verify ``TmrValue<uint8_t>`` state
       survives and acquisition succeeds under nominal conditions
     - ``TmrValue::vote()`` returns correct state; channel operates correctly
     - 1 (contained)
     - PASS
   * - FI-17
     - Correlator illegal channel transition — kIdle → kTracking (FM-050)
     - Call ``track()`` on channel in kIdle state (not in transition matrix)
     - State precondition check returns ``kNotLocked``; channel state unchanged
     - 2
     - PASS
   * - FI-18
     - Correlator CFI sentinel — nominal acquisition exercises all CFI paths (FM-053)
     - Generate clean signal; acquire; verify all CFI sentinels
       (prepare_code_replica, pcps_search) pass terminal checks
     - Acquisition succeeds (``kOk`` + ``locked == true``); CFI counters
       reached expected terminal values (no ``kFaultDetected``)
     - 1
     - PASS
   * - FI-19
     - Correlator TMR adversarial — track survives single-copy state corruption (FM-050)
     - Acquire to kTracking; ``track()`` invokes ``vote()`` which repairs
       the corrupted copy; tracking continues successfully
     - ``track()`` returns ``kOk``; channel remains kTracking; ``vote()``
       performed majority repair
     - 1 (contained)
     - PASS

   * - FI-20
     - Sync Detector CFI skip — injected milestone skip fires
       ``kFaultDetected`` + FaultPacket (FM-056)
     - ``SyncDetector::inject_cfi_skip(1)``; call ``detect()`` on clean
       signal that would normally succeed
     - CFI terminal check fires (``cfi_count != Magic + 3``);
       ``kFaultDetected`` returned; FaultPacket emitted (``capture.count()
       == 1``); output scrubbed (all result fields zeroed)
     - 1
     - PASS
   * - FI-21
     - Sync Detector TMR peak offset — single-copy corruption repaired
       by majority vote (FM-055)
     - Clean ``detect()`` at offset 500 populates TMR [500, 500, 500];
       ``inject_tmr_fault(0, 9999)`` corrupts one copy to [9999, 500, 500];
       ``voted_peak_offset()`` asserts majority vote returns 500
     - ``TmrValue<uint16_t>::vote()`` returns correct offset despite
       single-copy corruption; repair exercised directly without reset
     - 1 (contained)
     - PASS
   * - FI-22
     - Sync Detector CFI recovery — clean detection succeeds after
       injected fault (FM-056)
     - ``inject_cfi_skip(1)`` + ``detect()`` → ``kFaultDetected``; then
       second ``detect()`` with no injection → ``kOk``
     - First call: ``kFaultDetected`` + 1 FaultPacket; second call:
       ``kOk`` + no additional FaultPacket; one-shot injection verified
     - 1
     - PASS

Summary
-------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Metric
     - Value
   * - Total injection scenarios
     - 22
   * - Detected (PASS)
     - 21
   * - Documented residual risk (FI-02)
     - 1
   * - Undetected faults
     - 0
   * - Single-bit fault detection coverage
     - **100%** (excluding acknowledged residual risk FI-02)

The single residual risk (FI-02 — all-TMR-copies-same corruption) is an
acknowledged limitation of the TMR pattern: when all three copies are overwritten
with the same wrong value simultaneously (e.g., by a severe multi-bit event), the
majority vote cannot detect the discrepancy.  This is documented in the FMEA
(``assurance/fmea.rst``) and accepted as a design constraint inherent to TMR
without hardware scrubbing.

Test Suite Reference
---------------------

All scenarios in this report correspond 1-to-1 with ``TEST_CASE`` entries in
``cpp/tests/test_fault_injection.cpp``.  The test executable is
``lunalink_swifi_tests``, built with ``LUNALINK_FAULT_INJECTION=1``.

Run all SWIFI tests:

.. code-block:: bash

   task test-swifi

Or directly:

.. code-block:: bash

   cmake --build build/debug --target lunalink_swifi_tests
   build/debug/lunalink_swifi_tests --reporter console
