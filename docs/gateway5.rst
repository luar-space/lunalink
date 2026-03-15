Gateway 5: Frame Sync & Decoding --- Verification Guide
========================================================

This document maps each Gateway 5 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs all unit tests (< 10 s)
   task swifi                 # runs SWIFI fault-injection suite (25 scenarios)

All tests must pass with exit code 0.

----

GD5.1 --- Software Correlator (Acquisition / Tracking)
------------------------------------------------------

Spec reference: LSIS-AFS-701 through LSIS-AFS-704

The software correlator implements PCPS (Parallel Code Phase Search)
acquisition and DLL/PLL tracking for AFS-I Gold-2046 spreading codes.

Implementation files:

-  ``cpp/include/lunalink/signal/correlator.hpp`` --- public API, types,
   loop filters, channel struct
-  ``cpp/signal/correlator.cpp`` --- PCPS search, E/P/L correlator,
   tracking loops, pseudorange computation

Architecture
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Component
     - Description
   * - PCPS Acquisition
     - 2048-pt FFT-based parallel code phase search; circular
       cross-correlation of received signal with local code replica;
       Doppler search over |pm|\ 10 kHz in 500 Hz bins
   * - DLL
     - First-order loop filter (1 Hz bandwidth); Early-Minus-Late
       discriminator with 0.5-chip spacing
   * - PLL
     - Second-order loop filter (15 Hz bandwidth); Costas discriminator;
       integrator saturation at |pm|\ 2\ |x|\ Doppler range
   * - Doppler Prior
     - Pluggable interface (``DopplerPrior``); ``UniformPrior`` shipped;
       ML-predicted narrowing is an extension point
   * - Channels
     - 6 simultaneous channels; per-channel TMR state; pseudorange ring
       buffer (10-deep)

.. |pm| unicode:: U+00B1
.. |x| unicode:: U+00D7

API
~~~

.. code-block:: cpp

   class Correlator {
   public:
       explicit Correlator(const DopplerPrior& prior,
                           ITelemetryHal& hal) noexcept;

       [[nodiscard]] CorrelatorStatus assign_channel(
           uint8_t ch_idx, PrnId prn_id) noexcept;

       [[nodiscard]] CorrelatorStatus acquire(
           uint8_t ch_idx,
           std::span<const std::complex<float>> rx,
           AcquisitionResult& result) noexcept;

       [[nodiscard]] CorrelatorStatus track(
           uint8_t ch_idx,
           std::span<const std::complex<float>> rx) noexcept;

       [[nodiscard]] CorrelatorStatus pseudorange(
           uint8_t ch_idx, double& out_meters) const noexcept;
   };

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - TMR channel state
     - ``TmrValue<uint8_t>`` on ``ChannelState``; majority vote on every
       state read; active repair on mismatch
   * - Legal transition matrix
     - ``constexpr TransitionMatrix<ChannelState, 8>``; Idle |rarr|
       Acquiring |rarr| Tracking |rarr| Lost |rarr| Idle; illegal edges
       rejected with telemetry
   * - CFI sentinels
     - 3 independent sentinels: ``prepare_code_replica`` (0xA5A5A5A5),
       ``pcps_search`` (0xB4B4B4B4), ``correlate_epl`` (0xC3C3C3C3)
   * - PLL integrator saturation
     - ``std::clamp`` at |pm|\ ``kMaxPllIntegratorRadS``; prevents runaway
   * - C/N0 lock detection
     - Channel declared lost if C/N0 < 25 dB-Hz; prevents false lock
   * - NaN/Inf guard
     - ``std::isfinite()`` on code phase after DLL update; kLost on failure
   * - Bounded code phase wrapping
     - ``std::fmod`` replaces unbounded ``while`` loop
   * - SEU-resistant status codes
     - ``CorrelatorStatus`` and ``ChannelState`` use high Hamming distance

.. |rarr| unicode:: U+2192

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/test_correlator.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - FFT forward/inverse recovery
     - Round-trip preserves input within float tolerance
   * - Channel starts in kIdle
     - Default state after construction
   * - assign_channel validates index
     - Out-of-range returns ``kInvalidInput``
   * - Acquire locks on clean signal
     - PCPS peak at correct code delay; Doppler = 0 Hz
   * - Acquire locks with Doppler offset
     - +2000 Hz Doppler estimated within 500 Hz
   * - Acquisition fails on noise-only
     - Returns ``kAcqFailed``; ``locked == false``
   * - Narrowed prior fallback
     - Wrong prior falls back to uniform search
   * - Track updates pseudorange
     - 5 tracking epochs produce valid pseudorange
   * - Multiple channels independent
     - Two PRNs acquired simultaneously
   * - Out-of-range guards
     - Safe defaults for all accessors with bad index

SWIFI scenarios (``cpp/tests/test_fault_injection.cpp``):

-  **FI-16**: Channel state TMR vote corrects single-copy corruption
-  **FI-17**: Illegal transition (kIdle |rarr| kTracking) returns ``kNotLocked``
-  **FI-18**: CFI nominal --- all sentinels pass under clean signal
-  **FI-19**: TMR adversarial --- track survives single-copy state corruption

----

GD5.2 --- Sync Pattern Detector
--------------------------------

Spec reference: LSIS-AFS-710

The sync pattern detector searches a buffer of received soft symbols for
the 68-symbol LSIS synchronisation pattern (Table 12) using bipolar
cross-correlation.

Implementation files:

