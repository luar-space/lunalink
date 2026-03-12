Software FMEA
=============

This Software Failure Modes and Effects Analysis (FMEA) is the formal companion to the
Fault Containment Architecture described in ``mission_assurance.rst``.  Where that document
specifies *how* LunaLink responds to faults, this document analyses *what can go wrong*,
*what happens to the system if it does*, and *how the fault is detected and contained*.

The scope is all safety-critical software functions in ``cpp/signal/``.  Hardware failure
modes (power supply, oscillator, memory DRAM/SRAM) are outside scope and are treated
as inputs to this analysis via the system-level hazard analysis.

**Standard References**: NPR 7150.2D SWE-028/029, MIL-STD-1629A, ECSS-Q-ST-30C.

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


NPR 7150.2D SWE Traceability
-----------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - SWE
     - Addressed by this document
   * - SWE-028
     - All safety-critical software functions enumerated with failure modes and
       effects (full FMEA table above)
   * - SWE-029
     - Fault detection methods and containment zones specified per failure mode;
       cross-referenced to FIZ architecture in ``mission_assurance.rst``
   * - SWE-127
     - SWIFI column in every FM entry cross-references the empirical detection
       evidence required by SWE-127 (unit test coverage of fault paths)
