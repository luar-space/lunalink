Gateway 1: Spreading Code Generation --- Verification Guide
============================================================

This document maps each Gateway 1 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs all unit tests (< 2 s)
   task test-py               # runs Python binding tests

All unit tests must pass with exit code 0.

----

GD1.1 --- Spreading Code Loader (Gold, Weil, Legendre)
-------------------------------------------------------

Spec references: LSIS-AFS-101 (Gold), LSIS-AFS-102 (Weil)

Code parameters
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Family
     - Chips
     - PRN IDs
     - Description
   * - Gold-2046
     - 2046
     - 1--210
     - AFS-I primary ranging code; dual-LFSR (G1: x\ :sup:`11`\ +x\ :sup:`2`\ +1, G2: x\ :sup:`11`\ +x\ :sup:`8`\ +x\ :sup:`5`\ +x\ :sup:`2`\ +1)
   * - Weil-10230
     - 10230
     - 1--210
     - AFS-Q primary ranging code; Legendre + Weil construction with preamble insertion
   * - Weil-1500
     - 1500
     - 1--210
     - AFS-Q tertiary code; same construction at shorter length

Implementation files:

-  ``cpp/include/lunalink/signal/prn.hpp`` --- public API (loaders, unpack, checksums)
-  ``cpp/signal/prn.cpp`` --- loader implementation
-  ``cpp/include/lunalink/signal/gold.hpp`` --- Gold generator class
-  ``cpp/include/lunalink/signal/weil.hpp`` --- Weil generator class
-  ``cpp/include/lunalink/signal/legendre.hpp`` --- Legendre symbol calculator
-  ``cpp/signal/prn_table.cpp`` --- Gold-2046 packed LUT (52.5 KB, 210 PRNs)
-  ``cpp/signal/prn_table_weil10230.cpp`` --- Weil-10230 packed LUT (262.3 KB)
-  ``cpp/signal/prn_table_weil1500.cpp`` --- Weil-1500 packed LUT (38.6 KB)

Table generation
~~~~~~~~~~~~~~~~

Tables are generated from Annex 3 reference hex files via
``scripts/gen_prn_table.py``:

1. Read hex from ``docs/references/006_GoldCode2046hex210prns.txt`` (Gold),
   ``007_l1cp_hex210prns.txt`` (Weil-10230),
   ``008_Weil1500hex210prns.txt`` (Weil-1500)
2. Convert hex |rarr| bits |rarr| MSB-first packed bytes (8 chips per byte)
3. Emit C++ with ``alignas(64)``, ``constinit``,
   ``LUNALINK_LUT_SECTION`` (``.lunalink_lut`` on ELF, ``__TEXT,lunalink_lut`` on Mach-O)

.. |rarr| unicode:: U+2192

Generator validation (``test_prn_generators.cpp``) cross-checks all 210
Gold PRNs produced by ``GoldGenerator`` against the packed LUT --- any
bit-for-bit mismatch fails the build.

Packed storage
~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 20 20 35

   * - Table
     - Bytes / PRN
     - Total (210 PRNs)
     - Storage
   * - Gold-2046
     - 256
     - 52.5 KB
     - ``.lunalink_lut`` section, ``constinit``
   * - Weil-10230
     - 1279
     - 262.3 KB
     - ``.lunalink_lut`` section, ``constinit``
   * - Weil-1500
     - 188
     - 38.6 KB
     - ``.lunalink_lut`` section, ``constinit``

API
~~~

