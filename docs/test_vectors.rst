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

Workflow
--------

::

    task vectors          # regenerate all vectors from current code
    task vectors-check    # validate headers and checksums
    task vectors-release  # tag + create GitHub release
