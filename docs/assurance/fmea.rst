Software FMEA
=============

**Document ID**: LUNALINK-FMEA-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.5 (dependability); method per ECSS-Q-HB-80-03A Rev.1; ECSS-Q-ST-30C; MIL-STD-1629A; NPR 7150.2D SWE-028/029 (secondary)
**Revision**: 1.3
**Date**: 2026-03-20
**Status**: Released — Competition Submission Baseline

This Software Failure Modes and Effects Analysis (FMEA) is the formal companion to the
Fault Containment Architecture described in ``mission_assurance.rst``.  Where that document
specifies *how* LunaLink responds to faults, this document analyses *what can go wrong*,
*what happens to the system if it does*, and *how the fault is detected and contained*.

The scope is all safety-critical software functions in the core C++ source directories.  Hardware failure
modes (power supply, oscillator, memory DRAM/SRAM) are outside scope and are treated
as inputs to this analysis via the system-level hazard analysis.

Severity Classification
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 10 20 70

   * - Severity
     - Label
     - Definition
   * - S1
     - Catastrophic
     - Loss of mission; erroneous navigation data broadcast to dependent rovers with no
       detection.  Can cause rover hardware damage or loss.
   * - S2
     - Critical
     - Sustained navigation service outage (> 1 frame period = 12 s) without ground
       notification; or systematic position-fix errors detectable only post-mission.
   * - S3
     - Marginal
     - Single-frame outage (12 s gap); system self-recovers to Zone 2 state; ground
       telemetry alarm generated.
   * - S4
     - Negligible
     - Degraded diagnostic capability only; navigation service unaffected.

Detection Classifications
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 90

   * - Code
     - Meaning
   * - D-POST
     - Detected by Power-On Self-Test before any transmission begins
   * - D-CBIT
     - Detected by Continuous Built-In Test during operation (background scrub)
   * - D-SWIFI
     - Detection verified empirically by the SWIFI fault-injection suite
   * - D-TELEM
     - Detected via fixed-width telemetry fault packet observable by ground
   * - D-FRAME
     - Detected by frame-level post-generation checksum before DMA hand-off
   * - D-NONE
     - No detection mechanism in software; relies on system-level or ground detection


FMEA Table — Spreading Code Generator (``gold.cpp``, ``weil.cpp``, ``matched_code.cpp``)
-----------------------------------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-001
     - SEU flips register bit inside Gold code LFSR
     - Incorrect chip sequence for one or more PRN IDs; receiver despreading fails;
       navigation message undecodable
     - D-SWIFI: Variable Mirroring on LFSR state catches single-bit deviation within
       one chip period.  D-FRAME: Post-generation checksum on final symbol stream.
     - S3
     - Zone 1
     - Variable mirroring on LFSR state registers; mirror mismatch → ``kFaultDetected``;
       frame scrubbed.  Next frame regenerated from correct ROM-based seed.
   * - FM-002
     - SEU corrupts LUT entry in packed PRN table
     - All frames using the affected PRN ID produce wrong chip sequence; error
       systematic until LUT repaired
     - D-POST: LUT CRC verified at boot.  D-CBIT: Background ``verify_integrity()``
       detects mismatch during operation.
     - S2
     - Zone 3
     - LUT CRC (CRC-32) checked at POST and by CBIT.  On mismatch: Zone 3 fault;
       signal chain halts; telemetry alarm; ground commanded recovery.
   * - FM-003
     - Matched-code XOR stage corrupted (wrong output buffer)
     - Combined I/Q chip sequence incorrect; affects all broadcast PRN IDs simultaneously
     - D-FRAME: Post-generation CRC detects wrong output before DMA hand-off.
       D-SWIFI: Aliasing protection catches overlapping buffer pointer.
     - S2
     - Zone 2
     - Pointer-range aliasing check at function entry.  Post-generation checksum
       as final gate.  Zone 2 fault: frame scrubbed, frame counter advanced.
   * - FM-004
     - SEU skips loop iteration in spreading code generation
     - Output sequence truncated; frame too short; receiver sync loss
     - D-SWIFI: CFI sentinel counter mismatch at loop exit.
     - S3
     - Zone 1
     - CFI guard wraps every generation loop.  Terminal check ``cfi_count !=
       Magic_Start + N`` → ``kFaultDetected``.
   * - FM-005
     - SEU during Legendre cache initialisation corrupts reference sequences
       (TOCTOU: self-computed CRC blesses bad data)
     - Weil code generation produces incorrect spreading codes for all PRN IDs;
       silent corruption — no downstream detection possible if CRC is self-signed
     - D-POST: POST step 2 compares runtime Legendre CRC against build-time
       golden constants (``kLegendre10223Checksum``, ``kLegendre1499Checksum``).
       D-CBIT: ``verify_integrity()`` checks against same golden constants on
       every cache access.
     - S2
     - Zone 3 (POST)
     - Golden CRCs computed at build time and committed to
       ``codebook_checksums.hpp``.  POST step 2 verifies both Legendre sequences
       against golden constants before entering OPERATIONAL.  Runtime
       ``get_cached_legendre()`` re-verifies on every access.


