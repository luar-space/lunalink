Algorithm Selection Rationale
=============================

This document details the specific algorithms and architectural patterns chosen for the Forward Error Correction (FEC) and signal processing components of the LunaLink reference implementation. The selections prioritize **determinism**, **robustness**, and **computational efficiency**, aligning with Class B Flight Software constraints.

Architectural Hardening Patterns
--------------------------------

To meet the rigorous requirements of lunar missions (satellites, landers, and rovers), the implementation employs the following mission-critical patterns:

*   **Strict, Reproducible Arithmetic:** All Belief Propagation (BP) calculations in the LDPC decoder use IEEE-754 float64 with ``-fno-fast-math`` and a fixed, layered update order, giving **bit-reproducible results** run-to-run and across conforming toolchains. The numerically sensitive φ-transform is clamped with a quantified worst-case error analysis (documented in ``ldpc.cpp``). For flight targets without an FPU, the structural hardening transfers unchanged to a fixed-point port.
*   **Flat Worst-Case Execution Time (WCET):** Critical loops in the BCH decoder, CRC-24 generator, and LDPC check-node update are implemented using **branchless logic**. By removing data-dependent branches, the implementation guarantees a stable execution time, which is essential for real-time scheduling and avoiding "jitter" in navigation streams.
*   **Re-entrant Design (Workspace Pattern):** The LDPC decoder uses a dedicated `LdpcWorkspace` struct for all scratchpad memory. This removes dependency on static local buffers, allowing a single satellite node to track multiple concurrent signal sources (e.g., multiple rovers) without data corruption.
*   **LUT Memory Pinning:** All large Look-Up Tables (LDPC matrices, BCH codebooks, Weil sequences) utilize the `LUNALINK_LUT_SECTION` macro. This pins the data to non-volatile memory (Flash/MRAM), preventing unnecessary RAM usage and protecting the "Golden Matrices" from radiation-induced volatile memory flips.

BCH(51,8) Channel Coding
------------------------

The SB1 frame header (FID + TOI) is protected by the BCH(51,8) code of LSIS
§2.4.3.1.1.  Two components are provided: a constant-time LFSR encoder and
a maximum-likelihood correlation decoder.

BCH(51,8) Encoder: LFSR-based Mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** A Linear Feedback Shift Register (LFSR) with a feedback polynomial defined by the Lunanet SIS specification. It maps 9 bits of header information (FID and TOI) into a 52-bit codeword.

**Rationale:**
*   **Simplicity:** The LFSR implementation is extremely resource-light, requiring minimal CPU cycles and memory.
*   **Determinism:** The encoding process is strictly constant-time, ensuring predictable latency for the critical frame header.
*   **Compliance:** Directly implements the generator logic required by LSIS-AFS-501.

BCH(51,8) Decoder: Maximum Likelihood (ML) Correlation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Exhaustive correlation of the received soft symbol sequence against all **400 valid codewords** (4 FID values × 100 TOI values; the 9-bit FID+TOI input space would allow 2⁹ = 512, but TOI is constrained to 0–99).

**Rationale:**
*   **Optimal Performance:** ML decoding minimizes the Frame Error Rate (FER) in AWGN channels. The measured soft-decision ML decoder reaches error-free operation ≈ 2 dB earlier than hard-decision ML on this code (see :doc:`bch_performance`); algebraic bounded-distance decoders (like Berlekamp-Massey) give up further gain by stopping at t = 2 errors.
*   **Determinism:** The execution time is strictly constant (:math:`O(N \times 52)`), regardless of the error pattern or SNR. This guarantees a stable Worst-Case Execution Time (WCET) for real-time scheduling.
*   **Simplicity:** The codebook is small (3.2 KB), fitting easily into L1 cache or high-speed SRAM, avoiding complex iterative logic or Galois Field arithmetic.

LDPC Rate-1/2 Channel Coding
----------------------------

The LSIS-AFS LDPC codes (LSIS §2.4.3.1.2, Annex 1/2) are rate 1/2: SF2
(2400,1200) with lifting size Zc=120, and SF3/SF4 (1740,870) with Zc=88,
both derived from the 3GPP NR Base Graph 2 structure.

