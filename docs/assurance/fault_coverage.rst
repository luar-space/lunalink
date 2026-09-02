Fault Coverage Report
=====================

**Document ID**: LUNALINK-FAULT-COV-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.5; method per ECSS-Q-HB-80-03A Rev.1; NPR 7150.2D SWE-027 (secondary); Safety Plan SW-SAF-06
**Revision**: 2.0
**Date**: 2026-03-20
**Status**: Released — Competition Submission Baseline

Purpose
-------

SWIFI (Software-Implemented Fault Injection) test-suite coverage for LunaLink,
and the primary evidence artefact for ``SW-SAF-06`` ("the SWIFI suite shall
achieve 100% single-bit fault detection coverage for all hardening
patterns").

The table below is **human-curated and aligned with the SWIFI test suite** in
the per-module test files.  Adding a fault scenario or changing a hardening
pattern means updating it.

Injection Technique Coverage
----------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 12 58

   * - #
     - Technique
     - Status
     - Notes
   * - 1
     - **Bit-flip injection** (memory/hardware fault tolerance)
     - FULL
     - ``LdpcCsrMatrix::inject_bit_flip()``; FI-05, FI-13, FI-14
   * - 2
     - **Stuck-at fault** (register corruption handling)
     - FULL
     - ``TmrValue::inject_fault()``, ``CheckedRange::force_set()``; FI-01–FI-04, FI-35, FI-36
   * - 3
     - **Return-value injection** (error-handling paths)
     - FULL
     - ``LdpcEncodeWorkspace::fi_force_status``, ``FrameDecodeWorkspace::fi_force_sync_fail``,
       ``fi_force_ldpc_fail``; FI-27–FI-31
   * - 4
     - **Exception injection** (exception safety & recovery)
     - N/A
     - LunaLink builds with ``-fno-exceptions``.  All error paths use status-code
       returns (SEU-hardened enums) + telemetry emission.  Exception injection is
       inapplicable by design.  See `Exception Injection Rationale`_ below.
   * - 5
     - **Timing injection** (race conditions & deadlocks)
     - FULL
     - ``WindowedWatchdog::inject_tick_count()``; FI-12, FI-32–FI-34
   * - 6
     - **Macro-based injection** (clean enable/disable for CI)
     - FULL
     - ``#ifdef LUNALINK_FAULT_INJECTION``; all injection APIs gated; CI via ``task swifi``
   * - 7
     - **Checksum validation** (fault detection mechanisms)
     - FULL
     - CRC-32 (LDPC), golden CRC (Legendre/BCH/Weil); FI-05, FI-15

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
   * - FI-23
     - Frame Decode CFI skip — injected milestone skip fires
       ``kFaultDetected`` + FaultPacket (FM-060)
     - ``workspace.cfi_skip_inject = 1``; call ``frame_decode()`` on
       noiseless signal that would normally succeed
     - CFI terminal check fires (``cfi_count != 0x7E7E7E7E + 6``);
       ``kFaultDetected`` returned; FaultPacket emitted
       (``capture.count() == 1``); workspace temporaries scrubbed
     - 1
     - PASS
   * - FI-24
     - Aliasing — overlapping workspace staging and output in
       ``frame_build_partial`` (FM-067)
     - Place ``FrameBuildWorkspace`` via placement new so ``staging``
       physically overlaps ``out`` span
     - ``spans_overlap()`` runtime guard fires; ``kFaultDetected``
       returned; staging scrubbed
     - 1
     - PASS
   * - FI-25
     - Aliasing — overlapping workspace staging and output in
       ``frame_build`` (FM-067)
     - Place ``FrameBuildWorkspace`` via placement new so ``staging``
       physically overlaps ``out`` span
     - ``spans_overlap()`` runtime guard fires; ``kFaultDetected``
       returned; staging scrubbed
     - 1
     - PASS
   * - FI-27
     - Return-value injection: ``ldpc_encode()`` forced ``kFaultDetected``
     - ``workspace.fi_force_status = kFaultDetected``; call ``ldpc_encode()``
     - Injected status returned immediately; output scrubbed; one-shot
       (subsequent call succeeds without injection)
     - 1
     - PASS
   * - FI-28
     - Return-value injection: ``subframe_build_sb2()`` forced LDPC failure
     - ``ldpc_ws.fi_force_status = kFaultDetected``; call ``subframe_build_sb2()``
     - Subframe builder propagates failure; output buffer scrubbed (all zero)
     - 1
     - PASS
   * - FI-29
     - Return-value injection: ``frame_build()`` forced LDPC failure
     - ``ldpc_ws.fi_force_status = kFaultDetected``; call ``frame_build()``
     - Frame build fails; full 6000-symbol output scrubbed; staging scrubbed
     - 2
     - PASS
   * - FI-30
     - Return-value injection: ``frame_decode()`` forced sync failure
     - ``workspace.fi_force_sync_fail = 1``; call ``frame_decode()``
     - Decode returns ``kSyncNotFound``; all workspace data buffers scrubbed
     - 1
     - PASS
   * - FI-31
     - Return-value injection: ``frame_decode()`` forced LDPC failure
     - ``workspace.fi_force_ldpc_fail = 1``; call ``frame_decode()``
     - Decode returns ``kFaultDetected``; all workspace data buffers scrubbed
     - 2
     - PASS
   * - FI-32
     - Timing injection: watchdog upper-bound exceeded (informational)
     - ``inject_tick_count(t_max + 1)``; query ``upper_bound_exceeded()``
     - Upper-bound flag set to true; ``evaluate()`` still passes (upper bound
       is informational — hardware watchdog owns hang path); no FaultPacket
     - N/A (informational)
     - PASS
   * - FI-33
     - Timing injection: zero-tick starvation (complete ``wip_tick()`` absence)
     - ``reset()`` then immediately ``evaluate()`` with zero ticks
     - Lower-bound violated (0 < t_min); ``FaultPacket(fiz_level=2)`` emitted;
       ``evaluate()`` returns false
     - 2
     - PASS
   * - FI-34
     - Timing injection: exact boundary tick counts (t_min and t_max)
     - ``inject_tick_count(t_min)`` and ``inject_tick_count(t_max)``
     - Both exact boundary values pass; ``t_min - 1`` fails with FaultPacket
     - 1–2
     - PASS
   * - FI-35
     - TMR two-of-three corruption (different bad values, no majority)
     - ``inject_fault(0, BAD1)`` + ``inject_fault(1, BAD2)``; ``vote()``
     - No majority exists; ``vote()`` falls back to v1 (incorrect); documents
       TMR correction boundary: single-copy only.
     - N/A (boundary)
     - DOCUMENTED
   * - FI-36
     - TMR two-of-three corruption (same bad value, wrong majority wins)
     - ``inject_fault(0, BAD)`` + ``inject_fault(1, BAD)``; ``vote()``
     - Two corrupted copies form false majority; ``vote()`` returns wrong value;
       confirms FI-02 residual risk from the two-copy injection perspective.
     - N/A (residual)
     - DOCUMENTED
   * - FI-37
     - ``compute_sise()`` input validation + scrubbing (FM-073)
     - Pass NaN or Inf in any of the 16 input components (4 vectors × 4 fields);
       ``is_finite()`` rejects at entry; CFI milestone 1 never reached; output
       struct scrubbed.
     - ``kInvalidInput`` returned; ``position_error_m`` and ``velocity_error_mps``
       zeroed; CFI sentinel remains at base value.  Covered by
       ``[LSIS-SISE] SWIFI: compute_sise NaN injection``,
       ``compute_sise Inf injection``, and ``compute_sise clock term injection``.
     - 1 (contained)
     - PASS
   * - FI-38
     - ``compute_sise_percentile()`` validation + scrubbing (FM-074)
     - Pass nullptr, mismatched spans, NaN-containing arrays, empty data, or
       out-of-range percentile (>1.0, <0.0, NaN); ``validate_percentile_inputs()``
       rejects; output scrubbed before return.
     - ``kInvalidInput`` or ``kEmptyData`` returned; percentile outputs zeroed;
       ``position_pass`` and ``velocity_pass`` forced to false.  Covered by the
       four ``[LSIS-SISE] SWIFI: compute_sise_percentile …`` test cases.
     - 1 (contained)
     - PASS
   * - FI-41
     - LDPC SP decoder ``phi_sum`` TMR bitwise mirror detection (FM-080)
     - Corrupt one ``uint64_t`` complement mirror of the ``phi_sum``
       accumulator; call ``verify_mirrors()``
     - ``verify_mirrors()`` detects mismatch between primary value and
       complement mirror; returns ``false``; caller scrubs workspace and
       returns ``kFaultDetected``
     - 1 (contained)
     - PASS


Retired / Reserved Scenario IDs
-------------------------------

Scenario IDs are never reused once assigned, so the numbering above contains
deliberate gaps:

- **FI-16–FI-19, FI-26** — retired.  These IDs were assigned in an earlier
  development tree to scenarios for components that are not part of this
  release; the IDs are permanently retired rather than renumbered.
- **FI-39, FI-40** — reserved; never assigned to a shipped scenario.
- **FI-42–FI-45** — assigned to the descoped Gateway 6 parser scenarios
  documented below; no test case carries these tags in this release.

Summary
-------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Metric
     - Value
   * - Total injection scenarios
     - 34 (table rows above)
   * - Scenario status: PASS
     - 31 (includes FI-32, whose upper-bound detection is
       informational-only by design)
   * - Scenario status: DOCUMENTED residual-risk boundary
       (FI-02, FI-35, FI-36)
     - 3
   * - Total accounted for
     - 34 (31 PASS + 3 DOCUMENTED)
   * - SWIFI ``TEST_CASE`` count (all passing)
     - 42 — see ``evidence/swifi_results.json`` and `Test Suite Reference`_
   * - Undetected faults
     - 0
   * - Single-bit fault detection coverage
     - **100%** (excluding acknowledged residual risks)
   * - Injection techniques implemented
     - **7 of 7** (6 active + 1 N/A by design)

The residual risks (FI-02 — all-TMR-copies-same corruption; FI-35/FI-36 — TMR
two-of-three corruption with different/same bad values) are acknowledged
limitations of the TMR pattern: overwrite several copies at once and the
majority vote has nothing left to compare against.  That limit is inherent to
TMR without hardware scrubbing, and the FMEA (``assurance/fmea.rst``) records
it as such.

Descoped Scenarios (Gateway 6 — Message Parsing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following SWIFI scenarios are **documented and descoped for this
release**: the parser hardening mechanisms they target are implemented and
analysed in FMEA entries FM-069 through FM-072, but no automated injection
test case exists for them yet.  They are not counted in the summary above.

- **FI-42** (descoped): CFI skip injection in ``extract_bits()`` — corrupt
  ``cfi_count`` sentinel; verify ``kFaultDetected`` returned and output
  scrubbed.
- **FI-43** (descoped): Bit-value injection in ``parse_sb2()`` — set a bit
  in the SB2 data array to value 2 (invalid); verify ``kInvalidInput``
  propagated from ``extract_bits()``.
- **FI-44** (descoped): CFI skip injection in ``parse_sb3()`` /
  ``parse_sb4()`` — corrupt ``cfi_count``; verify ``kFaultDetected``.
- **FI-45** (descoped): CFI skip injection in ``compute_tot()`` — corrupt
  ``cfi_count``; verify ``kFaultDetected`` and ``result.t_f`` scrubbed.

PR #77 Coverage Notes (FM-103)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

FMEA entry FM-103 (``enforce_spare_bit_pattern`` SB2 spare-region rewrite +
defensive bounds guard) was added in PR #77 as part of the G-18 LSIS-AFS-300
enforcement work.

- **FM-103 — nominal rewrite path:** covered by the round-trip tests added
  in ``cpp/tests/messages/test_frame.cpp`` ("builder enforces LSIS-300
  regardless of caller input" and "frame_build emits alternating SB2 spare
  pattern").  Both tests pass deliberately non-compliant SB2 spare bytes
  through the public ``subframe_build_sb2`` / ``frame_build`` entry points
  and assert that the emitted frame contains the alternating 0/1 pattern at
  ``[kSb2SpareOffset, kSb2SpareOffset + kSb2SpareBits)``.  The CRC-24 and
  LDPC encode steps run over the post-enforcement bytes, so any failure of
  the rewrite would cascade into a CRC or LDPC mismatch on round-trip
  decode.
- **FM-103 — defensive bounds guard:** the ``msg.size() <
  kSb2SpareOffset + kSb2SpareBits`` branch is unreachable from the nominal
  caller (``subframe_build_ldpc`` always passes a 1200-byte span for SF2),
  marked ``NOCOVER(DEFENCE_IN_DEPTH)``.  A dedicated SWIFI scenario that
  injects a short span via SEU corruption of ``expected_msg_bits`` is
  **deferred to the next SWIFI sweep** — tracked in the FM-103 "Open
  items" field in ``fmea.rst``.  No change to the ``Total scenarios``
  count in this PR.

.. _Exception Injection Rationale:

Exception Injection Rationale
-----------------------------

LunaLink builds with ``-fno-exceptions`` (see ``c_plus_plus_guidelines.rst``
§2.1).  All error paths use:

1. **SEU-hardened status-code enums** — Hamming distance ≥ 3 between valid
   values (e.g., ``kOk = 0x5A``, ``kFaultDetected = 0x99``), preventing
   single-bit flips from silently transforming one status into another.

2. **Telemetry emission** — ``FaultPacket`` with FIZ-level severity emitted on
   every fault path, providing real-time observability.

3. **Unconditional output scrubbing** — all output buffers zeroed on any
   non-``kOk`` status, ensuring no stale or corrupted data escapes.

Exception injection is therefore **not applicable**: the error-handling paths
that exceptions would test are fully exercised by the return-value injection
technique (FI-27–FI-31) and CFI skip injection (FI-04, FI-13, FI-14, FI-20,
FI-22, FI-23).

Test Suite Reference
---------------------

Every scenario in this report is verified by at least one ``TEST_CASE`` in
the per-module SWIFI test files (``cpp/tests/<domain>/test_swifi_*.cpp``:
``test_swifi_signal.cpp``, ``test_swifi_encoding.cpp``,
``test_swifi_messages.cpp``, ``test_swifi_codes.cpp``,
``test_swifi_platform.cpp``, ``test_swifi_core.cpp``).  The mapping is not
1-to-1: the suite contains 42 ``TEST_CASE`` entries (all passing — see
``evidence/swifi_results.json``) covering the 34 scenarios as follows:

- 32 scenarios map 1-to-1 to a single ``TEST_CASE`` each;
- **FI-37** is covered by three ``TEST_CASE`` entries (NaN, Inf, and
  clock-term injection) and **FI-38** by four (mismatched spans, NaN data,
  empty data, invalid percentile), all in ``test_swifi_core.cpp``;
- three supporting ``TEST_CASE`` entries carry no scenario ID: the POST
  step-2 LUT-corruption check, the nominal POST path, and the
  checksum-golden reporting utility (``test_swifi_platform.cpp``).

The test executable is ``lunalink_swifi_tests``, built with
``LUNALINK_FAULT_INJECTION=1``.

Run all SWIFI tests:

.. code-block:: bash

   task swifi

Or directly:

.. code-block:: bash

   cmake --build build/debug --target lunalink_swifi_tests
   build/debug/lunalink_swifi_tests --reporter console