FMEA Table — Forward Error Correction (``bch.cpp``, ``ldpc.cpp``, ``interleaver.cpp``)
---------------------------------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-010
     - SEU flips bit in BCH encoder output buffer
     - SB1 subframe undecodable by receiver; loss of satellite status/health message
     - D-FRAME: Post-generation CRC on full frame catches single-bit corruption.
     - S3
     - Zone 2
     - Post-generation checksum at frame commit gate.  Zone 2 fault on failure.
   * - FM-011
     - SEU corrupts BCH codebook in RAM
     - All BCH-encoded SB1 subframes systematically wrong; persistent service
       degradation
     - D-POST: ``bch_codebook_checksum()`` at boot.  D-CBIT: Periodic re-check
       during operation.
     - S2
     - Zone 3
     - Codebook CRC verified by POST and CBIT.  Zone 3 fault on mismatch.
   * - FM-012
     - SEU flips bit inside LDPC parity computation (CSR matrix walk)
     - SB2-SB4 codeword invalid; receiver FEC cannot correct; frame loss
     - D-SWIFI: Variable mirroring on parity accumulator.  D-FRAME: Post-generation
       CRC.
     - S3
     - Zone 1–2
     - Variable mirroring on parity accumulator.  Frame-level post-generation
       checksum as secondary gate.
   * - FM-013
     - SEU corrupts LDPC CSR matrix (``LdpcCsrMatrix``) payload in RAM
     - All LDPC-encoded frames systematically wrong
     - D-POST: ``verify_integrity()`` CRC on CSR matrix at boot.  D-CBIT:
       JIT integrity check before each encode pass.  D-SWIFI: FI-05 (bit-flip
       detection), FI-27 (return-value injection on ``ldpc_encode``), FI-28/FI-29
       (caller scrub verification on ``subframe_build`` / ``frame_build``).
     - S2
     - Zone 3
     - CRC checked at POST, by CBIT background task, and by JIT integrity check
       immediately before every encode call.  Zone 3 fault on mismatch.
       Caller error-handling paths verified by return-value injection (FI-27–29).
   * - FM-014
     - SEU skips gather loop iteration in LDPC encoder
     - Parity vector computed from incomplete row set; wrong codeword output
     - D-SWIFI: CFI sentinel on gather loop.
     - S3
     - Zone 1
     - CFI guard on every gather-row loop.
   * - FM-015
     - Interleaver writes to wrong column index (SEU on index counter)
     - Interleaved symbol order incorrect; receiver de-interleaving produces
       garbled data
     - D-SWIFI: CFI sentinel on interleaver index.  D-FRAME: Post-generation CRC.
     - S3
     - Zone 1–2
     - CFI guard on interleaver column counter.  Post-generation CRC as secondary
       gate.
   * - FM-016
     - CRC-24 computation corrupted (wrong polynomial register)
     - Nav message delivered to receiver with incorrect integrity check value;
       receiver may accept corrupted data
     - D-SWIFI: Variable mirroring on CRC accumulator.
     - S1 (if receiver silently accepts bad CRC)
     - Zone 1
     - Variable mirroring on CRC accumulator.  Mirror mismatch → ``kFaultDetected``.
       Note: if detection fails, severity escalates to S1.  Mirroring + SWIFI
       100 % coverage required.


