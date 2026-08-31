Gateway 2: Forward Error Correction — Judge's Verification Guide
================================================================

This document maps each Gateway 2 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs the full unit-test suite
   task ber                   # runs BER simulations; slow, run once to verify

All unit tests must pass with exit code 0.  The performance suite is
excluded from the default run because it takes several minutes; the key
numerical results are recorded in this document for reference.

----

GD2.1 — BCH(51,8) Encoder and Decoder
--------------------------------------

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
caller-supplied buffer — no dynamic allocation, no exceptions.

Hard-decision ML decoder (``bch_decode``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exhaustive ML over the 400-entry codebook using Hamming distance.  Always
returns the nearest codeword regardless of error count.  Unlike an
algebraic t=2 decoder, it never declares failure due to excess errors; it
simply returns the best available estimate.

Soft-decision ML decoder (``bch_decode_soft``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Accepts 52 float channel LLRs (L\ :sub:`ch` = 2y / σ²).  Maximises the
inner-product metric Σ llr\ :sub:`j` × (1 − 2c\ :sub:`j`) over all 400
codebook entries.  This is the optimal (maximum-likelihood) decision rule
for BPSK AWGN — it retains reliability information discarded by hard
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

File: ``cpp/tests/performance/test_bch_ber.cpp``.  BPSK AWGN simulation comparing
hard ML vs soft ML over 1,000–10,000 frames per Eb/N0 point.

.. list-table::
   :header-rows: 1
   :widths: 15 18 18 15 20

   * - Eb/N0 (dB)
     - Hard FER
     - Soft FER
     - Frames
     - Soft gain
   * - 2.0
     - 1.49 × 10\ :sup:`-1`
     - 2.10 × 10\ :sup:`-2`
     - 1,000
     - 7×
   * - 3.0
     - 6.70 × 10\ :sup:`-2`
     - 5.67 × 10\ :sup:`-3`
     - 3,000
     - 12×
   * - 4.0
     - 1.93 × 10\ :sup:`-2`
     - 2.00 × 10\ :sup:`-3`
     - 3,000
     - 10×
   * - 5.0
     - 6.33 × 10\ :sup:`-3`
     - 3.33 × 10\ :sup:`-4`
     - 3,000
     - 19×
   * - 6.0
     - 1.67 × 10\ :sup:`-3`
     - **0**
     - 3,000
     - ∞
   * - **7.6** (operating point)
     - **0**
     - **0**
     - **10,000**
     - —

Key claim: the soft decoder reaches effective error-free operation ~2 dB
before the hard decoder.  At the system operating point (Es/N0 = 0 dB →
Eb/N0 = 7.6 dB for BCH R=9/52), zero frame errors in 10,000 frames bounds
FER < 3 × 10\ :sup:`-4` at 95% confidence.

Full discussion: ``docs/signal/bch_performance.rst``

----

GD2.2 — LDPC Encoder and Decoder (5G NR, SF2/SF3/SF4)
-------------------------------------------------------

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
     - 40
   * - SF3
     - 4560
     - 1740
     - 870
     - 1/2
     - 176 bits
     - 29
   * - SF4
     - 4560
     - 1740
     - 870
     - 1/2
     - 176 bits
     - 29 (reuses SF3 matrices)

Implementation files:

- ``cpp/include/lunalink/encoding/ldpc.hpp`` — public API, workspace types
- ``cpp/encoding/ldpc.cpp`` — encoder (two-stage sparse), decoder (layered sum-product BP)
- ``cpp/encoding/ldpc_tables.cpp`` — 10 sparse parity matrices (CSR format)

Encoder (``ldpc_encode``)
~~~~~~~~~~~~~~~~~~~~~~~~~

Two-stage sparse encoding: p1 = B\ :sup:`-1`·(A·s), p2 = C·s + D·p1
(mod 2).  A post-encode H·c=0 parity check verifies correctness and
detects faults before the codeword leaves the encoder.

Decoder (``ldpc_decode``) — Layered Sum-Product Belief Propagation
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

BER results — system operating point
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The LSIS spec requires BER < 10\ :sup:`-5` at SNR > 0 dB.  Two
interpretations of the SNR convention are addressed:

.. list-table::
   :header-rows: 1
   :widths: 20 25 25 30

   * - Interpretation
     - Definition
     - Test point
     - Measured result
   * - **A (primary)** Es/N0 > 0 dB
     - Energy-per-symbol (standard for coded systems)
     - Eb/N0 = 3.0 dB
     - BER = 0, FER = 0 in 18,000,000 bits (SF2) and 13,050,000 bits (SF3)
   * - **B (conservative)** Eb/N0 > 0 dB
     - Energy-per-bit
     - Eb/N0 = 2.0 dB
     - BER = 0 in 18,000,000 bits (SF2, 95% upper bound 1.7 × 10\ :sup:`-7`)
       and 13,050,000 bits (SF3, 95% upper bound 2.3 × 10\ :sup:`-7`)

Full discussion: ``docs/signal/ldpc_performance.rst``

----

GD2.3 — Block Interleaver / De-interleaver (60×98)
---------------------------------------------------

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

GD2.4 — CRC-24 Generator and Validator
---------------------------------------

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
cover GD2 end-to-end paths.

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

All four GD2 components are exposed to Python via the ``lunalink.afs``
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

   task test-py       # runs pytest suite including GD2 binding tests

----

Interoperability Test Vectors
------------------------------

Pre-generated static test vectors are committed under ``test-vectors/``.
They can be regenerated at any time from the live implementation:

.. code-block:: bash

   python tests/generate_test_vectors.py

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - File
     - Vectors
     - Description
   * - ``fec/bch_vectors.json``
     - 24
     - BCH encode/decode pairs for all 4 FIDs × 6 TOI spot values
   * - ``fec/ldpc_vectors.json``
     - 12
     - LDPC encode for SF2/SF3/SF4 × 4 message patterns (hex-truncated)
   * - ``fec/crc24_vectors.json``
     - 8
     - CRC-24Q input → 24-bit CRC pairs with verify flag
   * - ``fec/interleaver_vectors.json``
     - 4
     - 5880-bit round-trip pairs with spot-check position mapping

Validate them against the current build:

.. code-block:: bash

   uv run pytest tests/test_interop_self.py

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 20 30

   * - Suite
     - Tests
     - Assertions
     - Run time
     - How to run
   * - GD2 unit + integration
     - 40
     - ---
     - seconds
     - ``task test-cpp``
   * - BCH BER simulation
     - 3
     - 26,011
     - ~30 s
     - ``task ber``
   * - LDPC reference comparison + frame FER sweep
     - 2
     - ---
     - ~5 min
     - ``task ber``
   * - Python bindings
     - included in pytest
     - —
     - < 5 s
     - ``task test-py``

All C++ unit tests pass in a few seconds with exit code 0.
All performance tests pass with the BER/FER bounds stated in this document.

Related documents:

- ``docs/signal/bch_performance.rst`` — BCH hard vs soft characterisation
- ``docs/signal/ldpc_performance.rst`` — LDPC BER waterfall characterisation
- ``docs/signal/compliance_matrix.rst`` — full spec-to-code mapping
