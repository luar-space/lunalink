Software FMEA
=============

**Document ID**: LUNALINK-FMEA-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.5 (dependability); method per ECSS-Q-HB-80-03A Rev.1; ECSS-Q-ST-30C; MIL-STD-1629A; NPR 7150.2D SWE-028/029 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

This Software Failure Modes and Effects Analysis (FMEA) is the formal companion to the
Fault Containment Architecture described in ``mission_assurance.rst``.  Where that document
specifies *how* LunaLink responds to faults, this document analyses *what can go wrong*,
*what happens to the system if it does*, and *how the fault is detected and contained*.

The scope is all safety-critical software functions in ``cpp/signal/``.  Hardware failure
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
       JIT integrity check before each encode pass.
     - S2
     - Zone 3
     - CRC checked at POST, by CBIT background task, and by JIT integrity check
       immediately before every encode call.  Zone 3 fault on mismatch.
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
       SWIFI test ``watchdog_lower_bound_trip`` verifies detection empirically.
     - S2
     - Zone 3
     - **Implemented** — ``WindowedWatchdog`` class in ``watchdog.hpp``; lower-bound
       trip emits ``FaultPacket(fiz_level=2)``; SWIFI test
       ``watchdog_lower_bound_trip`` passes.  WCET-derived lower and upper
       thresholds documented in ``assurance/wcet_profile.rst``.  Lower-bound trip
       treated as Critical Fault; Zone 3 response.
   * - FM-034
     - CFI binary identity (SCI header) corrupt at boot
     - Unknown software version executing; traceability broken; may be wrong build
     - D-POST: SCI header CRC verified as first POST step.
     - S2
     - Zone 3
     - SCI header CRC checked as step 1 of POST.  Mismatch → boot abort; no
       transmission; telemetry fault; awaits ground command.


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


FMEA Table — Software Correlator (``correlator.cpp``)
------------------------------------------------------

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
   * - FM-050
     - SEU corrupts channel state TMR register (e.g., kTracking → undefined)
     - Channel operates in wrong mode; correlator may process acquisition data
       as tracking or vice versa; incorrect pseudoranges
     - D-SWIFI: TMR majority vote on ``ch.state`` detects single-copy corruption;
       illegal transition matrix catches invalid state values.
     - S2
     - Zone 1
     - ``TmrValue<uint8_t>`` on channel state; majority vote before every state
       read; high Hamming distance enum values (≥3 bits apart); illegal transition
       → telemetry fault packet + ``kFaultDetected``.
   * - FM-051
     - PCPS acquisition false lock (peak/mean metric passes threshold on noise)
     - Channel transitions to tracking with wrong code phase and Doppler;
       pseudoranges systematically wrong until C/N0 lock detection triggers
     - D-CBIT: C/N0 estimation in ``track()`` detects low signal quality
       (< 25 dB-Hz) within one code period; channel transitions to kLost.
     - S3
     - Zone 2
     - Threshold set to 15.0 (Pfa ≈ 10⁻⁶ for N=2048 Gaussian noise).
       C/N0-based lock detection in tracking loop provides secondary gate;
       channel declared lost if C/N0 < ``kMinCn0DbHz``.
   * - FM-052
     - PLL integrator runaway (SEU corrupts loop filter integrator state)
     - Doppler estimate diverges; carrier phase wraps uncontrollably;
       tracking loop becomes unstable; wrong pseudoranges
     - D-CBIT: Integrator saturation clamp (``std::clamp`` at
       ±\ ``kMaxPllIntegratorRadS``); C/N0 lock detection.
     - S3
     - Zone 1–2
     - ``LoopFilter2::integrator`` clamped to ±2× Doppler range on every
       update.  C/N0 estimation detects divergence; channel declared lost.
   * - FM-053
     - SEU skips loop iteration in ``prepare_code_replica`` or ``correlate_epl``
     - Code replica or E/P/L correlator output truncated; wrong acquisition
       or tracking result
     - D-SWIFI: CFI sentinel counters in both functions; terminal check
       fires ``kFaultDetected`` on count mismatch.
     - S3
     - Zone 1
     - ``uint32_t cfi_count`` with unique magic per function
       (0xA5A5A5A5 for prepare, 0xC3C3C3C3 for EPL, 0xB4B4B4B4 for PCPS);
       terminal check ``cfi_count != magic + N`` → output scrubbed +
       ``kFaultDetected``.
   * - FM-054
     - SEU corrupts code phase float to NaN/Inf during tracking
     - Code phase becomes non-finite; all subsequent pseudoranges invalid;
       correlate_epl produces garbage
     - D-SWIFI: ``std::isfinite()`` guard after code phase update.
     - S3
     - Zone 1
     - ``std::isfinite(code_phase_chips)`` checked after every DLL update.
       On failure: channel transitions to kLost; ``kFaultDetected`` returned.
       Bounded wrapping via ``std::fmod`` replaces unbounded ``while`` loop.


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