.. code-block:: cpp

   // Load packed PRN code (pointer to static LUT, zero-copy)
   [[nodiscard]] PrnStatus gold_prn_packed(PrnId prn_id, PrnCode& out) noexcept;
   [[nodiscard]] PrnStatus weil10230_prn_packed(PrnId prn_id, PrnCode& out) noexcept;
   [[nodiscard]] PrnStatus weil1500_prn_packed(PrnId prn_id, PrnCode& out) noexcept;

   // Extract one chip from packed sequence (MSB-first)
   [[nodiscard]] inline PrnStatus unpack_chip(
       const PrnCode& code, uint16_t chip_idx, uint8_t& out_chip) noexcept;

   // Codebook integrity checksums (CBIT)
   [[nodiscard]] uint64_t weil10230_codebook_checksum() noexcept;
   [[nodiscard]] uint64_t weil1500_codebook_checksum() noexcept;

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - TMR-hardened PRN ID
     - ``PrnId`` = ``TmrValue<CheckedRange<uint8_t, 1, 210>>``; triple-replicated
       storage with 2-of-3 majority vote and active repair on every read
   * - SEU-resistant status codes
     - ``PrnStatus`` values use non-contiguous bit patterns with Hamming distance |geq| 4
       (``kOk`` = 0x5A, ``kFaultDetected`` = 0x99, etc.)
   * - Codebook integrity
     - CRC-32 over all packed bytes; golden values in ``codebook_checksums.hpp``;
       verified at POST step 2 and on demand via ``weil*_codebook_checksum()``
   * - Watchdog integration
     - ``wip_tick()`` called per byte during checksum computation --- proves progress
       to the windowed watchdog HAL
   * - Section pinning
     - All LUTs placed in ``LUNALINK_LUT_SECTION`` with ``alignas(64)`` for
       physical memory placement control and integrity monitoring
   * - Secure scrub
     - Output buffer zeroed via ``secure_scrub`` on any error path (prevents stale data leak)

.. |geq| unicode:: U+2265

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/signal/test_prn.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - Gold PRN 1 has binary chip values
     - All 2046 chips |isin| {0, 1}
   * - Gold PRN returns distinct sequences
     - PRN 1 |ne| PRN 2 (reject isomorphic sequences)
   * - Gold PRN 1 first byte matches reference
     - Packed byte = 0x5D (01011101\ :sub:`b`), matching Annex 3
   * - All 210 Gold PRNs have binary chip values
     - Exhaustive correctness check
   * - Weil-10230 PRN 1 first chips match reference
     - First 14 chips = ``00010111110101``
   * - Weil-1500 PRN 1 first byte matches reference
     - Packed byte = 0xE4
   * - PRN APIs: saturating logic (C-Pattern 4)
     - ``CheckedRange`` bounds enforcement for out-of-range IDs
   * - Weil codebook checksums (CBIT integrity)
     - Golden CRC-32 matches runtime computation; stable across calls
   * - PrnId: TMR resilience
     - Single-copy SEU corrected by ``vote()``; ``repair()`` restores clean state

.. |isin| unicode:: U+2208
.. |ne| unicode:: U+2260

