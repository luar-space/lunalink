LDPC Decoder Performance Characterization
==========================================

Empirical BER/FER for the LSIS-AFS LDPC decoder, measured against the target
set by the competition requirements (BER < 10\ :sup:`-5` at SNR > 0 dB).
LSIS V1.0 itself sets no error-rate requirement.  Every number on this page is produced by one reproducible
command, ``task ldpc-algo-card``, which runs the characterisation tool
(``scripts/ldpc_characterise.cpp``) against the decoder that ships in this
repository and regenerates the tables and figures included below.  The full
algorithm-level characterisation (convergence CDF, error patterns,
saturation stress) is in the companion :doc:`ldpc_algo_card`.

Codes Under Test
----------------

.. list-table::
   :header-rows: 1
   :widths: 15 15 15 15 15

   * - Subframe
     - Mother (n)
     - Information (k)
     - Rate
     - Punctured bits (z)
   * - SF2
     - 6240
     - 1200
     - 1/2 (output n=2400)
     - 240
   * - SF3/SF4
     - 4576
     - 870
     - 1/2 (output n=1740)
     - 176

Decoder Configuration
---------------------

- **Algorithm:** Layered Sum-Product belief propagation (φ-transform
  formulation with quantified clamping analysis; see ``ldpc.cpp``)
- **Arithmetic:** float64
- **Max iterations:** 50
- **Early termination:** syndrome check every iteration
- **Hardening:** CFI sentinels, TMR mirrors, bounds checks, scrub-on-error
  (characterised with hardening enabled, so the numbers below include its cost)

Channel Model
-------------

- **Modulation:** BPSK
- **Channel:** AWGN
- **LLR computation:** L\ :sub:`ch` = 2y / sigma\ :sup:`2` (optimal for BPSK AWGN)
- **Eb/N0 to sigma:** sigma = 1 / sqrt(2 * R * Eb/N0\ :sub:`linear`)

Target
------

**Competition target:** BER < 10\ :sup:`-5` @ SNR > 0 dB.

SNR Interpretation Note
~~~~~~~~~~~~~~~~~~~~~~~

The phrase "SNR > 0 dB" in the competition requirement is ambiguous. Two interpretations exist
and both are addressed:

.. list-table::
   :header-rows: 1
   :widths: 20 25 25 30

   * - Interpretation
     - Definition
     - Test point
     - Evidence
   * - **A (primary)** Es/N0 > 0 dB
     - Energy-per-symbol. Standard in digital-communications specs for coded systems.
     - Eb/N0 = 3.0 dB (= Es/N0 for rate-1/2)
     - 0 bit errors in 18,000,000 bits (SF2) and 13,050,000 bits (SF3/SF4)
       at 3.0 dB; a further 60,000-frame floor run at each of 2.5 and
       3.0 dB is also error-free
   * - **B (conservative)** Eb/N0 > 0 dB
     - Energy-per-bit. Requires meeting BER < 10\ :sup:`-5` at all Eb/N0 above 0 dB.
     - Eb/N0 = 2.0 dB (past the waterfall knee)
     - 0 bit errors in 18,000,000 bits (SF2) and 13,050,000 bits (SF3) at
       2.0 dB — 95% BER upper bound 1.7 × 10\ :sup:`-7` (SF2) /
       2.3 × 10\ :sup:`-7` (SF3), 60× below the 10\ :sup:`-5` target

Under Interpretation A the requirement is satisfied with wide margin.
Under Interpretation B, the measured BER at 1.6 dB is 6.6 × 10\ :sup:`-5`
(SF2), so the 10\ :sup:`-5` threshold is crossed between 1.6 dB and 2.0 dB;
at 2.0 dB the requirement is met with the margin above.  The waterfall
tables below resolve the transition with 15,000 frames per point.

BER Waterfall Results
---------------------

