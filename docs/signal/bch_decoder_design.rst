BCH(51,8) Decoder: Architectural Design Record
================================================

Why the BCH(51,8) decoder in the LunaLink Signal-In-Space (SIS) library is
built the way it is.

Context and Constraints
-----------------------

BCH(51,8) protects the SB1 sub-block (FID and TOI) in the Augmented Forward
Signal (AFS), and decoding it sits on the critical path for receiver
synchronisation.  The target environments:

*   **Lunar Satellites/Orbiters:** Limited CPU (LEON3/4, Cortex-M4F).
*   **Surface Rovers/Astronaut Handhelds:** High power-efficiency requirements.
*   **Radiation Environments:** Susceptibility to Single Event Upsets (SEUs).

Architectural Choice: Bit-Parallel ML Correlation
-------------------------------------------------

The decoder is a **Maximum Likelihood (ML) correlator** over a static codebook,
using bit-parallel XOR-popcount rather than a syndrome-based algebraic decoder
such as Berlekamp-Massey.

Rationale
---------

1. Information Entropy vs. Search Space
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The SB1 field carries 9 bits of information: 2 bits of FID, 7 of TOI.  TOI is
constrained to 0–99 (§2.5.5, Table 22), which leaves **400 valid codewords**
out of the 512 a 9-bit space allows.

*   An exhaustive search of 400 candidates is computationally trivial.
*   The entire codebook occupies only **3.2 KB** (400 x 64-bit words), fitting
    easily into the L1 cache or tightly coupled memory (TCM) of space-grade MCUs.

2. Deterministic Execution (WCET)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Algebraic decoders (Berlekamp-Massey, Chien Search) involve complex iterative
logic and branching based on the number and position of detected errors.

*   In flight software (FSW), non-deterministic execution time (jitter) complicates
    real-time scheduling.
*   Our ML approach is **constant-time**: it performs exactly 400 XOR/Popcount
    operations regardless of signal quality, providing a guaranteed
    Worst-Case Execution Time (WCET).

3. Coding Gain and Optimality
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Algebraic decoders typically correct errors only up to the formal limit
:math:`t = \lfloor (d_{min}-1)/2 \rfloor`. For this code (:math:`d_{min}=5`),
an algebraic decoder stops at 2 errors.

*   The ML decoder is **optimal**; it will find the closest codeword even if 3 or 4
    errors occur (if the noise realization doesn't move the vector closer to
    another codeword).
*   That margin shows up at the low Signal-to-Noise Ratio (SNR) typical of
    lunar-to-surface links.

4. Implementation Simplicity and Verifiability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Space flight certification (ECSS-Q-ST-80C / NASA Class B) requires rigorous
code auditing and path coverage.

*   Traditional decoders need Galois Field (:math:`GF(2^m)`) arithmetic
    libraries, and every line of those widens the surface for bugs.
*   XOR-popcount is mathematically simple, branch-free, and far easier to
    verify and test.

5. Radiation Resilience (SEU Mitigation)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
By using a static Look-Up Table (LUT) stored in ``const`` memory (mapped to
Flash/PROM/MRAM):

*   Reference data held there resists Single Event Upsets better than the
    transient register data an algebraic calculation would carry.
*   The simplicity of the bit-parallel loop reduces the "vulnerability window"
    during which an alpha particle could disrupt a calculation.

Hardware Performance Targets
----------------------------

On a 100 MHz LEON3 processor, the 400-iteration XOR-Popcount loop is
estimated to complete in under **20 microseconds**, well within the 12.0s
frame processing budget.
