Validation Plan
===============

The validation strategy for the LunaLink LSIS-AFS reference implementation.  Functional testing is the floor; reference-grade correctness, resilience under impairment, and spec traceability are the target.

Validation Hierarchy
--------------------

Five levels cover the signal chain end to end.

*   **Level 1: Unit Tests** — Mathematical correctness of LFSRs, GF(2) operations, and bit-packing.
*   **Level 2: Component Tests** — Verification of the individual blocks (:doc:`/verifying`).
    *   **Generator Validation:** Bit-for-bit comparison between C++ Algorithmic Generators (Gold, Weil) and Annex3 Hex reference data.
    *   **Coherency Validation:** Verification of Primary/Secondary/Tertiary phase alignment (LSIS-172) over a full 6000-epoch frame.
    *   **Interim Codes:** Validation of Table 11 mappings for PRN 1-12.
*   **Level 3: Integration Tests** — End-to-end Round-Trip (TX -> Channel -> RX) verification.
*   **Level 4: Compliance Tests** — Traceability to every "shall" requirement in the LSIS V1.0 spec.
*   **Level 5: Interoperability Tests** — Cross-decoding of standardized test vectors with independent implementations.

Reference-Grade Edge Case Validation (G2, G3, G6)
-------------------------------------------------

Flight-like scenarios demand these temporal and numerical edge cases:

.. list-table::
   :widths: 25 50 25
   :header-rows: 1

   * - Scenario
     - Description
     - Target Gateway
   * - **WN Rollover**
     - Transition from Week 8191 to 0.
     - G6 (Parser)
   * - **ITOW Boundary**
     - Transition from 503 to 0 (end of week).
     - G6 (Parser)
   * - **Field Limits**
     - Maximum and minimum values for all 22 MSG types (eccentricity, drift, etc.).
     - G6 (Parser)
   * - **Invalid FID/TOI**
     - Injection of out-of-range Frame ID (>3) or Time of Interval (>99).
     - G2 (BCH Decoder)
   * - **CRC Collisions**
     - Verification of CRC-24 error detection with specific bit-error patterns.
     - G2 (Integrity)

Signal Impairment & Robustness (G4, G5)
---------------------------------------

The baseband signal generator (G4) carries a reference channel model that puts the G5 receiver through realistic lunar-orbit impairments.

1. **Doppler (CFO, carrier frequency offset):** *Exercised on a foreign signal*

   * The L3 cross-decode acquires (PCPS) and tracks (PLL) a 7-SV LANS-AFS-SIM
     composite with lunar NRHO ephemeris, so carrier offset is handled on a
     real third-party signal (``tests/test_interop_decode.py``,
     :doc:`../interop_results`).
   * Not yet done: a parametric −10…+10 kHz sweep on the LunaLink-generated
     signal.

2. **Clock Drift (SCO, sample clock offset):** *Planned, not implemented*

   * Intent: sample clock offsets up to 50 ppm; code-phase tracking must
     compensate.
   * No test exists in this release and the receiver has no delay-locked
     loop; recorded in :doc:`../roadmap`.

3. **Quantization Loss:** *Implemented*

   * ``cpp/tests/integration/test_e2e_impairments.cpp``
     (``task integration``): full TX→RX round trips at int16 and int8, an
     FER sweep at int8, the int8-vs-float32 implementation loss measured
     directly, and burst-error resilience through the interleaver.
   * Result: int8 costs no measurable performance
     (:doc:`ldpc_performance` §*Quantization Resilience*), so an 8-bit
     front-end suffices.

4. **Phase Noise:** *Planned, not implemented*

   * Intent: oscillator phase noise per standard PNT models (Wiener process)
     to test PLL jitter tolerance.
   * No test exists in this release; recorded in :doc:`../roadmap`.

Standardized Test Scenarios (G7)
--------------------------------

As defined in the Interoperability Plan, the exported round-trip test
vectors (``tests/generate_test_vectors.py``) are built from five
standardized payloads:

*   **MSG-1 (All Zeros):** Baseline validation.
*   **MSG-2 (All Ones):** Scrambler/logic validation.
*   **MSG-3 (Alternating):** Interleaver pattern validation.
*   **MSG-4 (Ephemeris):** Real-world field serialization (Keplerian CED from
    a lunar NRHO orbit).
*   **MSG-5 (Random):** LDPC convergence and stress testing.

The cross-implementation frame comparison against LANS-AFS-SIM
(``tests/test_interop_frames.py``, :doc:`../interop_results`) uses a related
but distinct set: five frame cases spanning three fill patterns (all-zeros,
all-ones, alternating) plus boundary FID/TOI values.

Performance Benchmarking
------------------------

Final verification measures four NFRs:

*   **Generation Latency:** < 1.0s per PRN.
*   **Encoding Throughput:** > 10 Frames Per Second (FPS).
*   **Decoding Latency:** < 1.0s per 12s frame.
*   **Real-Time Margin:** Processing time must be < 8% of signal duration (12s).

Design Rationale & Traceability
-------------------------------

The design records (:doc:`algorithms`, :doc:`bch_decoder_design`) explain:
*   **Algorithm Choice:** e.g., "Layered Sum-Product BP chosen for LDPC decoding — exact check-node update with a clamped, numerically analysed φ-transform."
*   **Mathematical Alignment:** Code snippets linked to Equation 1, Equation 2, and Figure 7/8 of the LSIS spec.
*   **Implementation Trade-offs:** Rationale for static buffer sizing vs. heap flexibility.