FMEA Table — Frame Assembly & Modulation (``frame.cpp``, ``modulator.cpp``, ``iq_mux.cpp``)
---------------------------------------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-020
     - SEU corrupts sync pattern in frame header
     - Receiver cannot acquire frame sync; navigation service unavailable until
       receiver re-acquires
     - D-FRAME: Post-generation CRC covers full frame including sync pattern.
     - S3
     - Zone 2
     - Post-generation checksum gate.  Zone 2 fault on failure.
   * - FM-021
     - Frame counter increments incorrectly (SEU on counter register)
     - Time-of-transmission field wrong; rover ranging error
     - D-SWIFI: ``CheckedRange<T>`` saturating arithmetic detects wrap-around.
       TMR on frame counter in safety-critical path.
     - S2
     - Zone 1
     - ``CheckedRange`` saturating arithmetic + TMR voting on frame counter.
   * - FM-022
     - IQ mux assigns wrong power ratio (50/50 constraint violated)
     - Receiver acquisition performance degraded; potential loss of lock at
       minimum power levels
     - D-FRAME: Structural validation checks I/Q amplitude balance before DMA
       hand-off.
     - S3
     - Zone 2
     - Structural frame validation at commit gate.
   * - FM-023
     - SEU corrupts DMA hand-off CRC after computation
     - Corrupted frame transmitted; no on-board detection post-hand-off
     - D-SWIFI: Handoff CRC check by consumer before DMA trigger.
     - S2
     - Zone 1 (pre-DMA)
     - Handoff CRC re-verified by DMA consumer before initiating transfer.
       If mismatch: transmission suppressed; Zone 2 fault.
   * - FM-024
     - SEU corrupts symbol buffer pointer (aliasing between I and Q buffers)
     - I channel data written to Q buffer or vice versa; wrong signal on air
     - D-SWIFI: Pointer-range aliasing check at modulator entry.
     - S2
     - Zone 1
     - Pointer-range aliasing check at function entry; ``kFaultDetected`` if
       overlap detected.
   * - FM-025
     - SEU skips milestone step in subframe builder CFI
       (``subframe_build_ldpc`` 3-milestone counter)
     - CRC or LDPC encode step silently skipped; subframe codeword incomplete
       or wrong
     - D-SWIFI: CFI sentinel terminal check (``cfi_count != Magic + 3``) fires
       ``kFaultDetected``.
     - S3
     - Zone 1
     - ``volatile uint32_t cfi_count`` with unique magic pattern; milestone
       incremented after each step; terminal check before ``kOk`` promotion.
       Output scrubbed on failure.
   * - FM-026
     - SEU skips milestone step in frame assembler CFI
       (``frame_build`` 6-milestone counter)
     - One or more frame assembly stages skipped; incomplete frame
     - D-SWIFI: CFI sentinel terminal check (``cfi_count != Magic + 6``) fires
       ``kFaultDetected``.
     - S3
     - Zone 2
     - ``volatile uint32_t cfi_count`` with unique magic pattern; 6 milestones
       (sync, SB1, SB2, SB3, SB4, interleave); terminal check.  Output scrubbed
       on failure.
   * - FM-027
     - SEU skips milestone step in partial frame builder CFI
       (``frame_build_partial`` 3-milestone counter)
     - Partial frame incomplete; SB1 or interleave step skipped
     - D-SWIFI: CFI sentinel terminal check (``cfi_count != Magic + 3``) fires
       ``kFaultDetected``.
     - S3
     - Zone 2
     - ``volatile uint32_t cfi_count`` with unique magic pattern; 3 milestones
       (sync, SB1, interleave); terminal check.  Output scrubbed on failure.
   * - FM-103
     - ``enforce_spare_bit_pattern()`` over-writes the SB2 spare region
       with the LSIS-300 alternating 0/1 pattern before CRC-24 is
       computed.  Failure modes: (a) SEU on ``kSb2SpareOffset`` /
       ``kSb2SpareBits`` causes the loop to target a wrong byte range
       within the 1200-byte message buffer; (b) SEU on the
       ``expected_msg_bits`` parameter causes ``subframe_build_ldpc`` to
       pass a shorter-than-1200 span to the helper; (c) SEU on the
       ``type`` argument causes SF3/SF4 to be rewritten (they should
       be pass-through).
     - (a) wrong-byte write corrupts either the Time-Conversions field
       or the SB2 CRC region, producing a frame that either fails its
       own CRC-24 on transmit or is silently garbled on the wire.  (b)
       silently writing past the end of a shorter span would corrupt
       stack-adjacent data — defended by the defence-in-depth bounds
       guard inside the helper.  (c) a corrupted SF3/SF4 data span is
       caught by its own CRC-24 guard on receive (LSIS-FID0-465).
     - D-BOUNDS + D-TYPE: The helper contains an explicit
       ``msg.size() >= kSb2SpareOffset + kSb2SpareBits`` guard before
       the rewrite loop — an SEU-shortened span triggers a silent skip
       rather than a stack overrun, and the downstream CRC-24 check on
       the intact message catches the anomaly on the receive side.
       The ``type`` check uses the SEU-hardened ``LdpcSubframe`` enum
       values (Hamming distance ≥ 3) — a single bit flip cannot
       transform ``kSF3`` / ``kSF4`` into ``kSF2``.  The constants
       ``kSb2SpareOffset`` / ``kSb2SpareBits`` are ``constexpr`` with
       static initialisation (see message_parser.hpp) and live in the
       ``.rodata`` LUT section protected by the linker script — SEU on
       those values is the same attack surface as the BCH / LDPC parity
       tables and shares their fault-coverage argument.
     - S2
     - Zone 2
     - Post-G-18 addition.  Covered by
       ``[LSIS-AFS-300] SB2 builder enforces LSIS-300 pattern regardless
       of caller input`` and ``[LSIS-AFS-300] frame_build emits LSIS-300
       pattern in the SB2 spare region`` in test_frame.cpp.  SWIFI
       coverage for the defensive bounds guard is the only open item
       and is tracked for the next SWIFI sweep.


