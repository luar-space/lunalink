Test Vector Format & Interoperability
=====================================

This page documents the LunaLink test vector format, aligned with the official 
**LSIS-AFS Interoperability Testing** protocol (Level 1–5).

Purpose
-------

Test vectors enable any team to validate their AFS receiver pipeline against 
our encoder output (and vice versa) without installing our library. These 
vectors pair ground truth message fields with intermediate and final encoded 
outputs, including mandatory competition headers.

Interoperability Levels
-----------------------

Following the official competition structure, validation is divided into five 
increasing levels of complexity:

*   **Level 1: Code Generation**: Verification of identical spreading codes 
    (Gold, Weil, Legendre).
*   **Level 2: Encoding**: Verification of bit-identical encoded frames 
    (Sync, BCH, LDPC, Interleaving).
*   **Level 3: Signal Generation**: Verification of compatible I/Q signals 
    (Sample rates, BPSK mapping).
*   **Level 4: Decoding**: Cross-decoding implementation A's signal with 
    implementation B's decoder.
*   **Level 5: Message Parsing**: Field-by-field JSON comparison of extracted 
    navigation data.

Standardized Test Messages (Level 2)
------------------------------------

Milestone 2 validation requires processing the five official baseline messages:

.. list-table:: Mandatory Test Scenarios
   :header-rows: 1
   :widths: 15 25 60

   * - Scenario
     - Name
     - Purpose
   * - MSG-1
     - All Zeros
     - Baseline check; verifies padding and CRC handling.
   * - MSG-2
     - All Ones
     - Inverse baseline; verifies scrambler/mapping logic.
   * - MSG-3
     - Alternating
     - Pattern ``0xAA...``; verifies interleaver/deinterleaver matrix.
   * - MSG-4
     - Ephemeris
     - Known CED parameters; verifies field bit-packing.
   * - MSG-5
     - Random
     - Known seed; stress tests LDPC decoder convergence.

Mandatory File Formats
----------------------

Test vectors are exported in the following official competition formats:

1. Spreading Codes (``codes_prn{N}.hex``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ASCII text using sectional headers:
*   ``[GOLD_CODE]``
*   ``[WEIL_PRIMARY]``
*   ``[WEIL_TERTIARY]``
*   ``[SECONDARY_S0]`` ... ``[SECONDARY_S3]``

2. Encoded Frames (``frame_{id}.bin``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Binary symbols with a **64-byte metadata header**:
*   **Magic**: ``"LSISAFS\0"`` (8 bytes)
*   **Version**: ``1`` (4 bytes, uint32)
*   **Frame Length**: ``6000`` (4 bytes, uint32)
*   **PRN**: ``{N}`` (4 bytes, uint32)
*   **Timestamp**: Unix epoch (8 bytes, uint64)
*   **Reserved**: 36 bytes (zero-filled)
*   **Data**: 6000 bytes (1 byte/symbol: ``0x00`` or ``0x01``).

3. Baseband Signal (``signal_{prn}_{dur}s.iq``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Binary I/Q samples with a **128-byte metadata header**:
*   **Magic**: ``"LSISIQ\0\0"`` (8 bytes)
*   **Version**: ``1`` (4 bytes, uint32)
*   **Sample Rate**: float64 (8 bytes)
*   **Duration**: float64 seconds (8 bytes)
*   **PRN**: uint32 (4 bytes)
*   **Sample Format**: ``"float32"`` (16 bytes)
*   **Reserved**: 80 bytes (zero-filled)
*   **Data**: Interleaved float32 pairs ``[I0, Q0, I1, Q1, ...]``.

4. Parsed Data (``parsed_{id}.json``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
JSON field extraction using the official schema:

.. code-block:: json

    {
      "version": "1.0",
      "timestamp": "2026-03-07T12:00:00Z",
      "frame_id": "test_001",
      "subframe1": { "fid": 0, "toi": 42 },
      "subframe2": { "wn": 1234, "itow": 256, "ced": { "af0": 1.23e-9 } },
      "time_of_transmission": 1234567890.123
    }

What ships where
----------------

This repository commits a compact vector set under ``test-vectors/``:

*   ``codes/`` — 210 × ``codes_prn{NNN}.hex`` (Level 1, sectioned hex format)
*   ``frames/`` — encoded 6000-symbol frames with the ``LSISAFS`` header
*   ``signals/`` — I/Q vectors with the ``LSISIQ`` header: PRN 1 at full
    12-second duration, plus short (2 ms) vectors for PRN 1–12 at both
    sample rates, gzipped to keep the clone light
*   ``parsed/`` — parsed-data JSONs
*   ``fec/`` and ``decode/`` — component-level FEC and decode-chain vectors
    beyond the competition's mandatory formats

The full-size shared distribution — including 12-second signals for the
Table 11 PRNs, bundled oracles (Annex 3, LANS-AFS-SIM dumps,
PocketSDR-AFS), and a zero-dependency validator — is published at
`luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__.

A 12-second signal for **any** PRN regenerates in under a second:

::

    uv run lunalink encode --format iq32 --prn 7 --rate 10230000 --out signal_prn007_12s.iq

Parsed-data caveat (LSIS V1.0 TBW fields)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Only fields that LSIS V1.0 actually pins (FID/TOI per Tables 13/14,
WN/ITOW per Table 22, ToT per §2.5.5, CRC-24Q per §2.4.3.1.3) are emitted
as concrete values in ``parsed_*.json``.  CED, Health, Time Conversions,
and the SB3/SB4 data layouts are still *to be written* in V1.0; those are
surfaced as raw bit-slices with explicit ``_lsis_ref`` markers so
consumers can byte-equal-compare without inventing bit positions the spec
does not define.

Regeneration
------------

::

    # everything, into a checkout of the shared vectors repository
    uv run python scripts/export_test_vectors_all.py --public-repo <path>

    # individual levels / local subsets
    uv run python scripts/export_test_vectors_l1.py test-vectors/codes
    uv run python scripts/export_test_vectors_fec.py --output-dir test-vectors/fec