LDPC Encoder: Two-Stage Sparse Matrix Multiplication
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** A two-stage encoding method for structured LDPC codes.
1.  Stage 1: Calculate parity :math:`p_1 = B^{-1} \cdot (A \cdot s) \pmod 2`.
2.  Stage 2: Calculate parity :math:`p_2 = (C \cdot s + D \cdot p_1) \pmod 2`.

**Rationale:**
*   **Efficiency:** Sparse matrix multiplication avoids the :math:`O(n^2)` complexity of standard dense generator matrix multiplication. This is critical for high-throughput navigation messages on low-power lunar hardware.
*   **Low Memory:** Only the sparse sub-matrices (:math:`A, B, C, D`) need to be stored, significantly reducing the ROM/Flash footprint compared to a full generator matrix.

LDPC Decoder: Layered Sum-Product Belief Propagation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Belief Propagation (BP) that updates check-node and variable-node messages in a serial "layered" schedule, using the exact Sum-Product check-node update in its φ-transform formulation (:math:`\phi(x) = -\ln\tanh(x/2)`, self-inverse), which turns the product of hyperbolic tangents into a numerically stable sum.

**Rationale:**
*   **Convergence Speed:** Layered scheduling converges approximately 2× faster than standard "flooding" schedules, reducing the number of required iterations and thus CPU cycles.
*   **Decoding Optimality:** Sum-Product is the exact BP update — no Min-Sum approximation loss. The 3-way gap decomposition in ``test_ldpc_reference.cpp`` measures the shipped decoder within 0.15 dB of reference BP implementations on this code, i.e. essentially at the algorithmic optimum for its length.
*   **Numerical Robustness:** The φ-transform's singular endpoints are clamped with a quantified worst-case error analysis (upper clamp error ≈ 1.9e-13; empirically verified not to shift the waterfall by more than 0.01 dB — see ``ldpc.cpp``).
*   **Memory Footprint:** Layered decoding allows for "in-place" memory updates, reducing the RAM requirement for LLR storage by nearly 50% compared to flooding.

Integrity & Shaping
-------------------

CRC-24Q Generator
~~~~~~~~~~~~~~~~~

**Algorithm:** Bit-wise Galois Linear Feedback Shift Register (LFSR) with polynomial :math:`G(x) = x^{24} + x^{23} + \dots + 1` (``0x864CFB``).

**Rationale:**
*   **Compliance:** Strictly mandated by LSIS-FID0-467.
*   **Robustness:** Optimized for detecting burst errors common in wireless channels.
*   **Implementation:** The bit-wise implementation avoids the memory overhead of large lookup tables (e.g., slicing-by-8), favoring code simplicity and small footprint.

Block Interleaver
~~~~~~~~~~~~~~~~~

**Algorithm:** 60 × 98 matrix transposition (Write Row-Wise, Read Column-Wise).

**Rationale:**
*   **Fading Resistance:** Spreads burst errors (which affect adjacent symbols) across all three LDPC codewords of the frame (the 5880 interleaved symbols of SB2+SB3+SB4), ensuring that errors are distributed randomly enough for the decoders to correct them.
*   **Simplicity:** Direct index mapping avoids complex permutation generation logic.

Spreading & PRN Generation
--------------------------

Weil Code Generation
~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Construction of Weil sequences by XORing a Legendre sequence with a cyclically shifted version of itself, with specific insertion points for balancing.

**Rationale:**
*   **Superior Correlation:** Weil codes (e.g., length 10230 for Primary, 1500 for Tertiary) provide significantly better cross-correlation and auto-correlation properties than Gold codes for these specific lengths. This is crucial for multi-access interference (MAI) rejection in a dense lunar satellite constellation.
*   **Large Code Set:** Provides a large family of sequences with nearly uniform correlation properties, allowing for many unique PRN IDs without performance degradation.
*   **Compliance:** Mandated by LSIS-AFS-102 for the Augmented Forward Signal.

Gold Code Generation
~~~~~~~~~~~~~~~~~~~~

**Algorithm:** XOR of two preferred pairs of maximum-length sequences (m-sequences) generated by LFSRs.

**Rationale:**
*   **Legacy Interoperability:** Gold codes are the industry standard for GNSS (e.g., GPS L1 C/A), ensuring that receiver front-ends designed for terrestrial GNSS can be adapted for LunaLink with minimal architectural changes.
*   **Efficient Hardware:** Can be implemented using just two LFSRs and an XOR gate, making them extremely efficient for high-rate spreading in low-power hardware.