-  ``cpp/include/lunalink/signal/sync_detector.hpp`` --- public API
-  ``cpp/signal/sync_detector.cpp`` --- cross-correlation, CFI, TMR

Detection algorithm
~~~~~~~~~~~~~~~~~~~

1. Precompute bipolar reference from ``kSyncPattern``: bit 0 |rarr| +1.0,
   bit 1 |rarr| |minus|\ 1.0
2. Slide a 68-symbol window across the input buffer
3. At each offset, compute the dot product with the bipolar reference
4. Track best and second-best peaks (TMR on best offset)
5. Declare detection if best peak |geq| threshold (default = 50.0)

.. |minus| unicode:: U+2212
.. |geq| unicode:: U+2265

API
~~~

.. code-block:: cpp

   class SyncDetector {
   public:
       explicit SyncDetector(ITelemetryHal& hal) noexcept;
       [[nodiscard]] bool set_threshold(float threshold) noexcept;

       [[nodiscard]] SyncDetectorStatus detect(
           std::span<const float> rx_symbols,
           SyncResult& result) noexcept;
   };

   struct SyncResult {
       uint16_t frame_offset;
       float    peak_metric;
       float    second_peak;
       bool     detected;
   };

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - CFI sentinels
     - 3-milestone counter (0x7E7E7E7E): input validation, search
       complete, result populated; terminal check before ``kOk``
   * - TMR peak tracking
     - ``TmrValue<uint16_t>`` on best offset; majority vote before result
   * - WIP ticks
     - ``wip_tick()`` in search loop; proves progress to watchdog
   * - SEU-resistant status codes
     - ``SyncDetectorStatus``: kOk=0x5A, kInvalidInput=0xA5,
       kNotDetected=0x33, kFaultDetected=0xCC (Hamming distance |geq| 4)
   * - Telemetry on fault
     - ``FaultPacket`` emitted via HAL on CFI violation
   * - Pessimistic default
     - Status initialised to ``kFaultDetected``; promoted only on CFI pass
   * - Secure scrub
     - ``SyncResult`` zeroed on fault path

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/test_sync_detector.cpp``, 13 tests)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - Rejects empty input
     - ``kInvalidInput`` on zero-length span
   * - Rejects input < 68 symbols
     - ``kInvalidInput`` on 67-symbol span
   * - Accepts exactly sync length
     - Perfect match gives peak = 68.0
   * - Detects sync at start of frame
     - ``frame_offset == 0``; ``peak_metric == 68.0``
   * - Detects sync at arbitrary offset
     - Offset 1234 in 7000-symbol stream
   * - Detects sync at end of buffer
     - Last valid position
   * - Detects sync in moderate noise
     - Sigma = 0.5; peak above threshold
   * - Fails on noise-only (no sync)
     - ``kNotDetected``; peak below threshold
   * - Custom threshold affects detection
     - Threshold 69 rejects perfect match; threshold 60 accepts
   * - Second peak < peak metric
     - Discrimination ratio
   * - CFI fault emits telemetry
     - CaptureTelemetryHal records zero packets on clean run
   * - All-zeros input
     - ``kNotDetected`` (all-zero float is 0.0, not bipolar)
   * - All-ones (bipolar +1.0) input
     - ``kNotDetected`` (partial correlation only)

SWIFI scenarios (``cpp/tests/test_fault_injection.cpp``):

-  **FI-20**: CFI adversarial --- ``inject_cfi_skip(1)`` fires ``kFaultDetected``
   + FaultPacket; output scrubbed
-  **FI-21**: TMR adversarial --- ``inject_tmr_fault(0, 9999)`` corrupts one
   copy; ``voted_peak_offset()`` verifies majority vote returns 500
-  **FI-22**: CFI recovery --- clean detection succeeds after injected fault;
   one-shot injection verified

----

Compliance Cross-Reference
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 55 30

   * - ID
     - Requirement
     - Evidence
   * - LSIS-AFS-701
     - PCPS acquisition for AFS-I Gold-2046
     - ``correlator.cpp`` + ``test_correlator.cpp``
   * - LSIS-AFS-702
     - Channel assignment with PRN validation
     - ``correlator.cpp`` + ``test_correlator.cpp``
   * - LSIS-AFS-703
     - Pseudorange from tracking state
     - ``correlator.cpp`` + ``test_correlator.cpp``
   * - LSIS-AFS-710
     - Sync pattern detection via 68-symbol cross-correlation
     - ``sync_detector.cpp`` + ``test_sync_detector.cpp``

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 20 25

   * - Suite
     - Tests
     - Assertions
     - Run time
     - How to run
   * - GD5.1 unit tests
     - 18
     - ~180
     - < 2 s
     - ``task test-cpp``
   * - GD5.2 unit tests
     - 13
     - ~36
     - < 5 s
     - ``task test-cpp``
   * - SWIFI (GD5 scenarios)
     - 7
     - ~50
     - < 2 s
     - ``task swifi``

All tests pass with exit code 0.

Related documents:

-  :doc:`signal/compliance_matrix` --- LSIS-AFS-701, LSIS-AFS-710
-  :doc:`signal/architecture` --- module-to-gateway mapping
-  :doc:`signal/icd` --- formal interface control document
-  :doc:`assurance/mission_assurance` --- CFI, TMR, scrubbing patterns
-  :doc:`assurance/fmea` --- correlator and sync detector failure modes
-  :doc:`assurance/fault_coverage` --- SWIFI scenario catalogue