FMEA Table — State Machine & System-Level (top-level signal chain FSM)
-----------------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-030
     - SEU flips state register to undefined value
     - Signal chain attempts undefined state behaviour; unpredictable output
     - D-SWIFI: Forbidden State Detection (``default`` branch = Critical Fault).
     - S2
     - Zone 2–3
     - SEU-resistant enumerations; ``default`` branch triggers Zone 3 safe mode +
       telemetry fault packet.
   * - FM-031
     - SEU forces illegal state transition (valid states, invalid edge)
     - Hardware power amplifier commanded to a forbidden configuration; potential
       hardware damage
     - D-SWIFI: Legal Transition Matrix miss fires ``kFaultDetected``.
     - S1
     - Zone 3
     - Legal Transition Matrix; any non-permitted ``(current, next)`` pair →
       ``kFaultDetected`` + telemetry + safe mode.
   * - FM-032
     - Two consecutive Zone 2 frame faults
     - Systematic signal chain degradation; navigation service quality unknown
     - D-TELEM: Zone 3 safe mode trigger; telemetry alarm to ground.
     - S2
     - Zone 3
     - Zone 2 fault counter monitored; two consecutive faults → Zone 3 safe mode;
       halts transmission; ground commanded recovery.
   * - FM-033
     - Windowed watchdog lower-bound violation (task completes too early)
     - Radiation-induced logic skip; frame generation incomplete; wrong frame on air
     - D-TELEM: Windowed watchdog lower-bound trip; fault alarm.  D-SWIFI:
       SWIFI tests FI-12 (lower-bound trip), FI-32 (upper-bound informational),
       FI-33 (zero-tick starvation), FI-34 (exact boundary ticks) verify detection
       empirically across the full windowed-watchdog operating envelope.
     - S2
     - Zone 3
     - **Implemented** — ``WindowedWatchdog`` class in ``watchdog.hpp``; lower-bound
       trip emits ``FaultPacket(fiz_level=2)``; 4 SWIFI scenarios (FI-12, FI-32–34)
       cover under-run, over-run, starvation, and exact-boundary conditions.
       WCET-derived lower and upper thresholds documented in
       ``assurance/wcet_profile.rst``.  Lower-bound trip treated as Critical Fault;
       Zone 3 response.
   * - FM-034
     - CFI binary identity (SCI header) corrupt at boot
     - Unknown software version executing; traceability broken; may be wrong build
     - D-POST: SCI header CRC verified as first POST step.
     - S2
     - Zone 3
     - SCI header CRC checked as step 1 of POST.  Mismatch → boot abort; no
       transmission; telemetry fault; awaits ground command.
       Build-time CRC serialises ``uint32_t`` fields in native byte order
       (``sys.byteorder``) to match runtime ``reinterpret_cast`` layout;
       validated on LE (x86_64, ARM64) and BE (s390x) via cross-architecture
       CI (``cross-arch.yml``).


FMEA Table — Telemetry & Fault Reporting (``telemetry.cpp``)
-------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-040
     - SEU corrupts telemetry fault packet before transmission
     - Ground receives wrong fault code; incorrect anomaly response
     - D-NONE (within telemetry subsystem)
     - S4
     - —
     - Fixed-width binary packet format minimises corruption blast radius.
       Telemetry CRC at transport layer.  Residual risk accepted; S4 severity.
   * - FM-041
     - Stack overflow during fault reporting
     - Secondary fault during fault handling; potential loss of fault log
     - D-POST: Stack sentinel check (step 4 of POST).
     - S3
     - Zone 3
     - Fixed-width telemetry eliminates variable-length stack use.  Stack sentinel
       verified at POST.  ``-fstack-protector-strong`` compiler canary.


