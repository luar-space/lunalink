Forward error correction
========================

The forward-error-correction block implements the BCH(51,8) code that protects
subframe 1, the rate-1/2 LDPC codes for subframes 2 to 4, the 60×98 block
interleaver and the CRC-24 (LSIS V1.0 §2.4).

This page lists the parameters, source files, interface, hardening and tests
of each part, with the command that runs the tests.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs the full unit-test suite
   task ber                   # runs BER simulations; slow, run once to verify

All unit tests must pass with exit code 0.  The performance suite is
excluded from the default run because it takes several minutes; its results
are in :doc:`/signal/bch_performance` and :doc:`/signal/ldpc_performance`.

----

BCH(51,8) Encoder and Decoder
-----------------------------

Competition deliverable: GD2.1.

Spec reference: LSIS-AFS-501

Code parameters
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Parameter
     - Value
     - Description
     - Source
     - Note
   * - n
     - 52 bits
     - Codeword length
     - ``kBchCodewordLength``
     - 1 raw MSB + 51 LFSR symbols
   * - k
     - 9 bits
     - Information bits
     - Implicit in FID(2b) + TOI(7b)
     - 4 FIDs × 100 TOIs = 400 valid messages
   * - d\ :sub:`min`
     - ≥ 5
     - Minimum Hamming distance
     - Codebook exhaustive check
     - Guarantees t=2 correction
   * - R
     - 9/52 ≈ 0.173
     - Code rate
     - —
     - Low rate → large Eb/N0 margin vs LDPC

Implementation files:

- ``cpp/include/lunalink/encoding/bch.hpp`` — public API
- ``cpp/encoding/bch.cpp`` — encoder, hard-ML decoder, soft-ML decoder
- ``cpp/encoding/bch_table.cpp`` — 400-entry codebook (packed 64-bit words)

Encoder (``bch_encode``)
~~~~~~~~~~~~~~~~~~~~~~~~~

Generates a 52-bit codeword from a FID ∈ {0,1,2,3} and TOI ∈ {0..99}
using a linear-feedback shift register.  Output is written to a
caller-supplied buffer, with no dynamic allocation and no exceptions.

