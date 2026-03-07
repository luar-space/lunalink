BCH(51,8) Decoder: Architectural Design Record
================================================

This document outlines the architectural rationale for the BCH(51,8) decoder 
implementation in the LunaLink Signal-In-Space (SIS) library.

Context and Constraints
-----------------------

The BCH(51,8) code is used to protect the SB1 sub-block (FID and TOI) in the 
Augmented Forward Signal (AFS). Decoding this block is a critical path for 
receiver synchronization. The target environments are:

*   **Lunar Satellites/Orbiters:** Limited CPU (LEON3/4, Cortex-M4F).
*   **Surface Rovers/Astronaut Handhelds:** High power-efficiency requirements.
*   **Radiation Environments:** Susceptibility to Single Event Upsets (SEUs).

Architectural Choice: Bit-Parallel ML Correlation
-------------------------------------------------

We have selected a **Maximum Likelihood (ML) Correlation Decoder** utilizing a 
static codebook and bit-parallel XOR-Popcount operations, rather than a 
traditional syndrome-based algebraic decoder (e.g., Berlekamp-Massey).

Rationale
---------

### 1. Information Entropy vs. Search Space
The SB1 field contains 9 bits of information (2 bits FID, 7 bits TOI). However, 
the TOI is constrained to 0–99 (§2.3.4.1), resulting in only **400 valid codewords** 
out of the 512 possible in a 9-bit space. 

*   An exhaustive search of 400 candidates is computationally trivial.
*   The entire codebook occupies only **3.2 KB** (400 x 64-bit words), fitting 
    easily into the L1 cache or tightly coupled memory (TCM) of space-grade MCUs.

### 2. Deterministic Execution (WCET)
Algebraic decoders (Berlekamp-Massey, Chien Search) involve complex iterative 
logic and branching based on the number and position of detected errors. 

*   In flight software (FSW), non-deterministic execution time (jitter) complicates 
    real-time scheduling.
*   Our ML approach is **constant-time**: it performs exactly 400 XOR/Popcount 
    operations regardless of signal quality, providing a guaranteed 
    Worst-Case Execution Time (WCET).

### 3. Coding Gain and Optimality
Algebraic decoders typically correct errors only up to the formal limit 
:math:`t = \lfloor (d_{min}-1)/2 \rfloor`. For this code (:math:`d_{min}=5`), 
an algebraic decoder stops at 2 errors.

*   The ML decoder is **optimal**; it will find the closest codeword even if 3 or 4 
    errors occur (if the noise realization doesn't move the vector closer to 
    another codeword).
*   This provides superior performance in low Signal-to-Noise Ratio (SNR) 
    conditions typical of lunar-to-surface links.

### 4. Implementation Simplicity and Verifiability
Space flight certification (ECSS-Q-ST-80C / NASA Class A) requires rigorous 
code auditing and path coverage.

*   Traditional decoders require complex Galois Field (:math:`GF(2^m)`) arithmetic 
    libraries, which increase the attack surface for bugs.
*   The XOR-Popcount logic is mathematically simple, contains no complex 
    branching, and is significantly easier to formally verify and test.

### 5. Radiation Resilience (SEU Mitigation)
By using a static Look-Up Table (LUT) stored in ``const`` memory (mapped to 
Flash/PROM/MRAM):

*   The reference data is inherently more resistant to Single Event Upsets (SEU) 
    compared to transient data in registers during complex algebraic calculations.
*   The simplicity of the bit-parallel loop reduces the "vulnerability window" 
    during which an alpha particle could disrupt a calculation.

Hardware Performance Targets
----------------------------

On a 100 MHz LEON3 processor, the 400-iteration XOR-Popcount loop is 
estimated to complete in under **20 microseconds**, well within the 12.0s 
frame processing budget.
