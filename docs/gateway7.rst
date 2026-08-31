Gateway 7: Integration & Validation --- Verification Guide
===========================================================

This document maps each Gateway 7 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # C++ unit tests (< 10 s)
   task integration           # E2E round-trip tests (< 60 s)
   task performance           # WCET + BER benchmarks
   python tests/generate_test_vectors.py
                              # Generate interop test vectors

All tests must pass with exit code 0.

----

Scope
-----

Gateway 7 demonstrates end-to-end functionality, interoperability, and
compliance across all prior gateways (G1--G6).

.. list-table::
   :widths: 10 30 10 50
   :header-rows: 1

   * - ID
     - Deliverable
     - Status
     - Evidence
   * - GD7.1
     - SISE Calculation (Position/Velocity Error)
     - Done
     - ``sise.cpp``, ``test_sise.cpp`` (20 tests)
   * - GD7.2
     - Round-Trip E2E Validation
     - Done
     - ``test_e2e.cpp`` (10 tests), ``test_e2e_impairments.cpp``
   * - GD7.3
     - Interoperability Test Vector Suite
     - Done
     - ``generate_test_vectors.py``, ``test-vectors/``
   * - GD7.4
     - Performance Benchmark Validation
     - Done
     - 5 benchmark files in ``cpp/tests/performance/``
   * - GD7.5
     - Compliance Validation Report
     - Done
     - ``compliance_matrix.rst``, ``traceability_matrix.rst``

----

GD7.1 --- SISE Calculation
--------------------------

**Spec reference:** Competition Spec Phase 7.1 (Equations 1--2).

Signal-In-Space Error (SISE) quantifies the error between broadcast and
true satellite position/velocity.  The 95th percentile over a 24-hour
evaluation window must remain below 40 m (position) and 0.01 m/s
(velocity).

**Implementation files:**

.. list-table::
   :widths: 60 40
   :header-rows: 1

   * - File
     - Purpose
   * - ``cpp/include/lunalink/core/sise.hpp``
     - Header: SiseVec, SiseResult, SisePercentileResult, compute_sise, compute_sise_percentile
   * - ``cpp/core/sise.cpp``
     - Implementation: RSS, nth_element percentile, CFI hardening
   * - ``cpp/tests/core/test_sise.cpp``
     - 20 test cases (zero/known error, NaN/Inf rejection, percentile, boundary, 24-hour trajectory)

**API:**

.. code-block:: cpp

   namespace lunalink::core {

   struct SiseVec {
       double x = 0.0;
       double y = 0.0;
       double z = 0.0;
       double ct = 0.0;  // clock bias (pos) or drift (vel), scaled by c
   };

   // Single-epoch SISE: 4-D RSS of (true - broadcast) vectors
   [[nodiscard]] SiseResult compute_sise(
       SiseVec pos_true, SiseVec pos_broadcast,
       SiseVec vel_true, SiseVec vel_broadcast) noexcept;

   // 95th percentile over N epochs, validates against spec limits
   [[nodiscard]] SisePercentileResult compute_sise_percentile(
       const double* pos_errors_m, const double* vel_errors_mps,
       size_t n, double percentile = 0.95) noexcept;

   } // namespace lunalink::core

**Python bindings:**

.. code-block:: python

   from lunalink.afs.signal import SiseVec, compute_sise, compute_sise_percentile

   r = compute_sise(SiseVec(100, 200, 300, 0), SiseVec(103, 204, 300, 0),
                     SiseVec(10, 20, 30, 0), SiseVec(11, 21, 31, 0))
   assert r.status == SiseStatus.OK
   assert abs(r.position_error_m - 5.0) < 1e-12

**Hardening:** CFI sentinels, NaN/Inf input validation, secure scrub on failure,
single-exit pattern, cppcheck-suppressed duplicate conditions.

**Test evidence:**

.. code-block:: bash

   ./build/debug/test_lunalink "[core]"
   # All tests passed (20 test cases)

----

GD7.2 --- Round-Trip E2E Validation
------------------------------------

**Spec reference:** Gateway 7 deliverables (round-trip tests).

Full signal chain round-trip: navigation data → frame_build → modulate →
AWGN channel → correlator → frame_decode → message parse → verify
bit-exact recovery.

**Test evidence** (``cpp/tests/integration/test_e2e.cpp``):

.. list-table::
   :widths: 60 40
   :header-rows: 1

   * - Test Case
     - Coverage
   * - ``[LSIS-AFS-720] E2E round-trip: TX → AWGN → RX``
     - Noiseless + 3 dB AWGN
   * - ``[LSIS-AFS-810] E2E: TX → RX → parse → ToT``
     - Time-of-transmission formula
   * - ``E2E: noiseless round-trip (Eb/N0 = infinity)``
     - Perfect channel baseline
   * - ``E2E: CRC rejects corrupted subframe``
     - Corruption detection
   * - ``E2E: consecutive frames with incrementing TOI``
     - Stateless decoder
   * - ``E2E: kSyncNotFound then success``
     - Error recovery
   * - ``E2E: frame_decode < 1 second``
     - NFR-1.3 latency
   * - ``E2E: FID=0 TOI=99 with random offset``
     - Boundary FID/TOI values
   * - ``[LSIS-TABLE11] all 12 interim codes round-trip``
     - PRN 1-12 parameterised sweep
   * - ``Gateway 5: frame-level FER sweep``
     - 350 frames, 3 SNR points, zero errors