Hard-decision ML decoder (``bch_decode``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exhaustive ML over the 400-entry codebook using Hamming distance.  It
always fills in the FID and TOI of the nearest codeword, and the status
says how far to trust them: ``kOk`` within distance 2, the code's
guaranteed correction radius; ``kNullOutput`` beyond it; and
``kAmbiguousMatch`` when two codewords tie.  A caller can therefore reject
the header, as an algebraic t=2 decoder would, or take the ML estimate.
``frame_decode`` uses the soft decoder below, which has no distance
check.

Soft-decision ML decoder (``bch_decode_soft``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Accepts 52 float channel LLRs (L\ :sub:`ch` = 2y / σ²).  Maximises the
inner-product metric Σ llr\ :sub:`j` × (1 − 2c\ :sub:`j`) over all 400
codebook entries.  This is the optimal (maximum-likelihood) decision rule
for BPSK AWGN: it retains reliability information discarded by hard
thresholding.

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - SEU-resistant status codes
     - All ``BchStatus`` enum values have Hamming distance ≥ 4 between each other (non-contiguous patterns)
   * - TMR-hardened ``Fid`` and ``Toi``
     - Triple-replicated storage with 2-of-3 majority vote on every read
   * - Control Flow Integrity
     - ``cfi_count`` sentinel incremented every codebook iteration; verified after the loop; returns ``kFaultDetected`` on mismatch
   * - Codebook integrity
     - ``bch_codebook_checksum()`` checked at startup (test: *Coverage Booster: bch_codebook_checksum stable*)
   * - Memory scrubbing
     - Temporaries cleared via ``secure_scrub`` before return

Unit tests
~~~~~~~~~~

Run: ``task test-cpp``  (file: ``cpp/tests/encoding/test_bch.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - BCH decoder: zero errors
     - Clean codeword decodes to correct FID/TOI
   * - BCH decoder: single error correction
     - 1-bit flip corrected to nearest codeword
   * - BCH decoder: double error correction
     - 2-bit flips corrected (at d\ :sub:`min` ≥ 5 guarantee boundary)
   * - BCH decoder: confidence threshold (NASA safety)
     - Hamming-distance threshold guards against over-confident decisions
   * - BCH decoder: ambiguity detection
     - ``kAmbiguousMatch`` returned when two codewords tie
   * - BCH codebook exhaustive check
     - All 400 entries decode to their own FID/TOI
   * - BCH codebook checksum
     - Compiled-in checksum matches runtime computation
   * - BCH soft decoder: clean symbols
     - Perfect BPSK LLRs (±1) decode all 400 entries correctly
   * - BCH soft decoder: noisy symbols corrected
     - 3 soft-flipped bits corrected; inner-product metric still selects correct entry
   * - BCH soft decoder: exhaustive codebook
     - All 400 entries verified under soft-decision decoding

Performance tests (``task ber``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

File: ``cpp/tests/performance/test_bch_ber.cpp``.  A BPSK simulation over an
additive white Gaussian noise channel compares the hard-decision and
soft-decision decoders across a range of Eb/N0, including the system
operating point.  The frame error rates and their confidence bounds are in
:doc:`/signal/bch_performance`.

----

LDPC Encoder and Decoder (5G NR, SF2/SF3/SF4)
---------------------------------------------

Competition deliverable: GD2.2.

Spec reference: LSIS-AFS-320

Code parameters
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 15 15 15 15 15 10

   * - Subframe
     - Mother n
     - Output n
     - k
     - Rate
     - Punctured
     - Lifted z
   * - SF2
     - 6240
     - 2400
     - 1200
     - 1/2
     - 240 bits
     - 120
   * - SF3
     - 4576
     - 1740
     - 870
     - 1/2
     - 176 bits
     - 88
   * - SF4
     - 4576
     - 1740
     - 870
     - 1/2
     - 176 bits
     - 88 (reuses SF3 matrices)

Implementation files:

- ``cpp/include/lunalink/encoding/ldpc.hpp`` — public API, workspace types
- ``cpp/encoding/ldpc.cpp`` — encoder (two-stage sparse), decoder (layered sum-product BP)
- ``cpp/encoding/ldpc_tables.cpp`` — 10 sparse parity matrices (CSR format)

Encoder (``ldpc_encode``)
~~~~~~~~~~~~~~~~~~~~~~~~~

Two-stage sparse encoding: p1 = B\ :sup:`-1`·(A·s), p2 = C·s + D·p1
(mod 2).  A post-encode H·c=0 parity check verifies correctness and
detects faults before the codeword leaves the encoder.

Decoder (``ldpc_decode``): Layered Sum-Product Belief Propagation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **Algorithm:** Layered Sum-Product BP (φ-transform formulation, with a
  quantified numerical-clamping analysis in ``ldpc.cpp``)
- **Arithmetic:** float64
- **Max iterations:** 50 (caller-configurable)
- **Early termination:** syndrome check every iteration
- **Decoder quality:** within 0.15 dB of reference belief-propagation
  implementations on this code (3-way gap decomposition,
  ``test_ldpc_reference.cpp``)

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - SEU-resistant status codes
     - ``LdpcStatus`` and ``LdpcSubframe`` use non-contiguous bit patterns
   * - Parity table integrity
     - CRC32 over all 10 matrices verified at startup
   * - Post-encode self-check
     - H·c=0 checked on every encoded codeword; ``kFaultDetected`` returned on fail
   * - Workspace separation
     - All large temporaries (~85 KB decoder, ~14 KB encoder) in caller-supplied structs — no stack allocation, no dynamic memory

Unit tests
~~~~~~~~~~

Run: ``task test-cpp``  (file: ``cpp/tests/encoding/test_ldpc.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - LDPC Encoder: basic execution
     - SF2/SF3/SF4 encode with random messages; H·c=0 self-check passes
   * - LDPC Encoder: error paths
     - Invalid subframe, wrong lengths → correct error status
   * - LDPC: Table CRC32 integrity (all 10 matrices)
     - Runtime CRC32 matches stored constant for each of the 10 parity matrices
   * - LDPC: Matrix dimension consistency
     - All sparse matrices have correct row/column dimensions per subframe
   * - LDPC: Subframe types coverage
     - SF2, SF3, SF4 all reach encode+decode paths

Bit error rate at the operating point
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LSIS V1.0 sets no error-rate requirement.  The target used here, a bit error
rate below 10\ :sup:`-5` at SNR > 0 dB, comes from the competition
requirements.  "SNR" can be read as energy per symbol or as energy per bit,
and the decoder is measured under both readings.  The results, with their confidence bounds,
are in :doc:`/signal/ldpc_performance`; :doc:`/signal/ldpc_algo_card` compares
the decoder with a reference decoder and the finite-length bound.

----

Block Interleaver / De-interleaver (60×98)
------------------------------------------

Competition deliverable: GD2.3.

Spec reference: LSIS-FID0-470

Dimensions: 60 rows × 98 columns = 5880 bits.
Write row-wise, read column-wise (standard block interleaver permutation).

Implementation files:

- ``cpp/include/lunalink/encoding/interleaver.hpp``
- ``cpp/encoding/interleaver.cpp``

Hardening
~~~~~~~~~

CFI sentinels and mirrored address calculation (Category B hardening):
the output index is computed twice via independent paths; a mismatch
triggers ``kFaultDetected`` before any write.

Unit tests (file: ``cpp/tests/encoding/test_interleaver.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - Block Interleaver: Round-trip correctness
     - interleave → deinterleave recovers the original for all-zeros, all-ones, alternating, and random patterns
   * - Block Interleaver: Specific mapping check
     - Known input positions map to expected output positions (bit[0]→bit[0], bit[1]→bit[60], …)
   * - Block Interleaver: Error handling
     - Undersized input/output → correct error status; no write attempted

----

CRC-24 Generator and Validator
------------------------------

Competition deliverable: GD2.4.

Spec reference: LSIS-FID0-467

Polynomial: G(X) = 0x864CFB (CRC-24Q), seed = 0, MSB-first bit ordering.

Implementation files:

- ``cpp/include/lunalink/encoding/crc.hpp``
- ``cpp/encoding/crc.cpp``

Hardening
~~~~~~~~~

CFI counter verifies the full 24-bit output loop ran without skip;
``kFaultDetected`` returned on mismatch.

Unit tests (file: ``cpp/tests/encoding/test_crc.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - CRC-24: Basic correctness
     - ``crc24_calculate`` then ``crc24_verify`` returns true for clean data
   * - CRC-24: Round-trip and error detection
     - 1-bit flip in data causes verify to return false
   * - CRC-24: Known vector (Qualcomm/3GPP pattern)
     - Output matches published test vector for the CRC-24Q polynomial

----

Cross-Cutting: Integration and Assurance Tests
-----------------------------------------------

Beyond the per-component unit tests, the following integration tests
cover the end-to-end paths through this block.

Round-trip tests (``cpp/tests/encoding/``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Test case
     - What it verifies
   * - BCH codebook exhaustive check (``test_bch.cpp``)
     - encode → decode recovers correct FID/TOI for all 400 valid messages
   * - Gateway 2: LDPC Round-trip (``test_ldpc.cpp``)
     - encode → decode with no channel noise recovers all message bits for SF2, SF3, SF4

Safety & fault-tolerance tests (``cpp/tests/encoding/test_swifi_encoding.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SWIFI fault injection for the FEC domain covers: LDPC parity-table bit-flip
detection (integrity check fails closed), forced error-status injection on
``ldpc_encode``, and TMR detection of corrupted mirrored decoder state.
Generic TMR voting and ``CheckedRange`` saturation are exercised in
``cpp/tests/platform/test_swifi_platform.cpp``.

----

Python Bindings
---------------

All four components are exposed to Python via the ``lunalink.afs``
extension module (nanobind).  Stubs are in ``src/lunalink/afs/_afs.pyi``.

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Function
     - Signature
     - Notes
   * - ``bch_encode``
     - ``(fid, toi) → BchCodeword``
     - Returns 52-element bit array (``uint8``)
   * - ``bch_decode``
     - ``(cw: BchCodeword) → BchResult``
     - Hard-decision ML
   * - ``bch_decode_soft``
     - ``(llr: BchSoftInput) → BchResult``
     - Soft-decision ML (52 LLRs, ``float32``)
   * - ``ldpc_encode``
     - ``(sf: LdpcSubframe, msg) → NDArray[uint8]``
     - SF2/SF3/SF4 supported (variable-length output)
   * - ``ldpc_decode``
     - ``(sf, soft, max_iters) → (status, NDArray[uint8])``
     - Returns decoded bits (variable-length)
   * - ``crc24_calculate``
     - ``(data: BitArray) → BitArray``
     - Returns data + 24-bit CRC
   * - ``crc24_verify``
     - ``(data_with_crc: BitArray) → bool``
     - Appended-CRC convention
   * - ``block_interleave``
     - ``(data: InterleaverBlock) → InterleaverBlock``
     - Exactly 5880 bits (``uint8``)
   * - ``block_deinterleave``
     - ``(data: InterleaverBlock) → InterleaverBlock``
     - Exactly 5880 bits (``uint8``)

Python tests:

.. code-block:: bash

   task test-py       # runs the pytest suite, including the binding tests

----

Interoperability Test Vectors
------------------------------

The shared vector repository, `luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__, has a vector set
for each stage of this block in ``fec/``, with full inputs and outputs:

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - File
     - Vectors
     - Description
   * - ``fec/bch_vectors.json``
     - 24
     - BCH(51,8) codewords for all 4 FIDs × 6 TOI values, with decode
       round trip
   * - ``fec/crc24_vectors.json``
     - 8
     - CRC-24Q input → 24-bit CRC pairs
   * - ``fec/interleaver_vectors.json``
     - 4
     - 60×98 block interleaver, full 5880-bit input and output
   * - ``fec/ldpc_vectors.json``
     - 12
     - LDPC encode for SF2/SF3/SF4 × 4 message patterns, full codewords

``task conformance`` has LunaLink produce the same four files and runs the
repository's ``validate.py diff-fec`` on them, along with the checks for
every other level (:doc:`/interoperate`).

The BCH and CRC-24Q files are also kept under ``test-vectors/fec/`` as
offline fixtures, which ``uv run pytest tests/test_interop_self.py`` checks
against the current build.

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Suite
     - How to run
   * - Unit and integration tests
     - ``task test-cpp``
   * - BCH frame-error-rate simulation
     - ``task ber``
   * - LDPC reference comparison and frame-error-rate sweep
     - ``task ber``
   * - Python bindings
     - ``task test-py``

``task ber`` takes several minutes.

Related documents:

- :doc:`/signal/bch_performance`: hard-decision and soft-decision BCH decoding
- :doc:`/signal/ldpc_performance`: LDPC bit and frame error rates
- :doc:`/signal/compliance_matrix`: implementation status by block
