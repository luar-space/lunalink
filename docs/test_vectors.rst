Test Vector Format
==================

This page documents the LunaLink test vector format — a layered, spec-traceable
format designed for cross-team interoperability testing of AFS codec
implementations.

Purpose
-------

Test vectors enable any team to validate their AFS receiver pipeline against
our encoder output (and vice versa) without installing our library. Each vector
pairs ground truth message fields with intermediate and final encoded outputs,
so teams can isolate exactly where a divergence occurs.

Signal Chain and Vector Layers
------------------------------

The AFS transmit pipeline has multiple stages. Test vectors capture five layers
(L0–L4), stopping before modulation:

.. code-block:: text

    Message fields (JSON)              ← L0: ground truth
        ↓ serialize
    Information bits                   ← L1: packed bit fields (hex in JSON)
        ↓ CRC-24 append
    Protected bits                     ← L2: checksum (hex in JSON)
        ↓ LDPC encode + interleave
    Encoded symbols (0/1)              ← L3: per-subframe binary files
        ↓ BCH(51,8) + sync prepend
    Complete frame symbols (0/1)       ← L4: full frame binary file
        ↓ chip mapping + spreading + BPSK modulation
    Baseband IQ (complex float32)      ← NOT shared

Everything below L4 (chip mapping, spreading, BPSK modulation) is deterministic
and trivial. A team that matches L4 will produce identical RF.

.. list-table:: Layer Summary
   :header-rows: 1
   :widths: 10 30 20 20

   * - Layer
     - Content
     - Location
     - Typical Size
   * - L0
     - Message field values with units, bit widths, scale factors, spec refs
     - ``vector.json``
     - ~2 KB
   * - L1
     - Serialized information bits (before FEC)
     - ``vector.json`` (hex string)
     - ~330 hex chars
   * - L2
     - CRC-24 value
     - ``vector.json`` (6 hex chars)
     - 6 chars
   * - L3
     - LDPC-encoded + interleaved symbols (SB2+SB3+SB4 concatenated)
     - ``subframes.bin`` (1 byte/symbol)
     - ~17 KB
   * - L4
     - Complete frame: sync + SB1 + SB2 + SB3 + SB4
     - ``frame.bin`` (1 byte/symbol)
     - ~17 KB

Binary files use one byte per symbol (``0x00`` or ``0x01``) to avoid
endianness and bit-packing ambiguity.

File Layout
-----------

.. code-block:: text

    vectors/
    ├── manifest.json
    ├── validate.py
    ├── G01_ephemeris/
    │   ├── vector.json
    │   ├── subframes.bin
    │   └── frame.bin
    ├── G02_almanac/
    │   ├── vector.json
    │   ├── subframes.bin
    │   └── frame.bin
    ├── ...
    └── G30_utc/
        ├── vector.json
        ├── subframes.bin
        └── frame.bin

One directory per message type (22 total). The 8 spec-defined types (G1, G2,
G4, G5, G8, G22, G24, G30) carry ``"spec_status": "fully_defined"``; the 14
TBW types carry ``"spec_status": "sisicd_defined"``.

Manifest
--------

``manifest.json`` contains format version, spec reference, generation metadata,
and the list of included vector directories:

.. code-block:: json

    {
      "format_version": "1.0.0",
      "spec_reference": "LSIS V1.0",
      "generated_by": "lunalink",
      "generated_by_version": "0.2.0",
      "generated_by_commit": "abc1234",
      "generated_at": "2026-03-15T12:00:00Z",
      "vector_count": 22,
      "vectors": ["G01_ephemeris", "G02_almanac", "..."]
    }

Vector JSON Schema
------------------

Each ``vector.json`` contains all non-binary layers and SHA-256 references to
binary files:

.. code-block:: json

    {
      "format_version": "1.0.0",
      "spec_reference": "LSIS V1.0",
      "message_type": "G04",
      "message_name": "Clock & Ephemeris",
      "spec_section": "§2.5.4",
      "spec_status": "fully_defined",
      "prn": 1,
      "subframe": 2,
      "frame_id": 0,
      "toi": 42,
      "L0_fields": {
        "t_oc": {
          "value": 3600, "unit": "s", "bits": 16,
          "scale": "2^4", "spec_table": "Table 15"
        }
      },
      "L1_info_bits_hex": "A3F7...0000",
      "L1_info_bits_length": 1320,
      "L1_filler_bits": 0,
      "L2_crc24_hex": "B7A3F1",
      "L3_subframes_sha256": "e3b0c44298fc...",
      "L3_subframes_sizes": [5880, 5880, 5880],
      "L4_frame_sha256": "a1b2c3d4e5f6...",
      "L4_frame_symbols": 17760
    }

Validation
----------

``validate.py`` is a standalone Python 3 script with **zero external
dependencies**. Any team can run it without installing our library::

    python3 validate.py vectors/

It performs:

- SHA-256 checksum verification of all ``.bin`` files
- Binary file size validation against declared symbol counts
- JSON schema validation (all required fields present)
- Per-vector PASS/FAIL output; exits non-zero on any failure

Distribution via GitHub Releases
---------------------------------

Test vectors are versioned independently from the library:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Aspect
     - Detail
   * - Tag format
     - ``vectors-v1.0.0``
   * - Release asset
     - ``lunalink-test-vectors-v1.0.0.tar.gz``
   * - First release (V2)
     - 8 spec-defined message types
   * - Second release (V3)
     - All 22 message types
   * - Release notes
     - Changelog, spec version, included types, known limitations

Separate versioning allows correcting vectors without implying a library
change.

Integration Tests
-----------------

The same vectors drive four CI test suites:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Test
     - What it proves
   * - Golden encode
     - ``encode(L0_fields)`` produces output identical to ``frame.bin`` —
       regression guard
   * - Golden decode
     - ``decode(frame.bin)`` recovers ``L0_fields`` bit-perfect — full RX
       pipeline proof
   * - Noisy decode
     - ``decode(frame.bin + AWGN)`` at operational Eb/N₀ — LDPC decoder
       robustness
   * - Cross-layer
     - Encoder intermediate outputs match L1, L2, L3 — catches silent
       internal changes

Size Budget
-----------

.. list-table::
   :header-rows: 1
   :widths: 30 25 25

   * - Item
     - Per vector
     - All 22 vectors
   * - ``vector.json``
     - ~2 KB
     - ~44 KB
   * - ``subframes.bin``
     - ~17 KB
     - ~374 KB
   * - ``frame.bin``
     - ~17 KB
     - ~391 KB
   * - **Total**
     - ~36 KB
     - **~809 KB**

Under 1 MB total. No compression or git-lfs needed.

Noisy IQ vectors for BER testing (V4) are large (~490 MB each) and distributed
only as GitHub Release assets, never committed to the repository.

Workflow
--------

::

    task vectors          # regenerate all vectors from current code
    task vectors-check    # validate checksums (CI runs this)
    task vectors-release  # tag + create GitHub release