FMEA Table — Sync Pattern Detector (``sync_detector.cpp``)
-----------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-055
     - SEU corrupts TMR peak offset register during cross-correlation search
     - Frame boundary detected at wrong offset; downstream parser extracts
       garbled subframes; navigation message decode fails
     - D-SWIFI: TMR majority vote on ``peak_offset_tmr_`` detects single-copy
       corruption; two correct copies repair the third.  Adversarial SWIFI
       test FI-21 injects single-copy fault via ``inject_tmr_fault()`` and
       verifies repair.
     - S3
     - Zone 1
     - ``TmrValue<uint16_t>`` on peak offset (member variable); majority vote
       before populating ``SyncResult::frame_offset``; single-copy corruption
       repaired by ``vote()``.  SWIFI-verified via ``inject_tmr_fault()``.
   * - FM-056
     - SEU skips CFI milestone in ``detect()`` (3-milestone counter)
     - One or more detection stages skipped; result populated from incomplete
       search; wrong frame boundary or missed detection
     - D-SWIFI: CFI sentinel terminal check (``cfi_count != Magic + 3``) fires
       ``kFaultDetected``; FaultPacket emitted via telemetry HAL.
     - S3
     - Zone 1
     - ``volatile uint32_t cfi_count`` with unique magic pattern
       (0x7E7E7E7E); milestone incremented after input validation, search
       completion, and result population; terminal check before ``kOk``
       promotion.  Output scrubbed on failure.
   * - FM-057
     - SEU corrupts sync reference pattern (``sync_ref_`` array) in RAM
     - Cross-correlation matches wrong pattern; sync detected at wrong offset
       or not detected at all; navigation service outage
     - D-CBIT: Periodic re-computation of sync reference from ``kSyncPattern``
       can detect mismatch.  D-POST: ``kSyncPattern`` covered by LUT CRC in
       frame.cpp.
     - S2
     - Zone 2
     - ``kSyncPattern`` source array is ``constinit`` in pinned LUT section;
       ``sync_ref_`` recomputed at construction from ROM source.  Residual
       risk: runtime corruption of ``sync_ref_`` between construction and use.
       Mitigation: periodic CBIT re-check of reference pattern.
   * - FM-058
     - False sync detection (noise correlation exceeds threshold)
     - Parser attempts to decode noise as navigation data; CRC-24 check on
       subframes will reject; single-frame outage
     - D-FRAME: CRC-24Q on decoded subframes catches false-sync data.
       Pfa < 1% at Es/N0 = 0 dB empirically verified: 0/1000 false alarms
       in pure-AWGN trials (``[channel][pd][rx]`` test,
       ``test_sync_detection_rate.cpp``).
     - S3
     - Zone 2
     - Default threshold (50.0) gives >11 dB margin for 68-symbol pattern.
       Second-peak metric enables discrimination ratio check.  Downstream
       CRC-24Q acts as secondary gate.  Empirical Pfa = 0/1000 at the
       operational noise floor (Es/N0 = 0 dB) confirms threshold calibration.
   * - FM-059
     - SEU corrupts ``best_corr`` or ``second_best_corr`` accumulator
       during cross-correlation search (stack-resident float)
     - Peak metric reports wrong value; could cause false detection or
       missed detection depending on corruption direction; discrimination
       ratio (``second_peak``) unreliable
     - D-THRESHOLD: Threshold margin (50.0 vs 68.0 max) absorbs moderate
       corruption.  D-FRAME: downstream CRC-24Q rejects garbled data from
       false sync.  No direct detection of accumulator SEU.
     - S4
     - Zone 2
     - Residual risk accepted.  ``best_corr`` is a transient stack variable
       recomputed on every frame; corruption affects at most one 12-second
       frame.  TMR on the peak *offset* (FM-055) protects the safety-critical
       output (frame boundary); the metric is informational.  Mitigation:
       downstream CRC-24Q acts as secondary gate.


