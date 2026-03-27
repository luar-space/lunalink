Gateway 4: Baseband Signal Generation --- Verification Guide
=============================================================

This document maps each Gateway 4 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs all unit tests (< 2 s)
   task test-py               # runs Python binding tests
   task swifi                 # runs SWIFI fault-injection suite

All tests must pass with exit code 0.

----

GD4.1 --- BPSK Modulator (AFS-I + AFS-Q)
-----------------------------------------

Spec reference: LSIS-AFS-201

The modulator maps binary chip sequences {0, 1} to antipodal BPSK samples
{+1, -1} and optionally multiplies by a data symbol.  Two channel variants
are provided:

-  **AFS-I** (``modulate_bpsk_i``): BPSK(1) at 1.023 Mcps, modulated by a
   navigation data symbol d |isin| {-1, +1}
-  **AFS-Q** (``modulate_bpsk_q``): BPSK(5) at 5.115 Mcps, pilot channel
   (data symbol fixed to +1)

.. |isin| unicode:: U+2208

Implementation files:

-  ``cpp/include/lunalink/signal/modulator.hpp`` --- public API (inlined hot path)
-  ``cpp/signal/modulator.cpp`` --- validation helpers

Modulation formula
~~~~~~~~~~~~~~~~~~

.. code-block:: text

   sample[i] = (1 - 2 * chip[i]) * data_symbol

This branchless arithmetic maps chip 0 |rarr| +1 and chip 1 |rarr| -1,
then applies the data symbol sign.  Zero conditional branches in the hot
loop ensures constant-time execution regardless of input values.

.. |rarr| unicode:: U+2192

API
~~~

.. code-block:: cpp

   // AFS-I: BPSK(1) with navigation data symbol
   [[nodiscard]] ModulationStatus modulate_bpsk_i(
       std::span<const uint8_t> prn_chips,
       int8_t data_symbol,
       std::span<int8_t> out) noexcept;

   // AFS-Q: BPSK(5) pilot (data_symbol = +1)
   [[nodiscard]] ModulationStatus modulate_bpsk_q(
       std::span<const uint8_t> chips,
       std::span<int8_t> out) noexcept;

Input validation:

-  ``data_symbol`` must be exactly +1 or -1 (``kInvalidSymbol`` otherwise)
-  All chip values must be 0 or 1 (``kInvalidChipValue`` otherwise)
-  Output buffer must be |geq| input length (``kOutputTooSmall`` otherwise)
-  Input/output buffers must not overlap (``kFaultDetected`` with scrub)

.. |geq| unicode:: U+2265

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - Branchless arithmetic
     - ``(1 - 2 * chip) * data_symbol`` --- zero conditional branches in
       hot loop; constant-time execution prevents timing side channels
   * - SEU-resistant status codes
     - ``ModulationStatus`` values use non-contiguous bit patterns with
       Hamming distance |geq| 2 (``kOk`` = 0x5A, ``kFaultDetected`` = 0x99)
   * - Aliasing protection
     - ``spans_overlap()`` runtime check before computation; returns
       ``kFaultDetected`` and scrubs output if input/output buffers overlap
   * - Stack scrubbing
     - ``secure_scrub(out)`` via volatile pointer on all error paths;
       prevents partial or stale data leakage
   * - Static cast discipline
     - Explicit ``static_cast`` on all ``uint8_t`` |rarr| ``int`` promotions
       (GCC ``-Wsign-conversion -Werror`` safe)

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/signal/test_modulator.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - modulate_bpsk_i chip mapping with +1 symbol
     - Chips {0, 1} |rarr| {+1, -1} with data = +1
   * - modulate_bpsk_i chip mapping with -1 symbol
     - Chips {0, 1} |rarr| {-1, +1} with data = -1
   * - modulate_bpsk_i full PRN 1 all values in {-1, +1}
     - Full Gold-2046 PRN produces only {-1, +1} samples
   * - modulate_bpsk_i returns explicit error status
     - Output buffer too small triggers ``kOutputTooSmall``
   * - modulate_bpsk_i rejects invalid data symbol
     - ``data_symbol = 0`` rejected with ``kInvalidSymbol``
   * - modulate_bpsk_i rejects non-binary chips
     - Chip value = 2 rejected with ``kInvalidChipValue``