Generator validation (file: ``cpp/tests/signal/test_prn_generators.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - Gold-2046: All 210 PRNs
     - Generator output matches packed LUT bit-for-bit
   * - Weil-10230: valid binary output
     - PRN 1--10 produce chips |isin| {0, 1}
   * - Weil-1500: valid binary output
     - PRN 1--10 produce chips |isin| {0, 1}
   * - Generator error paths
     - Invalid PRN, undersized output |rarr| correct error status
   * - Weil zero insertion_index (PRN 38)
     - Regression: ``kFaultDetected`` returned (guards silent skip)

----

GD1.2 --- Tiered Code Combiner (Matched Code)
----------------------------------------------

Spec reference: LSIS-AFS-401 (AFS-Q Pilot Code Construction)

The matched code combines three code tiers into a single 10230-chip epoch:

.. code-block:: text

   matched_chip[i] = primary[i] XOR secondary[epoch % 4] XOR tertiary[(offset + epoch/4) % 1500]

Implementation files:

-  ``cpp/include/lunalink/signal/matched_code.hpp`` --- public API
-  ``cpp/signal/matched_code.cpp`` --- epoch generation

Secondary codes
~~~~~~~~~~~~~~~

Four 4-bit secondary codes per LSIS Table 10:

.. list-table::
   :header-rows: 1
   :widths: 15 40 45

   * - Code
     - Pattern
     - Binary
   * - S0
     - {1, 1, 1, 0}
     - 1110
   * - S1
     - {0, 1, 1, 1}
     - 0111
   * - S2
     - {1, 0, 1, 1}
     - 1011
   * - S3
     - {1, 1, 0, 1}
     - 1101

Matched code assignment
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   struct MatchedCodeAssignment {
     PrnId primary_prn;                                       // [1, 210]
     CheckedRange<uint8_t, 0, 3> secondary_code_idx;          // S0--S3
     PrnId tertiary_prn;                                      // [1, 210]
     CheckedRange<uint16_t, 0, 1499> tertiary_phase_offset;   // chip offset
   };

Interim mapping (LSIS Table 11, PRN 1--12):

-  Primary PRN = input PRN
-  Secondary index = ``(PRN - 1) % 4``
-  Tertiary PRN = input PRN (same as primary)
-  Tertiary phase offset = 0

API
~~~

.. code-block:: cpp

   // Generate one 10230-chip epoch using explicit assignment
   [[nodiscard]] MatchedCodeStatus matched_code_epoch_checked(
       const MatchedCodeAssignment& assignment,
       uint16_t epoch_idx,
       std::span<uint8_t> out) noexcept;

   // Convenience: default interim mapping for PRN 1--12
   [[nodiscard]] inline MatchedCodeStatus matched_code_epoch(
       PrnId prn_id, uint16_t epoch_idx, std::span<uint8_t> out) noexcept;

Algorithm (byte-optimised):

1. Pre-clear output with ``secure_scrub``
2. Validate inputs (output size |geq| 10230, epoch < 6000, assignment valid)
3. Load primary (Weil-10230) and tertiary (Weil-1500) packed codes
4. Compute ``modifier = secondary_chip XOR tertiary_chip``
5. XOR entire primary code with byte-wide modifier (1278 full bytes + 6 trailing chips)
6. CFI check: verify loop counter matches expected iteration count

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - Control Flow Integrity
     - Byte-loop iteration counter verified post-loop; ``kFaultDetected`` on mismatch
   * - CheckedRange parameters
     - Secondary index [0,3], tertiary phase [0,1499], epoch [0,5999] all saturating
   * - Secure scrub
     - Output zeroed on all error paths and pre-cleared before computation
   * - SEU-resistant status codes
     - ``MatchedCodeStatus`` uses non-contiguous bit patterns (same scheme as ``PrnStatus``)

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/signal/test_matched_code.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - secondary_code_index cycles through S0--S3
     - PRN 1 |rarr| S0, PRN 2 |rarr| S1, ..., PRN 5 |rarr| S0
   * - secondary codes match spec Table 10
     - Literal bit verification of S0={1,1,1,0}, S1={0,1,1,1}, S2={1,0,1,1}, S3={1,1,0,1}
   * - matched_code_epoch produces binary chip values
     - All 10230 output chips |isin| {0, 1}
   * - matched_code_epoch = primary XOR secondary XOR tertiary
     - Combiner correctness verified chip-by-chip
   * - matched_code_epoch differs across secondary boundary
     - Epoch 0 vs epoch 3 (secondary code changes at epoch % 4)
   * - matched_code_epoch changes across tertiary boundary
     - Epoch 0 vs epoch 4 (tertiary index advances at epoch / 4)
   * - different PRNs produce different sequences
     - Uniqueness across PRN IDs
   * - kEpochsPerFrame is 6000
     - Constant matches spec (12 s / 2 ms = 6000)
   * - default interim mapping only for PRN 1--12
     - PRN 13 returns ``kInvalidAssignment``
   * - matched_code_epoch_checked rejects invalid inputs
     - Epoch |geq| 6000 and undersized output |rarr| correct error status
   * - matched_code_epoch_checked supports tertiary phase offset
     - Non-zero offset produces different output
   * - checked matched helper APIs: saturating logic
     - TMR fault injection bypassed; CheckedRange saturates on out-of-bound read

SWIFI fault injection (file: ``cpp/tests/signal/test_fault_injection.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Scenario
     - What it verifies
   * - tmr_single_copy_corrupt
     - Single SEU in one TMR copy corrected by ``vote()``
   * - checked_range_force_beyond_max
     - Saturation enforced on next access after fault
   * - cfi_guard_inject_skip
     - Skipped loop iteration detected in matched code combiner

----

Cross-Cutting: Coherency & Round-Trip Tests
--------------------------------------------

Coherency test (``cpp/tests/signal/test_coherency.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Verifies primary/secondary/tertiary phase alignment (LSIS-172) over a full
6000-epoch frame.  Confirms that epoch boundaries align correctly and that
the tertiary code advances once every four epochs.

Round-trip test (``cpp/tests/signal/test_roundtrip.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The full TX |rarr| RX round-trip encodes a matched-code epoch through the
complete signal chain (spreading |rarr| modulation |rarr| IQ multiplexing)
and verifies reconstruction.

----

Python Bindings
---------------

All GD1 components are exposed to Python via ``lunalink.afs``.

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Function
     - Signature
     - Notes
   * - ``prn_code``
     - ``(prn_id: int) -> NDArray[uint8]``
     - Gold-2046 (2046 chips)
   * - ``weil10230_code``
     - ``(prn_id: int) -> NDArray[uint8]``
     - Weil-10230 (10230 chips)
   * - ``weil1500_code``
     - ``(prn_id: int) -> NDArray[uint8]``
     - Weil-1500 (1500 chips)
   * - ``matched_code_epoch``
     - ``(prn_id: int, epoch_idx: int) -> NDArray[uint8]``
     - Default interim mapping (PRN 1--12)
   * - ``matched_code_epoch_assigned``
     - ``(p_prn, s_idx, t_prn, t_phase, epoch) -> NDArray[uint8]``
     - Explicit assignment (any PRN)

Python tests: ``task test-py`` (file: ``tests/test_afs.py``)

----

Interoperability Test Vectors
-----------------------------

Pre-generated hex vectors for PRN 1--12 are committed under
``test-vectors/codes/``:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - File
     - Content
   * - ``prn001.hex`` -- ``prn012.hex``
     - INI-format with sections: ``[GOLD_CODE]``, ``[WEIL_PRIMARY]``,
       ``[WEIL_TERTIARY]``, ``[SECONDARY_S0]``--``[SECONDARY_S3]``

Each file contains the full hex representation of Gold-2046, Weil-10230,
and Weil-1500 for that PRN ID, plus the four secondary code patterns.
These vectors can be compared byte-for-byte with any independent
implementation using the same Annex 3 reference data.

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 20 25

   * - Suite
     - Tests
     - Assertions
     - Run time
     - How to run
   * - GD1 unit tests
     - 21
     - ~350
     - < 1 s
     - ``task test-cpp``
   * - Generator validation
     - 5
     - 210+ (all PRNs)
     - < 1 s
     - ``task test-cpp``
   * - SWIFI (GD1 scenarios)
     - 3
     - ~30
     - < 1 s
     - ``task swifi``
   * - Python bindings
     - included in pytest
     - ---
     - < 5 s
     - ``task test-py``

All C++ unit tests pass in under 2 seconds with exit code 0.
SWIFI scenarios verify 100% single-bit fault detection for all GD1 hardening patterns.

Related documents:

-  :doc:`signal/compliance_matrix` --- LSIS-110, LSIS-120, LSIS-130, LSIS-140, LSIS-150
-  :doc:`signal/architecture` --- module-to-gateway mapping
-  :doc:`signal/signal_chain` --- transmitter block diagram
-  :doc:`signal/validation_plan` --- five-level test hierarchy
-  :doc:`assurance/public/mission_assurance` --- TMR, CFI, CheckedRange patterns
-  :doc:`assurance/private/fmea` --- PRN generator failure modes
-  :doc:`assurance/private/fault_coverage` --- SWIFI scenario catalogue
