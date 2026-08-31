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
... (rest of the file)

BCH(51,8) Encoder: LFSR-based Mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** A Linear Feedback Shift Register (LFSR) with a feedback polynomial defined by the Lunanet SIS specification. It maps 9 bits of header information (FID and TOI) into a 52-bit codeword.

**Rationale:**
*   **Simplicity:** The LFSR implementation is extremely resource-light, requiring minimal CPU cycles and memory.
*   **Determinism:** The encoding process is strictly constant-time, ensuring predictable latency for the critical frame header.
*   **Compliance:** Directly implements the generator logic required by LSIS-AFS-501.

BCH(51,8) Decoder: Maximum Likelihood (ML) Correlation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Exhaustive correlation of the received soft symbol sequence against all $2^8 = 256$ valid codewords (or 400 total candidates when accounting for TOI constraints).

**Rationale:**
*   **Optimal Performance:** ML decoding minimizes the Frame Error Rate (FER) in AWGN channels, outperforming algebraic hard-decision decoders (like Berlekamp-Massey) by significant margins ($>2$ dB).
*   **Determinism:** The execution time is strictly constant ($O(N \times 52)$), regardless of the error pattern or SNR. This guarantees a stable Worst-Case Execution Time (WCET) for real-time scheduling.
*   **Simplicity:** The codebook is small (3.2 KB), fitting easily into L1 cache or high-speed SRAM, avoiding complex iterative logic or Galois Field arithmetic.

LDPC(1200,600) Channel Coding
-----------------------------

LDPC Encoder: Two-Stage Sparse Matrix Multiplication
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** A two-stage encoding method for structured LDPC codes.
1.  Stage 1: Calculate parity $p_1 = B^{-1} \cdot (A \cdot s) \pmod 2$.
2.  Stage 2: Calculate parity $p_2 = (C \cdot s + D \cdot p_1) \pmod 2$.

**Rationale:**
*   **Efficiency:** Sparse matrix multiplication avoids the $O(n^2)$ complexity of standard dense generator matrix multiplication. This is critical for high-throughput navigation messages on low-power lunar hardware.
*   **Low Memory:** Only the sparse sub-matrices ($A, B, C, D$) need to be stored, significantly reducing the ROM/Flash footprint compared to a full generator matrix.

LDPC Decoder: Layered Sum-Product Belief Propagation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Belief Propagation (BP) that updates check-node and variable-node messages in a serial "layered" schedule, using the exact Sum-Product check-node update in its φ-transform formulation ($\phi(x) = -\ln\tanh(x/2)$, self-inverse), which turns the product of hyperbolic tangents into a numerically stable sum.

**Rationale:**
*   **Convergence Speed:** Layered scheduling converges approximately $2\times$ faster than standard "flooding" schedules, reducing the number of required iterations and thus CPU cycles.
*   **Decoding Optimality:** Sum-Product is the exact BP update — no Min-Sum approximation loss. The 3-way gap decomposition in ``test_ldpc_reference.cpp`` measures the shipped decoder within 0.15 dB of reference BP implementations on this code, i.e. essentially at the algorithmic optimum for its length.
*   **Numerical Robustness:** The φ-transform's singular endpoints are clamped with a quantified worst-case error analysis (upper clamp error ≈ 1.9e-13; empirically verified not to shift the waterfall by more than 0.01 dB — see ``ldpc.cpp``).
*   **Memory Footprint:** Layered decoding allows for "in-place" memory updates, reducing the RAM requirement for LLR storage by nearly 50% compared to flooding.

Integrity & Shaping
-------------------

CRC-24Q Generator
~~~~~~~~~~~~~~~~~

**Algorithm:** Bit-wise Galois Linear Feedback Shift Register (LFSR) with polynomial $G(x) = x^{24} + x^{23} + \dots + 1$ ($0x864CFB$).

**Rationale:**
*   **Compliance:** Strictly mandated by LSIS-FID0-467.
*   **Robustness:** Optimized for detecting burst errors common in wireless channels.
*   **Implementation:** The bit-wise implementation avoids the memory overhead of large lookup tables (e.g., slicing-by-8), favoring code simplicity and small footprint.

Block Interleaver
~~~~~~~~~~~~~~~~~

**Algorithm:** $60 \times 98$ matrix transposition (Write Row-Wise, Read Column-Wise).

**Rationale:**
*   **Fading Resistance:** Spreads burst errors (which affect adjacent symbols) across the entire LDPC codeword, ensuring that errors are distributed randomly enough for the decoder to correct them.
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