**Quantization robustness** (``test_e2e_impairments.cpp``):

- int16 round-trip (ADC simulation)
- int8 round-trip (extreme quantization)

.. code-block:: bash

   task integration
   # All E2E tests pass

----

GD7.3 --- Interoperability Test Vector Suite
---------------------------------------------

**Spec reference:** Gateway 7 deliverables (interoperability tests).

Standardised test vectors for cross-implementation validation, covering
all five interoperability levels:

.. list-table::
   :widths: 10 30 30 30
   :header-rows: 1

   * - Level
     - Scope
     - Format
     - Location
   * - 1
     - Spreading codes (PRN 1-12)
     - ``.hex`` (Gold, Weil primary/tertiary, secondary)
     - ``test-vectors/codes/``
   * - 2
     - FEC encoding (BCH, LDPC, CRC, interleaver)
     - JSON (24 BCH + 12 LDPC + 8 CRC + 4 interleaver)
     - ``test-vectors/fec/``
   * - 3
     - Complete frames (5 standardised messages)
     - Binary with 128-byte LSISIQ header
     - ``test-vectors/frames/``
   * - 4
     - I/Q signal files (PRN 1-12, 2 sample rates)
     - Binary float32 with 128-byte LSISIQ header
     - ``test-vectors/signals/``
   * - 5
     - Parsed message metadata + decode-direction vectors
     - JSON (SB1-4 fields, expected decoder outputs)
     - ``test-vectors/parsed/``, ``test-vectors/decode/``

**Standard test messages:**

1. All zeros (baseline)
2. All ones (inverse baseline)
3. Alternating 0xAA pattern
4. Boundary (FID=3, TOI=99)
5. Mid-range random (seed 0x12345678)

.. code-block:: bash

   python tests/generate_test_vectors.py
   ls test-vectors/

Cross-Implementation Interoperability (LANS-AFS-SIM)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Cross-implementation decode demonstrated with
`LANS-AFS-SIM <https://github.com/osqzss/LANS-AFS-SIM>`_ (Ebinuma,
BSD-2-Clause), an independent LSIS-AFS implementation.

.. list-table::
   :widths: 10 35 15 40
   :header-rows: 1

   * - Level
     - Test
     - Result
     - Detail
   * - L1
     - Gold + Weil code bit-exact (PRN 1–12)
     - **PASS**
     - 0 chip mismatches across 24 code comparisons
   * - L2
     - Frame encoding bit-exact (5 payloads)
     - **PASS**
     - BCH, CRC-24, LDPC, interleaver all match
   * - L3
     - C++ correlator acquire + track + decode
     - **PASS**
     - PCPS lock, DLL/PLL track, CRC-verified nav data

Full results: :doc:`interop_results`

.. code-block:: bash

   # Reproduce
   python scripts/interop/build_lans_afs_sim.py
   pytest tests/test_interop_codes.py tests/test_interop_frames.py \
          tests/test_interop_decode.py -v

----

GD7.4 --- Performance Benchmark Validation
--------------------------------------------

**Spec reference:** NFR-1 (Performance).

.. list-table::
   :widths: 30 20 30 20
   :header-rows: 1

   * - Metric
     - Requirement
     - Measured
     - Status
   * - Code generation
     - < 1 s per PRN
     - < 10 ms
     - Pass
   * - Frame encoding
     - < 100 ms per frame
     - < 5 ms
     - Pass
   * - Frame decoding
     - < 1 s per frame
     - Verified in ``test_e2e.cpp``
     - Pass
   * - Real-time capable
     - Process faster than signal
     - 12 s signal in < 1 s
     - Pass
   * - LDPC convergence
     - < 50 iterations
     - Knee at 15 iterations
     - Pass

**Benchmark files:**

- ``test_wcet.cpp`` --- LDPC encode/decode, BCH ML, sync detect
- ``test_bch_ber.cpp`` --- BCH FER sweep (7 Eb/N0 points)
- ``test_ldpc_ber.cpp`` --- LDPC waterfall (9 SNR points), alpha sweep, quantization
- ``test_correlator_perf.cpp`` --- PCPS acquisition, DLL/PLL loop
- ``test_sync_detection_rate.cpp`` --- Frame sync Pd vs SNR

.. code-block:: bash

   task performance    # Run all performance benchmarks

----

GD7.5 --- Compliance Validation
---------------------------------

**Spec reference:** All LSIS requirements.

- **Shall-statement inventory:** ``docs/signal/shall_inventory.rst`` --- every
  LSIS-NNN identifier in LSIS V1.0 mapped to disposition (70 total: 38
  implemented, 12 RF/out-of-scope, 15 TBW, 3 TBC, 2 FLEX).
- **Compliance matrix:** ``docs/signal/compliance_matrix.rst`` --- all G1-G7
  requirements tracked; zero TODOs remaining for G1-G7.
- **Traceability matrix:** ``docs/assurance/traceability_matrix.rst`` ---
  22 LSIS-AFS + 5 SW-SAF requirements mapped to tests; 100% coverage.
- **Test coverage:** 189+ tests pass (150 unit + 39 SWIFI).

.. code-block:: bash

   task test-cpp       # 189+ tests pass
   task traceability   # 27 requirements, 0 orphans
