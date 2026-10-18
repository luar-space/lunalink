LDPC Sum-Product Decoder: Algorithm Card
==========================================

.. _spec-references:

**Specification references:**

- LSIS V1.0 — *LunaNet Signal-In-Space Recommended Standard — Augmented
  Forward Signal*, Vol. A, 29 January 2025 (local copy in
  ``docs/references/``) — §2.4.3: LDPC encoding, frame structure
- `3GPP TS 38.212 <https://www.3gpp.org/DynaReport/38212.htm>`_ — Base Graph 2
  (BG2) structure shared with 5G NR
- Competition requirements: ``BER < 10⁻⁵`` at ``SNR > 0 dB`` (LSIS V1.0
  sets no error-rate requirement)

Executive Summary
-----------------

The LSIS-AFS LDPC decoder uses **layered sum-product belief propagation**
(`belief propagation <https://en.wikipedia.org/wiki/Belief_propagation>`_) with
check-node updates via the
`φ-transform <https://en.wikipedia.org/wiki/Low-density_parity-check_code#Decoding>`_
identity. ``float64`` arithmetic with syndrome-check early termination every
iteration. For the SF2 code (``n=2400``, ``k=1200``, rate 1/2), the
``FER = 1%`` cliff is at **~1.19 dB Eb/N0**, a **1.0 dB**
`Shannon gap <https://en.wikipedia.org/wiki/Noisy-channel_coding_theorem>`_,
dominated entirely by the code design (block length and base graph), not the
decoder algorithm.

Codes Under Test
----------------

Code parameters per `LSIS V1.0 §2.4.3.1.2 <spec-references_>`_ and Table 16.
The parity-check matrix ``H = [A B 0; C D I]`` follows the structure in LSIS
Figure 10, with sub-matrices defined in Annex 1.

.. list-table::
   :header-rows: 1
   :widths: 12 12 12 10 12 12

   * - Subframe
     - Mother (n)
     - Information (k)
     - Rate
     - Output (n)
     - Punctured (z)
   * - SF2 (``Zc=120``)
     - 6240
     - 1200
     - 1/2
     - 2400
     - 240
   * - SF3/SF4 (``Zc=88``)
     - 4576
     - 870 (880 with filler)
     - 1/2
     - 1740
     - 176

Decoder Configuration
---------------------

- **Algorithm:** Layered
  `sum-product belief propagation <https://en.wikipedia.org/wiki/Low-density_parity-check_code#Decoding>`_
- **Check-node update:** φ-transform, ``φ(x) = ln(1 + 2/expm1(|x|))``, self-inverse
- **Arithmetic:** IEEE 754 ``float64`` (double precision)
- **Max iterations:** 50 (default)
- **Early termination:**
  `Syndrome check <https://en.wikipedia.org/wiki/Syndrome_decoding>`_ every iteration
- **Hardening:**
  `CFI <https://en.wikipedia.org/wiki/Control-flow_integrity>`_ sentinels,
  bounds-checked array access,
  `TMR <https://en.wikipedia.org/wiki/Triple_modular_redundancy>`_ mirroring on
  gather state (``phi_sum``, ``sign``), row-count ``volatile`` counter, secure scrub on exit
- **Complexity:** ``O(E)`` per iteration, where ``E`` is the number of non-zero
  entries in ``H`` (SF2: ``E ≈ 25,600``, SF3: ``E ≈ 18,800``). Each entry requires
  one ``phi_func()`` evaluation (two transcendentals) plus gather/scatter bookkeeping.
- **Scope:** The decoder is specialised for the two LSIS-AFS parity-check matrices
  (SF2 and SF3/SF4). It does not accept arbitrary ``H`` matrices: the CSR table
  data is compiled in at build time from Annex 1.
- **Memory:** ``~342 KB`` per ``LdpcWorkspace`` (heap-allocated). Each workspace
  is independent, so the decoder is fully **re-entrant** and **thread-safe** when
  each call uses its own workspace.