FMEA Table — RX Pipeline & Frame Decode (``frame_decode.cpp``, ``interleaver.cpp`` soft path)
-----------------------------------------------------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 5 25 20 15 8 12 15

   * - ID
     - Failure Mode
     - Effect on System
     - Detection Mechanism
     - Severity
     - FIZ Zone
     - Mitigation
   * - FM-060
     - SEU skips CFI milestone in ``frame_decode()`` (6-milestone counter)
     - One or more RX pipeline stages skipped; decoded navigation data incomplete
       or corrupted
     - D-SWIFI: CFI sentinel terminal check (``cfi_count != 0x7E7E7E7E + 6``)
       fires ``kFaultDetected``; FaultPacket emitted via telemetry HAL.
       Return-value injection FI-30 (forced sync fail) and FI-31 (forced LDPC
       fail) verify workspace scrubbing on each injected stage failure.
     - S3
     - Zone 1
     - ``volatile uint32_t cfi_count`` with unique magic (0x7E7E7E7E); milestone
       incremented after each stage (input validation, sync, BCH, deinterleave,
       LDPC, CRC); terminal check before ``kOk`` promotion.  All workspace
       temporaries scrubbed on failure.  Stage-level error handling verified by
       return-value injection (FI-30, FI-31).
   * - FM-061
     - SEU corrupts ``rx_symbols`` pointer causing aliasing with workspace
     - ``frame_decode()`` reads corrupted symbols while writing workspace; data
       integrity lost
     - D-SWIFI: ``spans_overlap()`` runtime guard at function entry.
     - S2
     - Zone 1
     - Pointer-range aliasing check between ``rx_symbols`` and ``workspace`` at
       entry; ``kFaultDetected`` returned if overlap detected.
   * - FM-062
     - LDPC decoder fails to converge on one or more subframes (channel noise).
       Decoder uses Layered Sum-Product BP (φ-transform, float64).
     - Subframe navigation data not decodable; single-frame outage for affected
       subframes
     - D-FRAME: ``ldpc_converged`` flag set per subframe; downstream CRC-24Q
       rejects unconverged output.  Overall status → ``kLdpcFailed``.
     - S3
     - Zone 2
     - Per-subframe convergence tracking.  Overall status demoted to
       ``kLdpcFailed`` if any subframe unconverged.  Workspace scrubbed.
       See FM-079, FM-080 for SP-specific numerical failure modes.
   * - FM-063
     - CRC-24Q verification fails on decoded subframe (residual bit errors)
     - Navigation data rejected; single-frame outage for affected subframe
     - D-FRAME: ``crc24_verify()`` returns false; output data buffer scrubbed
       for failing subframe.  Overall status → ``kCrcFailed``.
     - S3
     - Zone 2
     - CRC-24Q gate per subframe.  Failing data buffers scrubbed.  Status demoted
       to ``kCrcFailed``.
   * - FM-064
     - SEU corrupts index counter in ``block_deinterleave_soft()``
     - Soft-decision symbols permuted incorrectly; LDPC decoder receives garbled
       LLRs; all three subframes fail to converge
     - D-SWIFI: CFI sentinel + mirrored index accumulator detects counter
       corruption.
     - S3
     - Zone 1
     - ``in_idx_acc`` sequential accumulator cross-checked against computed index;
       ``cfi_count`` sentinel verified at loop exit.  On mismatch →
       ``kFaultDetected``; output scrubbed.
   * - FM-065
     - Sync pattern not detected (severe channel degradation or wrong frequency)
     - No frame boundary found; RX pipeline returns ``kSyncNotFound``; no
       navigation data available this epoch
     - D-FRAME: ``SyncDetector::detect()`` reports no detection; ``frame_decode``
       returns ``kSyncNotFound``.
       ``[e2e][rx]`` test ``kSyncNotFound then success — decoder is stateless``
       empirically validates that a failed decode call does not corrupt workspace
       or HAL state: a subsequent call on a valid frame returns ``kOk`` with
       correct TOI and FID recovered.
     - S3
     - Zone 2
     - Sync detection threshold (default 50.0) provides >11 dB margin.
       ``kSyncNotFound`` status propagated to caller for retry or outage handling.
       ``frame_decode()`` is stateless per-call: no persistent state between
       invocations; verified by ``[e2e][rx]`` test ``consecutive frames with
       incrementing TOI`` which decodes TOI=1, 2, 3 in independent calls with
       full data recovery on each.
   * - FM-066
     - ``strip_crc_copy()`` bounds violation (SEU corrupts size parameter)
     - Out-of-bounds read from message buffer or write to data buffer
     - D-SWIFI: Bounds validation in ``strip_crc_copy()`` returns false →
       ``kFaultDetected``.
     - S2
     - Zone 1
     - ``strip_crc_copy()`` validates both source and destination spans against
       ``n_data`` before any copy.  Returns false on violation; caller escalates
       to ``kFaultDetected``.
   * - FM-067
     - SEU corrupts ``FrameBuildWorkspace`` pointer causing aliasing with output
       frame buffer (``frame_build_partial`` / ``frame_build``)
     - Caller-provided staging buffer overlaps output frame; interleaver writes
       corrupt output; corrupted frame transmitted
     - D-SWIFI: ``spans_overlap()`` runtime guard at function entry detects
       overlap; ``ok = false``; frame scrubbed; ``kFaultDetected`` returned.
     - S2
     - Zone 1
     - Pointer-range aliasing check between ``workspace.staging`` and ``out``
       at entry.  Zero-fill and all processing guarded by ``if (ok)``.
       Staging unconditionally scrubbed on all exit paths (§7).
   * - FM-068
     - SEU corrupts ``LoopFilter1::integrator`` (DLL integrator runaway)
     - Code phase drifts uncontrollably → wrong pseudoranges → incorrect
       navigation solution
     - D-SWIFI: ``std::clamp`` on integrator every update cycle
       (±\ ``kMaxDllIntegratorChipsPerS``); C/N0 lock detection transitions
       channel to ``kLost`` on sustained divergence.
     - S3
     - Zone 1-2
     - Integrator clamped to ±100 chips/s every ``update()`` call.
       Mirrors the loop-filter integrator saturation pattern.
       Second-order PI filter (proportional + integral) enables zero
       steady-state error for constant-rate SCO drift.
       SWIFI FI-26 (``test_swifi_signal.cpp``) injects 10x saturation
       value and verifies clamp + tracking recovery.
       Empirical: SCO parametric sweep 0--50 ppm × 200 epochs
       (``test_channel_impairments.cpp``), lock maintained at all points.
       Steady-state DLL bias < 0.10 chips at 10 ppm (200 epochs).
       Combined impairment test (SCO + phase noise + data + quantization +
       multipath + Doppler rate) maintains lock at Ec/N0 = 10 dB.


   * - FM-075
     - SEU corrupts ``power_accum_`` during non-coherent integration
     - Accumulated power grid contains stale/corrupt values → incorrect
       detection metric → false acquisition lock or missed signal
     - D-CFI: ``scrub_float_array()`` (secure_scrub via volatile) at entry
       and exit of ``pcps_search()``; CFI sentinel on epoch accumulation
       loop (``cfi_epochs = 0xC3C3C3C3``); CFI sentinel on peak-search
       loop (``cfi_peak = 0xD2D2D2D2``).
     - S3
     - Zone 1-2
     - ``power_accum_`` zeroed via ``secure_scrub()`` before each Doppler
       bin and after the search completes.  Epoch loop CFI detects skipped
       iterations.  Peak-search CFI detects truncated scan.  Variance
       computed in ``double`` to prevent catastrophic cancellation.
   * - FM-076
     - SEU corrupts ``n_epochs`` parameter between guard clause and loop
     - Epoch loop runs wrong number of iterations → incorrect integration
       depth → metric does not match threshold calibration
     - D-CFI: ``n_epochs`` is ``uint8_t`` passed by value (register);
       ``cfi_epochs`` terminal check detects iteration count mismatch.
       Guard clause validates ``n_epochs ≤ kMaxIntegrationEpochs`` before
       ``uint8_t`` truncation.
     - S3
     - Zone 1
     - ``n_epochs_unchecked`` validated as ``size_t`` before cast.
       CFI sentinel verifies actual iterations match expected.
   * - FM-077
     - SEU causes ``doppler_wipeoff()`` to skip samples
     - Partial Doppler removal → smeared correlation peak →
       degraded acquisition sensitivity or false code delay
     - D-CFI: ``cfi_wipe = 0xE1E1E1E1`` sentinel in ``doppler_wipeoff()``;
       terminal check returns false on mismatch; caller
       ``accumulate_epochs()`` scrubs and returns false on failure.
     - S3
     - Zone 1
     - CFI protects the inner wipe-off loop.  On CFI failure, both
       ``scratch_`` and ``power_accum_`` are scrubbed before return.
   * - FM-078
     - Variance computation produces negative value due to floating-point
       cancellation → ``σ = 1e-10F`` fallback → inflated metric → false lock
     - False acquisition on noise → incorrect tracking initialisation →
       wrong pseudoranges
     - D-NUM: Variance accumulated in ``double`` precision to eliminate
       catastrophic cancellation for all practical SNR ranges.
       Fallback ``σ = 1e-10F`` produces metric ~ ``(peak-mean)/1e-10``,
       which would exceed any reasonable threshold only if ``peak > mean``
       by a significant margin (genuine signal present).
     - S3
     - Zone 2
     - ``double`` accumulation provides 15+ significant digits vs 7 for
       ``float``.  At ``kPcpsFftSize=4096`` cells and ``N≤20`` epochs,
       the maximum ``sum_pow2`` remains well within ``double`` range.
   * - FM-079
     - ``phi_func()`` NaN/Inf propagation — ``expm1()`` returns 0 for
       extreme input magnitudes, causing division by zero in
       ``log1p(1.0/expm1(x))``
     - LDPC decoder produces NaN LLRs; all downstream parity checks fail;
       subframe data unrecoverable
     - D-NUM: Input clamped to ``|x| >= 1e-10`` and ``|x| <= 30`` before
       ``expm1()`` call.  Output bounded to [0, 30].  No division by zero
       possible after clamping.
     - S3
     - Zone 1
     - ``phi_func()`` clamps input: values below ``1e-10`` map to the upper
       bound (30.0); values above 30 map to 0.0.  Clamping eliminates the
       ``expm1() == 0`` singularity.  Post-clamping output is always finite
       and bounded.
   * - FM-080
     - ``phi_sum`` accumulation overflow — unbounded ``double`` addition in
       check-node update produces +Inf, propagating to variable-node LLRs
     - LDPC decoder diverges; subframe navigation data lost
     - D-NUM: ``phi_func()`` output is bounded [0, 30] per FM-079 clamping;
       maximum row weight in LSIS-AFS parity matrix is 8; worst-case
       ``phi_sum`` is 8 × 30 = 240, well within ``double`` range.
     - S3
     - Zone 1
     - Bounded ``phi_func()`` output ensures ``phi_sum`` cannot overflow.
       Maximum accumulation is ``row_weight × 30.0``.

