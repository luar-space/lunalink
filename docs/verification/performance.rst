Timing and channel performance
==============================

This page says where each performance result is measured and how to reproduce
it.  The bit-error-rate target comes from LSIS V1.0.  The timing targets come
from the rubric of the competition LunaLink was first built for; the standard
sets none.

.. code-block:: bash

   task performance   # timing benchmarks and bit-error-rate simulations
   task wcet          # timing benchmarks only
   task ber           # bit-error-rate and frame-error-rate sweeps only

The output of the committed run is in ``docs/assurance/evidence/``, as
``wcet_results.xml`` and ``ber_results.xml``.

Timing
------

.. list-table::
   :widths: 34 26 20 20
   :header-rows: 1

   * - Quantity
     - Target
     - Measured
     - Source
   * - Spreading-code generation
     - < 1 s per PRN
     - < 10 ms
     - ``task wcet``
   * - Frame encoding
     - < 100 ms per frame
     - < 5 ms
     - ``task wcet``
   * - Frame decoding
     - < 1 s per frame
     - Asserted in ``test_e2e.cpp``
     - ``task integration``
   * - Whole chain
     - Faster than the 12 s the signal lasts
     - < 1 s
     - ``task wcet``

Timing that does not depend on the input
----------------------------------------

``task constant-time`` times the primitives whose execution should not depend
on their input: the BPSK modulator, the CRC-24, the interleaver and the
exhaustive maximum-likelihood BCH decoder.  It runs each on all-zero, all-one
and pseudo-random inputs and fails if the spread between input classes exceeds
14%.  That ceiling is twice the worst value measured over eight idle runs.

The LDPC decoder and the frame decoder are excluded.  They stop early when
the syndrome is satisfied, so their time does depend on the input.  Their
worst case is bounded by the budgets that ``task wcet`` asserts with early
termination disabled.  See the section on time-deterministic execution in
:doc:`/assurance/mission_assurance`.

Channel performance
-------------------

The error-rate results are kept on the pages that own them:

- :doc:`/signal/ldpc_performance` has the LDPC bit and frame error rates with
  confidence intervals, measured under both readings of "SNR > 0 dB".
- :doc:`/signal/ldpc_algo_card` compares the shipped decoder with a reference
  decoder and the finite-length bound, and has the error-floor search, the
  convergence results and the quantisation sweep.
- :doc:`/signal/bch_performance` compares hard-decision and soft-decision BCH
  decoding.