Channel Model
-------------

- **Modulation:** `BPSK <https://en.wikipedia.org/wiki/Phase-shift_keying#Binary_phase-shift_keying_(BPSK)>`_
  (LSIS §2.3.2, Table 8: logic ``1 → −1.0``, logic ``0 → +1.0``)
- **Channel:** `AWGN <https://en.wikipedia.org/wiki/Additive_white_Gaussian_noise>`_
  (additive white Gaussian noise)
- **LLR computation:**
  `Log-likelihood ratio <https://en.wikipedia.org/wiki/Log-likelihood_ratio>`_
  ``L_ch(y) = 2y/σ²``, where ``y`` is the received sample
  and ``σ² = 1/(2·R·Eb/N0)`` is the noise variance for code rate ``R``
- **Positive LLR → bit 0, negative → bit 1**
- **SNR convention:** All plots use ``Eb/N0`` (energy per **information** bit to
  noise spectral density). For rate ``R = 1/2``:
  ``Eb/N0 = Es/N0 + 3 dB``, where ``Es/N0`` is the energy per coded symbol.

Waterfall Performance
---------------------

SF2 (``n=2400``, ``k=1200``, rate 1/2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /_static/algo_card/waterfall_sf2.png
   :width: 100%
   :alt: SF2 FER/BER waterfall curve

.. include:: /_static/algo_card/waterfall_sf2_table.rst.inc

SF3 (``n=1740``, ``k=870``, rate 1/2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /_static/algo_card/waterfall_sf3.png
   :width: 100%
   :alt: SF3 FER/BER waterfall curve

.. include:: /_static/algo_card/waterfall_sf3_table.rst.inc

Subframe Comparison
~~~~~~~~~~~~~~~~~~~

.. image:: /_static/algo_card/waterfall_comparison.png
   :width: 100%
   :alt: SF2 vs SF3 waterfall overlay

**Note:** SF4 reuses the SF3 parity-check matrices (LSIS §2.4.3.1.2) and
produces identical BER performance. It is not plotted separately.

Shannon Gap Analysis
--------------------

.. image:: /_static/algo_card/shannon_gap.png
   :width: 100%
   :alt: Shannon gap decomposition

The `Shannon limit <https://en.wikipedia.org/wiki/Noisy-channel_coding_theorem>`_
for rate-1/2 BPSK over
BI-AWGN (binary-input `AWGN <https://en.wikipedia.org/wiki/Additive_white_Gaussian_noise>`_) is
**0.19 dB Eb/N0**. The sum-product decoder reaches ``FER < 1%`` at approximately
**1.19 dB**, giving a total Shannon gap of **~1.0 dB**.

This gap is entirely attributable to the code design (block length ``n=2400`` and
the LSIS-AFS
`base graph <https://en.wikipedia.org/wiki/Low-density_parity-check_code#Code_construction>`_
structure). The sum-product algorithm is the optimal message-passing rule for
this graph. The ``phi_func()`` implementation introduces clamping at
``|x| < 1e-10`` (returns 30.0, error 6.3 on a near-zero message) and
``|x| > 30`` (returns 0.0, error 1.9e-13). Both clamp regions correspond to
messages with negligible influence on hard decisions; the clamping error does
not shift the FER=1% cliff by a measurable amount (< 0.01 dB).

**Finite-length bound note:** The
`Polyanskiy-Poor-Verdú <https://doi.org/10.1109/TIT.2010.2043769>`_ normal
approximation (converse bound) is negligible at ``n=2400`` in the waterfall
region (``FER < 10⁻⁷⁰``). This confirms that the gap is dominated by the
code's `distance properties <https://en.wikipedia.org/wiki/Hamming_distance>`_,
not finite block-length effects.

**3GPP BG2 comparison:** The LSIS-AFS codes are derived from the
`3GPP NR <https://www.3gpp.org/DynaReport/38212.htm>`_ Base Graph 2 structure.
Published 3GPP RAN1 evaluations for BG2 at comparable block lengths
(``n=2560``, rate 1/2) report waterfall cliffs at approximately 1.0–1.2 dB
Eb/N0 with sum-product decoding. Our result of **1.19 dB** is consistent with
these references, accounting for the slight difference in lifting size
(``Zc=120`` vs ``Zc=160``) and puncturing pattern.

Error Floor
-----------

.. include:: /_static/algo_card/error_floor_table.rst.inc

High-frame-count runs at 2.5 dB and 3.0 dB probe for
`error floors <https://en.wikipedia.org/wiki/Error_floor>`_ caused by
trapping sets
in the `Tanner graph <https://en.wikipedia.org/wiki/Tanner_graph>`_. The
`Wilson score <https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval#Wilson_score_interval>`_
95% CI upper bound provides a statistical limit on the unobserved FER.

Convergence Speed
-----------------

.. image:: /_static/algo_card/convergence_cdf.png
   :width: 100%
   :alt: Convergence CDF

At ``Eb/N0 = 1.5 dB`` (waterfall edge), 95% of frames converge within
**10 iterations**, with a mean of approximately **7 iterations**. The default
budget of 50 iterations provides >7× headroom above the mean. Frames that fail
to converge after 50 iterations are noise-limited, not iteration-limited.

Implementation Loss Budget
--------------------------

.. image:: /_static/algo_card/loss_budget.png
   :width: 100%
   :alt: Implementation loss budget

Four configurations at ``Eb/N0 = 1.2 dB`` (mid-waterfall):

1. **Baseline** — ``float64`` channel LLRs, 50 iterations
2. **int8 quantised LLRs** — 8-bit ADC with peak-normalised scaling
3. **int4 quantised LLRs** — 4-bit ADC (extreme)
4. **15 iterations** — reduced iteration budget (3× less than default)

The baseline is the reference. ``int8`` quantisation adds negligible loss.
``int4`` and reduced iterations show measurable degradation, which quantifies where
each fraction of a dB goes.

Quantisation Sweep
------------------

.. image:: /_static/algo_card/quantisation_sweep.png
   :width: 100%
   :alt: Quantisation sweep

FER at ``1.2 dB`` as a function of LLR quantisation precision (3-bit through
``float``). Demonstrates that 5-bit or higher LLR precision has negligible
impact on decoder performance.

Codeword Symmetry Validation
-----------------------------

.. include:: /_static/algo_card/symmetry_table.rst.inc

For `linear codes <https://en.wikipedia.org/wiki/Linear_code>`_ over symmetric
channels (BPSK AWGN), error performance is theoretically independent of the
transmitted codeword. Empirically, all-zero messages show lower FER than
random messages. The mechanism is **puncturing**, not filler bits: the first
``2·Zc`` systematic bits (240 for SF2, 176 for SF3/SF4) are never
transmitted, and the decoder initialises their LLRs to 0, meaning "no channel
information" (``load_channel_llrs`` in ``ldpc.cpp``). The hard-decision rule
maps a non-negative LLR to bit 0, so any punctured position that belief
propagation fails to pin down resolves to 0: always correct for the
all-zero message, wrong about half the time for a random message.
Shortening is a distinct, symmetric mechanism: the 10 SF3/SF4 filler bits
are *known* zeros regardless of the message and are initialised to a
high-confidence positive LLR (``kFillerLlr = 250``); SF2 has no filler bits
at all. The asymmetry therefore sits in the interaction of the code's
puncturing with the decoder's zero-LLR tie-break, not in the BP update.

All performance measurements in this document use **random messages**, which
is the conservative (harder) case. The all-zero test confirms the
puncturing effect is understood, not that the decoder is
pattern-independent.

Bit-Error Forensics
-------------------

.. image:: /_static/algo_card/error_patterns.png
   :width: 100%
   :alt: Bit-error pattern histogram

When the decoder fails, the histogram of bit errors per failed frame reveals
the failure mode at three operating points:

- **1.0 dB** (deep waterfall, FER ~5%): errors cluster around ``k/2 ≈ 600``,
  the expected value for non-converged output (~50% bit error rate).
- **1.2 dB** (mid-cliff, FER ~1%): the key operating region. Do near-misses
  appear, or are failures still catastrophic?
- **1.4 dB** (near error-free, FER ~0.1%): rare failures at the tail of the
  waterfall.

This is characteristic of BP on short LDPC codes. Failures are binary: either
the decoder converges to the correct codeword, or it fails to converge and the
output carries no useful information. The
`CRC-24 <https://en.wikipedia.org/wiki/Cyclic_redundancy_check>`_ on each
subframe (LSIS Appendix H) reliably detects all non-converged frames.

Boundary Robustness
-------------------

Saturation stress tests at extreme SNR points verify graceful degradation:

- ``Eb/N0 = 0.1 dB`` (below capacity; the Shannon limit is ``0.19 dB``): decoder
  returns ``kNotConverged`` for nearly all frames. No crashes, no
  ``kFaultDetected``.
- ``Eb/N0 = 10.0 dB`` (high SNR, LLR saturation): zero
  errors, zero non-converged. The decoder handles large LLR magnitudes correctly.

Spec Compliance
---------------

**Requirement:** ``BER < 10⁻⁵`` at ``SNR > 0 dB``
(from the competition requirements).

**Coding gain** over uncoded BPSK: approximately **7.5 dB** at ``BER = 10⁻⁵``
(uncoded BPSK requires ``Eb/N0 ≈ 9.6 dB`` for ``BER = 10⁻⁵``; the coded
system achieves it below ``2.0 dB``).

Two interpretations are addressed:

- **Interpretation A** (``Es/N0 > 0 dB`` → ``Eb/N0 = 3.0 dB`` for rate 1/2):
  **zero errors** in 15,000 frames (18M bits for SF2, 13M bits for SF3).
  Trivially satisfied.
- **Interpretation B** (``Eb/N0 > 0 dB``): the waterfall cliff is
  at ``~1.2 dB``. At ``2.0 dB``, FER is zero in the high-resolution waterfall.
  At ``2.5 dB``, the error floor probe (60,000 frames) provides a statistical
  upper bound.

Test Methodology
----------------

- **PRNG:** `Mersenne Twister <https://en.wikipedia.org/wiki/Mersenne_Twister>`_
  (``mt19937``) with fixed seeds (``42``, ``137``, ``313``) for reproducibility
- **Message generation:** uniform random binary per frame (validated against
  all-zero in symmetry test)
- **Multi-seed aggregation:** 3 seeds per configuration, results combined for
  Wilson score 95% confidence intervals
- **Confidence intervals:**
  `Wilson score <https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval#Wilson_score_interval>`_
  method, which handles ``p=0`` and ``p=1`` gracefully and is preferred over
  `Wald intervals <https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval#Problems_with_using_a_normal_approximation_or_%22Wald_interval%22>`_
  for small error counts
- **Parallelisation:** work items distributed across all available CPU cores via
  atomic work queue (each simulation is independent)
- **Build configuration:** CMake Release mode (``-O3``), ``-fno-fast-math``,
  ``-ffp-contract=off``

How to Reproduce
----------------

.. code-block:: bash

   task ldpc-algo-card

This runs the full pipeline:

1. Builds ``ldpc_characterise`` (parallel C++ runner, Release mode)
2. Runs ~80 work items across all CPU cores (≈25 minutes on 10 threads)
3. Outputs ``build/algo_card/sp_results.json`` and copies it to
   ``docs/assurance/evidence/ldpc_algo_card.json``
4. Generates figures to ``docs/_static/algo_card/``

All results are deterministic (fixed seeds).  The figures and the results
JSON are committed, so ``task docs-build`` renders this page from the
submission run without repeating the campaign; run ``task ldpc-algo-card``
to regenerate both.