..
   Gateway 6 — Message Parsing

   * - FM-069
     - SEU corrupts bit array during ``extract_bits()`` — bit value > 1
     - Wrong WN/ITOW extracted from SB2; incorrect Time of Transmission
       computed; navigation solution uses wrong time reference
     - D-CFI: ``extract_bits()`` validates each bit value (must be 0 or 1);
       CFI sentinel verifies all milestones completed; output scrubbed on
       any failure path.
     - S2
     - Zone 1
     - Bit-value validation rejects any non-binary value immediately.
       CFI terminal check (2 milestones) detects skipped logic.
       Output ``uint32_t& out`` zeroed on error.
   * - FM-070
     - SEU corrupts SB3/SB4 type field — maps to wrong message handler
     - Navigation data interpreted as wrong message type; downstream
       processing uses incorrect field layout
     - D-CFI: ``parse_sb3()``/``parse_sb4()`` extract type field via
       ``extract_bits()`` (validated); ``type_field_to_message_type()``
       maps unrecognised values to ``kUnknown``; CFI sentinel verifies
       extraction pipeline integrity.
     - S3
     - Zone 2
     - Unknown type field values produce ``MessageType::kUnknown`` with
       ``kOk`` status (not an error — sparse type space is expected per
       spec).  Caller must check ``result.type`` before routing.
       CFI terminal check (2 milestones) detects skipped extraction.
   * - FM-071
     - SEU corrupts WN/ITOW values after extraction in ``parse_sb2()``
     - Time of Transmission computed from corrupted week/interval values;
       navigation solution has incorrect time reference
     - D-CFI: ``parse_sb2()`` CFI sentinel verifies 2 milestones (input
       validation + field extraction); result scrubbed on failure.
     - S2
     - Zone 1
     - CFI terminal check detects skipped extraction stages.
       ``secure_scrub_scalar()`` on ``result.wn`` and ``result.itow``
       on any non-``kOk`` path.
   * - FM-072
     - ``compute_tot()`` arithmetic overflow or SEU-corrupted input
     - Time of Transmission has wrong value; navigation solution uses
       incorrect time reference
     - D-CFI: Range validation (WN ≤ 8191, ITOW ≤ 503, TOI ≤ 99)
       at entry; CFI sentinel verifies 2 milestones; ``result.t_f``
       scrubbed on failure.
     - S2
     - Zone 1
     - Input validation rejects out-of-range values with specific
       status codes.  Double-precision arithmetic avoids uint32_t
       overflow.  CFI terminal check ensures formula execution.
       ``secure_scrub_scalar()`` on ``result.t_f`` on non-``kOk`` path.