SF2 Waterfall (n=2400, k=1200, rate 1/2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

12 Eb/N0 points, 15,000 frames (18 M bits) each.  95% CI is the Wilson
interval half-width on FER.

.. image:: /_static/algo_card/waterfall_sf2.png
   :alt: SF2 FER/BER waterfall
   :width: 90%

.. include:: /_static/algo_card/waterfall_sf2_table.rst.inc

SF3/SF4 Waterfall (n=1740, k=870, rate 1/2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /_static/algo_card/waterfall_sf3.png
   :alt: SF3 FER/BER waterfall
   :width: 90%

.. include:: /_static/algo_card/waterfall_sf3_table.rst.inc

Shannon Gap Analysis
~~~~~~~~~~~~~~~~~~~~

The Shannon limit for rate-1/2 BPSK over AWGN is **0.19 dB Eb/N0**.  The
SF2 waterfall passes the conventional "FER < 1%" cliff-edge threshold at
**~1.2 dB Eb/N0** (FER = 0.88%), giving a **Shannon gap of ~1.0 dB**.
That is a typical result for a short code (n = 2400):

- 5G NR LDPC codes at n = 8000+: ~0.3–0.5 dB from Shannon
- Short LDPC codes (n < 3000): ~0.6–1.2 dB typical
- Classical convolutional + Viterbi at comparable rate: ~2–3 dB from Shannon

The residual gap is dominated by code length, not decoder quality: the
3-way gap decomposition in ``test_ldpc_reference.cpp`` (``task ber``)
measures the shipped decoder within 0.15 dB of reference
belief-propagation implementations on this code.

.. image:: /_static/algo_card/shannon_gap.png
   :alt: Shannon gap
   :width: 90%

Error Floor Search
~~~~~~~~~~~~~~~~~~

60,000 frames per point at 2.5 and 3.0 dB for each code gave **zero frame
errors** in all four runs (72 M bits per SF2 point, 52.2 M bits per SF3
point).  The 95% Wilson upper bound on FER is 6.4 × 10\ :sup:`-5` per
point: no error floor is observable down to that level.

.. include:: /_static/algo_card/error_floor_table.rst.inc

Convergence Speed
-----------------

FER as a function of the maximum iteration count (mid-waterfall test
point).  The convergence knee is at ~15 iterations; the shipped limit of
50 leaves generous margin, and the syndrome check terminates typical
frames long before the cap.

.. image:: /_static/algo_card/convergence_cdf.png
   :alt: Convergence CDF
   :width: 90%

Quantization Resilience
-----------------------

Input-LLR quantisation sweep at Eb/N0 = 1.2 dB (mid-waterfall, 9,000
frames per configuration):

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 45

   * - Input quantisation
     - FER
     - BER
     - Note
   * - float (baseline)
     - 0.93%
     - 4.7 × 10\ :sup:`-3`
     - reference
   * - int8 (peak-normalised AGC)
     - 0.93%
     - 4.7 × 10\ :sup:`-3`
     - **no measurable loss** — 8-bit ADC front-ends are sufficient
   * - 5-bit
     - 1.4%
     - 6.9 × 10\ :sup:`-3`
     - ~0.5% FER penalty
   * - 4-bit
     - 5.6%
     - 2.8 × 10\ :sup:`-2`
     - visible degradation
   * - 3-bit
     - 97.5%
     - —
     - insufficient resolution at this SNR

.. image:: /_static/algo_card/quantisation_sweep.png
   :alt: Quantisation sweep
   :width: 90%

Decoder Symmetry Check
----------------------

For a linear code over a symmetric channel (BPSK AWGN) with a symmetric
decoder, error performance is codeword-independent in theory.  Measured at
the mid-waterfall point: random-codeword FER 0.88% vs all-zero FER 0% at
15,000 frames each.  The all-zero case is easier, and the
mechanism is the interaction of **puncturing** with the hard-decision
tie-break, not the BP update itself: the first 2·Zc systematic bits
(240 for SF2) are never transmitted, the decoder initialises their LLRs
to 0 ("no channel information"), and the hard-decision rule maps a
non-negative LLR to bit 0.  That is always correct for the all-zero message and
wrong for roughly half the punctured positions of a random message
whenever BP fails to pin them down.  (Shortening is a separate mechanism:
the SF3/SF4 filler bits are *known* zeros and are initialised to a
high-confidence positive LLR; SF2, the code used in this test, has no
filler bits.  See :doc:`ldpc_algo_card` for the same analysis.)
All headline results above therefore use random payloads, the
conservative, harder case.

Channel LLR Scaling
-------------------

The decoder expects **channel LLRs**, not raw soft symbols.  For BPSK
AWGN::

    L_ch = 2y / sigma^2

This scaling provides the decoder with reliability information.  Without
it, convergence degrades sharply.  ``frame_decode`` takes the factor as
``llr_scale`` and applies it to the de-interleaved payload only, because the
sync detector ahead of it needs unit-amplitude symbols.
:func:`lunalink.afs.receive_frame` takes ``noise_variance`` and derives the
factor from it.  Both default to no scaling.  C++ callers of ``ldpc_decode``
must supply LLRs per the header contract in ``ldpc.hpp``.

Test Methodology
----------------

- Every point above: ``task ldpc-algo-card`` (≈25 minutes on 10 threads;
  seeds fixed, results bit-reproducible) → ``build/algo_card/sp_results.json``
  plus the figures and table includes under ``docs/_static/algo_card/``.
- Independent pass/fail acceptance: ``task ber`` runs the
  ``[channel]``-tagged Catch2 tests (BCH FER sweeps, LDPC reference-decoder
  gap decomposition, sync detection rate) with Wilson-interval-aware
  assertions; ``task performance`` adds the ``[wcet]`` timing gates.
- The spec-compliance verdict feeds :doc:`compliance_matrix` (CM-321)
  and NFR-2.2 in ``scripts/compliance_report.py``.