----

GD4.2 --- IQ Multiplexer (5.115 MSPS)
--------------------------------------

Spec reference: LSIS-AFS-301

The IQ multiplexer combines the AFS-I and AFS-Q modulated signals into a
single interleaved baseband stream at the AFS-Q chip rate (5.115 MSPS).
AFS-I samples are upsampled by a factor of 5 to match AFS-Q.

Implementation files:

-  ``cpp/include/lunalink/signal/iq_mux.hpp`` --- public API and constants
-  ``cpp/signal/iq_mux.cpp`` --- multiplexer implementation

Signal dimensions
~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 20 55

   * - Parameter
     - Value
     - Description
   * - AFS-I input
     - 2046 chips
     - Gold-2046, 1 sample per chip
   * - AFS-Q input
     - 10230 chips
     - Weil-10230, 1 sample per chip
   * - Upsampling factor
     - 5\ |x|
     - Each I chip repeated 5 times to match Q rate
   * - Output rate
     - 5.115 MSPS
     - Combined baseband sample rate
   * - Output per epoch
     - 20,460 int16_t
     - Interleaved [I, Q, I, Q, ...] pairs
   * - Power sharing
     - 50/50
     - I and Q at equal unit amplitude {|pm|\ 1}

.. |x| unicode:: U+00D7
.. |pm| unicode:: U+00B1

API
~~~

.. code-block:: cpp

   [[nodiscard]] IqMuxStatus multiplex_iq(
       std::span<const int8_t> i_samples,    // 2046 BPSK samples
       std::span<const int8_t> q_samples,    // 10230 BPSK samples
       std::span<int16_t> out) noexcept;     // 20460 interleaved I/Q

Input validation:

-  I samples: |geq| 2046 elements, all values in {-1, +1}
-  Q samples: |geq| 10230 elements, all values in {-1, +1}
-  Output: |geq| 20460 elements
-  Sample value check: ``sample * sample == 1`` (branchless {-1, +1} test)

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - Control Flow Integrity
     - Loop counter incremented every iteration; terminal check verifies
       ``loop_count == kIqSamplesPerEpoch``; returns ``kFaultDetected``
       on mismatch (detects loop skip, early exit, or corruption)
   * - SEU-resistant status codes
     - ``IqMuxStatus`` values use non-contiguous bit patterns
       (``kOk`` = 0x5A, ``kFaultDetected`` = 0x99, etc.)
   * - Aliasing protection
     - ``spans_overlap()`` runtime check on all input/output buffer pairs
   * - Watchdog integration
     - ``wip_tick()`` called once per loop iteration; feeds windowed watchdog
       HAL to prove forward progress
   * - Stack scrubbing
     - ``secure_scrub(out)`` on all error paths
   * - Branchless sample validation
     - ``sample * sample != 1`` rejects any value outside {-1, +1}
       without branching on the value itself

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/signal/test_iq_mux.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - multiplex_iq upsamples I by factor 5
     - Every I chip appears 5 consecutive times in output
   * - multiplex_iq Q channel passes through at chip rate
     - Q samples appear at every odd interleaved position
   * - multiplex_iq output length is 2 |x| 10230 interleaved
     - Output dimension = 20,460 int16_t samples
   * - multiplex_iq 50/50 power: I and Q have equal amplitude
     - Both channels at unit amplitude {|pm|\ 1}
   * - multiplex_iq rejects undersized inputs and outputs
     - All three dimension checks produce correct error status
   * - multiplex_iq rejects invalid sample values
     - Samples outside {-1, +1} rejected with per-channel error codes

----

Cross-Cutting: Integration and Assurance Tests
-----------------------------------------------