..
   Gateway 7 — SISE Calculation

   * - FM-073
     - ``compute_sise()`` receives NaN/Inf position or velocity components
       (SEU corrupts caller's double registers)
     - Erroneous SISE metric; downstream validation uses wrong error value
     - D-CFI: ``is_finite()`` guard on all 12 input components at entry;
       post-condition check on RSS output finiteness; CFI sentinel verifies
       2 milestones; ``secure_scrub_scalar()`` on both output fields on failure.
       D-SWIFI: FI-37.
     - S2
     - Zone 1
     - Input validation rejects non-finite components with ``kInvalidInput``.
       Post-condition guards against intermediate overflow (component
       differences near ``DBL_MAX``; impossible in cislunar navigation where
       coordinates are bounded to ~5×10⁸ m).  CFI terminal check ensures
       both computation stages executed.  Both ``position_error_m`` and
       ``velocity_error_mps`` scrubbed to zero on any non-``kOk`` path.
   * - FM-074
     - ``compute_sise_percentile()`` receives null pointer, empty array,
       or array containing NaN/Inf values
     - Segfault, undefined percentile result, or incorrect pass/fail verdict
     - D-CFI: Null check, size check, NaN/Inf scan, and percentile range
       validation at entry; CFI sentinel verifies 2 milestones;
       ``secure_scrub_scalar()`` on both output fields and ``false`` on
       both pass flags on failure.  D-SWIFI: FI-38.
     - S2
     - Zone 1
     - ``validate_percentile_inputs()`` rejects null pointers (``kInvalidInput``),
       empty arrays (``kEmptyData``), out-of-range percentile, and non-finite
       array elements.  ``nth_element`` operates on a copy (no aliasing).
       CFI terminal check ensures percentile computation executed.
       All output fields scrubbed on any non-``kOk`` path.

Open FMEA Items
---------------

The following failure modes are identified as requiring further analysis and are
tracked as open items.  They are not currently mitigated by a software-layer control.

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - ID
     - Open Item
     - Disposition
   * - OPEN-001
     - Multi-bit SEU (word-level upset) in LDPC CSR matrix — current CRC detects
       this but does not correct it; recovery requires ground command
     - Accepted risk.  Mitigation: CBIT scrub frequency to be sized against
       expected multi-bit SEU rate (TBD from radiation environment model).
   * - OPEN-002
     - Latch-up event in processor — software cannot detect or recover; requires
       hardware power cycling
     - Out of scope for software FMEA.  Handled at system level by OBC health
       management and power controller.
   * - OPEN-003
     - FM-016 residual risk: if variable-mirror detection fails (two correlated
       SEUs hit both mirror and primary simultaneously), CRC-24 corruption is
       undetected at software level
     - Residual risk accepted.  Probability assessed as negligible (two independent
       SEUs in the same function call window).  System-level CRC at transport
       layer provides final defence.


Standards Traceability
----------------------

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - ECSS Reference
     - NPR Cross-Ref
     - Evidence
   * - Q-ST-80C §5.5; method per Q-HB-80-03A Rev.1 (software FMEA)
     - SWE-028
     - All safety-critical software functions enumerated with failure modes and
       effects (full FMEA table above)
   * - Q-ST-80C §5.5; method per Q-HB-80-03A Rev.1 (detection & containment)
     - SWE-029
     - Fault detection methods and containment zones specified per failure mode;
       cross-referenced to FIZ architecture in ``mission_assurance.rst``
   * - ECSS-E-ST-40C Rev.1 §5.5.3 (unit testing)
     - SWE-127
     - SWIFI column in every FM entry cross-references the empirical detection
       evidence required for unit test coverage of fault paths
