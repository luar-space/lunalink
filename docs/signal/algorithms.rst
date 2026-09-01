Algorithm Selection Rationale
=============================

Which algorithms and architectural patterns the Forward Error Correction (FEC) and signal-processing components use, and why.  Three properties decided every choice — **determinism**, **fault tolerance**, **computational efficiency** — in the order Class B flight software demands.

Architectural Hardening Patterns
--------------------------------

Lunar missions — satellites, landers, rovers — impose the patterns below.

*   **Strict, Reproducible Arithmetic:** All Belief Propagation (BP) calculations in the LDPC decoder use IEEE-754 float64 with ``-fno-fast-math`` and a fixed, layered update order, giving **bit-reproducible results** run-to-run and across conforming toolchains. The numerically sensitive φ-transform is clamped with a quantified worst-case error analysis (documented in ``ldpc.cpp``). For flight targets without an FPU, the structural hardening transfers unchanged to a fixed-point port.
*   **Flat Worst-Case Execution Time (WCET):** Critical loops in the BCH decoder, CRC-24 generator, and LDPC check-node update are implemented using **branchless logic**. By removing data-dependent branches, the implementation guarantees a stable execution time, which is essential for real-time scheduling and avoiding "jitter" in navigation streams.
*   **Re-entrant Design (Workspace Pattern):** The LDPC decoder uses a dedicated `LdpcWorkspace` struct for all scratchpad memory. This removes dependency on static local buffers, allowing a single satellite node to track multiple concurrent signal sources (e.g., multiple rovers) without data corruption.
*   **LUT Memory Pinning:** every large Look-Up Table (LDPC matrices, BCH codebooks, Weil sequences) carries the `LUNALINK_LUT_SECTION` macro. This pins the data to non-volatile memory (Flash/MRAM), preventing unnecessary RAM usage and protecting the "Golden Matrices" from radiation-induced volatile memory flips.

BCH(51,8) Channel Coding
------------------------

The SB1 frame header (FID + TOI) is protected by the BCH(51,8) code of LSIS
§2.4.3.1.1.  Two components are provided: a constant-time LFSR encoder and
a maximum-likelihood correlation decoder.

BCH(51,8) Encoder: LFSR-based Mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** A Linear Feedback Shift Register (LFSR) with a feedback polynomial defined by the Lunanet SIS specification. It maps 9 bits of header information (FID and TOI) into a 52-bit codeword.

**Rationale:**
*   **Simplicity:** the LFSR costs few CPU cycles and little memory.
*   **Determinism:** encoding is constant-time, so the frame header carries a fixed latency.
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
*   **Efficiency:** sparse multiplication avoids the :math:`O(n^2)` cost of a dense generator matrix — the difference between meeting and missing throughput on low-power lunar hardware.
*   **Low Memory:** only the sparse sub-matrices (:math:`A, B, C, D`) are stored, which cuts the ROM/Flash footprint well below that of a full generator matrix.

LDPC Decoder: Layered Sum-Product Belief Propagation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Belief Propagation (BP) that updates check-node and variable-node messages in a serial "layered" schedule, using the exact Sum-Product check-node update in its φ-transform formulation (:math:`\phi(x) = -\ln\tanh(x/2)`, self-inverse), which turns the product of hyperbolic tangents into a numerically stable sum.

**Rationale:**
*   **Convergence Speed:** Layered scheduling converges approximately 2× faster than standard "flooding" schedules, reducing the number of required iterations and thus CPU cycles.
*   **Decoding Optimality:** Sum-Product is the exact BP update — no Min-Sum approximation loss. The 3-way gap decomposition in ``test_ldpc_reference.cpp`` measures the shipped decoder within 0.15 dB of reference BP implementations on this code, i.e. essentially at the algorithmic optimum for its length.
*   **Numerical Stability:** the φ-transform's singular endpoints are clamped with a quantified worst-case error analysis (upper clamp error ≈ 1.9e-13; empirically verified not to shift the waterfall by more than 0.01 dB — see ``ldpc.cpp``).
*   **Memory Footprint:** layered decoding updates in place, cutting LLR storage by nearly half against flooding.

Integrity & Shaping
-------------------

CRC-24Q Generator
~~~~~~~~~~~~~~~~~

**Algorithm:** Bit-wise Galois Linear Feedback Shift Register (LFSR) with polynomial :math:`G(x) = x^{24} + x^{23} + \dots + 1` (``0x864CFB``).

**Rationale:**
*   **Compliance:** Strictly mandated by LSIS-FID0-467.
*   **Burst detection:** tuned for the burst errors that dominate wireless channels.
*   **Implementation:** The bit-wise implementation avoids the memory overhead of large lookup tables (e.g., slicing-by-8), favoring code simplicity and small footprint.

Block Interleaver
~~~~~~~~~~~~~~~~~

**Algorithm:** 60 × 98 matrix transposition (Write Row-Wise, Read Column-Wise).

**Rationale:**
*   **Fading Resistance:** a burst hits adjacent symbols; the transpose scatters it across all three LDPC codewords of the frame (the 5880 interleaved symbols of SB2+SB3+SB4), leaving each decoder a spread of errors it can correct.
*   **Simplicity:** Direct index mapping avoids complex permutation generation logic.

Spreading & PRN Generation
--------------------------

Weil Code Generation
~~~~~~~~~~~~~~~~~~~~

**Algorithm:** Construction of Weil sequences by XORing a Legendre sequence with a cyclically shifted version of itself, with specific insertion points for balancing.

**Rationale:**
*   **Superior Correlation:** at these lengths — 10230 primary, 1500 tertiary — Weil codes beat Gold codes on both cross- and auto-correlation, and that margin is what rejects multi-access interference (MAI) in a dense lunar constellation.
*   **Large Code Set:** the family is large and its correlation properties near-uniform, so many unique PRN IDs cost nothing in performance.
*   **Compliance:** Mandated by LSIS-AFS-102 for the Augmented Forward Signal.

Gold Code Generation
~~~~~~~~~~~~~~~~~~~~

**Algorithm:** XOR of two preferred pairs of maximum-length sequences (m-sequences) generated by LFSRs.

**Rationale:**
*   **Legacy Interoperability:** Gold codes are the GNSS industry standard, GPS L1 C/A among them, so a terrestrial GNSS front-end adapts to LunaLink with little architectural change.
*   **Efficient Hardware:** two LFSRs and an XOR gate suffice, which suits high-rate spreading on low-power parts.
