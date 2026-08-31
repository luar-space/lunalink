Validation Plan
===============

This document defines the comprehensive validation strategy for the LunaLink LSIS-AFS reference implementation. It extends beyond basic functional testing to ensure "Reference-Grade" correctness, robustness, and spec-traceability.

Validation Hierarchy
--------------------

We follow a five-level validation hierarchy to ensure quality at every stage of the signal chain.

*   **Level 1: Unit Tests** — Mathematical correctness of LFSRs, GF(2) operations, and bit-packing.
*   **Level 2: Component Tests** — Verification of individual Gateways (G1-G6).
    *   **Generator Validation:** Bit-for-bit comparison between C++ Algorithmic Generators (Gold, Weil) and Annex3 Hex reference data.
    *   **Coherency Validation:** Verification of Primary/Secondary/Tertiary phase alignment (LSIS-172) over a full 6000-epoch frame.
    *   **Interim Codes:** Validation of Table 11 mappings for PRN 1-12.
*   **Level 3: Integration Tests** — End-to-end Round-Trip (TX -> Channel -> RX) verification.
*   **Level 4: Compliance Tests** — Traceability to every "shall" requirement in the LSIS V1.0 spec.
*   **Level 5: Interoperability Tests** — Cross-decoding of standardized test vectors with independent implementations.

Reference-Grade Edge Case Validation (G2, G3, G6)
-------------------------------------------------

To ensure the implementation is robust for flight-like scenarios, we validate the following temporal and numerical edge cases:

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

The baseband signal generator (G4) includes a "Reference Channel Model" to test the receiver's (G5) robustness against realistic lunar orbit impairments.

1. **Doppler Modeling (CFO — carrier frequency offset):**
   * Sweep carrier frequency offsets from -10 kHz to +10 kHz.
   * Requirement: Acquisition and tracking must remain stable (G5 success metric).

2. **Clock Drift (SCO — sample clock offset):** — *Implemented*
   * Simulate sample clock offsets up to 50 ppm.
   * Requirement: Code phase tracking (DLL) must compensate for timing drift.
   * Test: ``test_channel_impairments.cpp`` — parametric sweep 0–50 ppm, 200 epochs each.
   * DLL upgraded to second-order PI filter for zero steady-state error.

3. **Quantization Loss:** — *Implemented*
   * Compare decoding success rates between ``float32``, ``int16``, and ``int8`` I/Q formats.
   * Document implementation loss (Target: < 0.2 dB).
   * Test: ``test_channel_impairments.cpp`` — acquisition metric comparison + tracking stability.
   * E2E test: ``test_e2e_impairments.cpp`` — full TX→RX pipeline at int16/int8.

4. **Phase Noise:** — *Implemented*
   * Inject oscillator phase noise per standard PNT models to test Costas loop (PLL) jitter.
   * Test: ``test_channel_impairments.cpp`` — Wiener process model, parametric sweep 0–1e-3 rad²/sample.

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

Performance Benchmarking (GD7.3, GD8.2)
---------------------------------------

Final verification includes empirical proof of the following NFRs:

*   **Generation Latency (GD1.1):** < 1.0s per PRN.
*   **Encoding Throughput (GD2.2):** > 10 Frames Per Second (FPS).
*   **Decoding Latency (GD2.2):** < 1.0s per 12s frame.
*   **Real-Time Margin:** Processing time must be < 8% of signal duration (12s).

Design Rationale & Traceability
-------------------------------

Each gateway deliverable (GD8.1) includes a **Design Rationale** section explaining:
*   **Algorithm Choice:** e.g., "Layered Sum-Product BP chosen for LDPC decoding — exact check-node update with a clamped, numerically analysed φ-transform."
*   **Mathematical Alignment:** Code snippets linked to Equation 1, Equation 2, and Figure 7/8 of the LSIS spec.
*   **Implementation Trade-offs:** Rationale for static buffer sizing vs. heap flexibility.