Coherency test (``cpp/tests/signal/test_coherency.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Verifies primary/secondary/tertiary phase alignment (LSIS-172) over a full
6000-epoch frame.  This is the upstream prerequisite for GD4: if matched-code
epochs are phase-incorrect, the modulated IQ stream carries the error.

Round-trip test (``cpp/tests/signal/test_roundtrip.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The TX round-trip encodes through the full signal chain: matched-code epoch
generation (G1) |rarr| BPSK modulation (G4) |rarr| IQ multiplexing (G4),
verifying that the complete baseband output is reconstructable.

SWIFI fault injection (``cpp/tests/signal/test_fault_injection.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Scenario
     - What it verifies
   * - aliasing_overlap_modulator
     - In-place aliasing (chips buffer = output buffer) returns
       ``kFaultDetected`` and scrubs output before any write
   * - aliasing_overlap_iq_mux
     - Overlapping I/output buffers detected; ``kFaultDetected`` returned
       with scrubbed output

----

Python Bindings
---------------

All GD4 components are exposed to Python via ``lunalink.afs``.

.. list-table::
   :header-rows: 1
   :widths: 25 40 35

   * - Function
     - Signature
     - Notes
   * - ``modulate_i``
     - ``(prn: GoldCode, data: int) -> BpskChipsI``
     - AFS-I BPSK(1) with data symbol
   * - ``modulate_q``
     - ``(chips: WeilPrimary) -> BpskChipsQ``
     - AFS-Q BPSK(5) pilot
   * - ``multiplex_iq``
     - ``(i: BpskChipsI, q: BpskChipsQ) -> IqMuxNative``
     - Interleaved IQ at 5.115 MSPS

Python tests: ``task test-py`` (file: ``tests/test_afs.py``)

----

Compliance Cross-Reference
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 55 30

   * - ID
     - Requirement
     - Evidence
   * - LSIS-101
     - AFS-I BPSK(1) modulation at 1.023 Mcps
     - ``modulate_bpsk_i`` + ``test_modulator.cpp``
   * - LSIS-102
     - AFS-Q BPSK(5) modulation at 5.115 Mcps
     - ``modulate_bpsk_q`` + ``test_modulator.cpp``
   * - LSIS-103
     - 50/50 power sharing between I and Q
     - ``multiplex_iq`` + ``test_iq_mux.cpp`` (equal amplitude test)
   * - LSIS-160
     - Align data symbols with primary code boundaries
     - ``modulate_bpsk_i`` epoch-aligned API
   * - LSIS-171
     - Align tertiary code with secondary code boundaries
     - Upstream: ``matched_code.cpp`` (verified in G1)
   * - LSIS-220
     - Synchronise tertiary start with frame start
     - Upstream: ``matched_code.cpp`` (verified in G1)

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
   * - GD4 unit tests
     - 12
     - ~120
     - < 1 s
     - ``task test-cpp``
   * - SWIFI (GD4 scenarios)
     - 2
     - ~20
     - < 1 s
     - ``task swifi``
   * - Coherency (upstream)
     - 1
     - 6000 epochs
     - < 2 s
     - ``task test-cpp``
   * - Python bindings
     - included in pytest
     - ---
     - < 5 s
     - ``task test-py``

All C++ unit tests pass in under 2 seconds with exit code 0.
SWIFI scenarios verify aliasing detection and output scrubbing for both
modulator and IQ multiplexer.

Related documents:

-  :doc:`signal/compliance_matrix` --- LSIS-101, LSIS-102, LSIS-103, LSIS-160, LSIS-171, LSIS-220
-  :doc:`signal/architecture` --- module-to-gateway mapping
-  :doc:`signal/signal_chain` --- transmitter block diagram with chip rates and data types
-  :doc:`signal/icd` --- formal interface control document
-  :doc:`assurance/public/mission_assurance` --- branchless, CFI, scrubbing patterns
-  :doc:`assurance/private/fmea` --- modulator and IQ mux failure modes
-  :doc:`assurance/private/fault_coverage` --- SWIFI scenario catalogue
