End-to-end tests and Signal-In-Space Error
==========================================

This page covers two things that span the blocks: the tests that run the whole
transmit and receive chain, and the Signal-In-Space Error calculation of
LSIS V1.0 §2.2.

End-to-end round trip
---------------------

The end-to-end tests take navigation data through frame building and
modulation, add noise, demodulate to soft symbols, decode the frame, parse the
message, and check that the recovered bits equal the transmitted ones.

.. code-block:: bash

   task integration

Tests in ``cpp/tests/integration/test_e2e.cpp``:

.. list-table::
   :widths: 60 40
   :header-rows: 1

   * - Test case
     - What it checks
   * - ``[LSIS-AFS-720] E2E round-trip: TX → AWGN → RX → bit-exact recovery``
     - Recovery without noise and with additive white Gaussian noise
   * - ``[LSIS-AFS-810] E2E: TX → RX → parse → ToT round-trip``
     - The time-of-transmission formula on decoded data
   * - ``E2E: noiseless round-trip (Eb/N0 = infinity)``
     - The baseline on a perfect channel
   * - ``E2E: CRC rejects corrupted subframe payload``
     - Corruption is detected
   * - ``E2E: consecutive frames with incrementing TOI``
     - The decoder keeps no state between frames
   * - ``E2E: kSyncNotFound then success — decoder is stateless``
     - Recovery after a failed synchronisation
   * - ``E2E: frame_decode completes in < 1 second``
     - Decode latency
   * - ``E2E round-trip: FID=0 TOI=99 with random offset``
     - Boundary values of FID and TOI
   * - ``[LSIS-TABLE11][e2e] all 12 interim codes round-trip``
     - PRN 1 to 12 with the interim assignments of Table 11
   * - ``Gateway 5: frame-level FER sweep``
     - Frame error rate at several signal-to-noise ratios

Tests in ``cpp/tests/integration/test_e2e_impairments.cpp``:

.. list-table::
   :widths: 60 40
   :header-rows: 1

   * - Test case
     - What it checks
   * - ``E2E impairment: round-trip at int16``
     - Samples quantised to 16 bits
   * - ``E2E impairment: round-trip at int8``
     - Samples quantised to 8 bits
   * - ``E2E impairment: FER sweep at int8``
     - Frame error rate with 8-bit samples
   * - ``E2E impairment: implementation loss int8 vs float32``
     - The loss from 8-bit quantisation relative to float32
   * - ``E2E impairment: burst error resilience through interleaver``
     - Burst errors spread by the interleaver

Sample-clock-offset and phase-noise impairments are not tested; see
:doc:`/signal/validation_plan`.

Signal-In-Space Error
---------------------

Competition deliverable: GD7.1.

Signal-In-Space Error is the error between a satellite's broadcast position
and velocity and its true position and velocity.  LSIS V1.0 §2.2 requires the
95th percentile over 24 hours to stay below 40 m in position (LSIS-001) and
0.01 m/s in velocity (LSIS-002); the standard marks both limits and the
24-hour period as to be confirmed.  LunaLink implements the two equations of
§2.2.2 and the percentile check.  The caller supplies the true and broadcast
vectors; LunaLink has no constellation simulation (see :doc:`/roadmap`).

Source files:

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - File
     - Purpose
   * - ``cpp/include/lunalink/core/sise.hpp``
     - ``SiseVec``, ``SiseResult``, ``SisePercentileResult``,
       ``compute_sise``, ``compute_sise_percentile``
   * - ``cpp/core/sise.cpp``
     - The root-sum-square of the four error components, and the percentile
   * - ``cpp/tests/core/test_sise.cpp``
     - Zero and known errors, rejection of NaN and infinity, the percentile,
       boundary values, and a 24-hour trajectory

Interface:

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

From Python:

.. code-block:: python

   from lunalink.afs.signal import SiseStatus, SiseVec, compute_sise

   r = compute_sise(SiseVec(100, 200, 300, 0), SiseVec(103, 204, 300, 0),
                    SiseVec(10, 20, 30, 0), SiseVec(11, 21, 31, 0))
   assert r.status == SiseStatus.OK
   assert abs(r.position_error_m - 5.0) < 1e-12

Hardening: control-flow sentinels, rejection of NaN and infinite inputs, a
scrub of the outputs on failure, and a single exit point.

.. code-block:: bash

   task test-cpp       # includes the [core] suite
   # or directly, once built:
   build/cpp-tests-ninja/test_lunalink "[core]"
