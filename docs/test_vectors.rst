Test Vector Format & Interoperability
=====================================

The LunaLink test vector format.  It follows the interoperability plan written for the ESA-CCSDS competition, which the
`shared vector repository <https://github.com/luar-space/lsis-afs-test-vectors>`__
bundles as ``references/interoperability.pdf``.
That plan is not part of LSIS V1.0.  To compare your own implementation
against these vectors, see :doc:`interoperate`.

Purpose
-------

Any team can check its implementation against the shared vectors, the same
ones LunaLink is checked against, without installing this library.  Each
vector pairs
ground-truth message fields with the intermediate and final encoded outputs,
file headers included.

Interoperability Levels
-----------------------

The plan splits validation into five levels:

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

The plan defines five baseline messages:

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
     - Pattern ``0101...`` (``0x55`` bytes, MSB-first); verifies
       interleaver/deinterleaver matrix.
   * - MSG-4
     - Ephemeris
     - Known CED parameters; verifies field bit-packing.
   * - MSG-5
     - Random
     - Known seed; stress tests LDPC decoder convergence.

Mandatory File Formats
----------------------

Exports take the four formats the plan defines:

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
*   **Sample Format**: ``"float32"`` or ``"int16"`` (16 bytes, zero-padded
    ASCII)
*   **Reserved**: 80 bytes (zero-filled)
*   **Data**: Interleaved pairs ``[I0, Q0, I1, Q1, ...]`` in the declared
    format, little-endian.

Both TC-2 sample formats work.  ``float32`` carries
normalised samples in [-1.0, +1.0] and is the default.  ``int16`` stores the
same samples at full scale (±32767; the value ``-32768`` is never emitted, so
negation is exact), which halves file size and matches the convention used by
SDR capture files and by LANS-AFS-SIM.  Quantisation to int16 costs no
measurable decode performance; see the quantisation sweep in
:doc:`signal/ldpc_performance`.

Take the format from the header; never infer it from the payload:

.. code-block:: python

   from lunalink.afs import read_iq_file, write_iq_file

   write_iq_file("signal.iq", iq, 5.115e6, prn_id=1, sample_format="int16")

   sig = read_iq_file("signal.iq")
   sig.sample_format   # 'int16'
   sig.iq              # float32 (N, 2), rescaled to [-1.0, +1.0]
   sig.sample_rate, sig.duration_s, sig.prn_id

4. Parsed Data (``parsed_{id}.json``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
JSON field extraction using the plan's schema:

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

The published vectors are in the shared repository,
`luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__.  They are the
reference: they ship with the oracles each level was checked against
(Annex 3, LANS-AFS-SIM dumps, PocketSDR-AFS) and a zero-dependency
validator, and LunaLink is held to them like any other implementation.
``task conformance`` fetches a pinned release, has LunaLink produce every
level, and runs the repository's ``diff-*`` commands on the result; see
:doc:`interoperate`.

This repository also commits fixtures under ``test-vectors/`` so that
``task test-py`` can exercise the formats without a network connection:

*   ``codes/`` — ``codes_prn{NNN}.hex`` in the sectioned hex format
*   ``frames/`` — encoded 6000-symbol frames with the ``LSISAFS`` header
*   ``signals/`` — I/Q vectors with the ``LSISIQ`` header: PRN 1 at full
    12-second duration, plus short (2 ms) vectors for PRN 1–12 at both
    sample rates, gzipped to keep the clone light
*   ``parsed/`` — parsed-data JSONs
*   ``fec/`` — BCH and CRC-24Q vectors
*   ``decode/`` — expected decoder outputs

They are not the published set: the file names and test payloads differ.
To compare an implementation, use the shared repository.

A 12-second ``LSISIQ`` signal for **any** PRN regenerates from the Python
API (:func:`~lunalink.afs.write_iq_file` writes exactly the header defined
under "Mandatory File Formats" above):

.. code-block:: python

    from lunalink.afs import make_test_payload, transmit_frame, write_iq_file

    sb2, sb3, sb4 = make_test_payload("random", seed=0x12345678)
    frame, iq = transmit_frame(prn_id=7, fid=0, toi=1,
                               sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
                               sample_rate=10.23e6)
    write_iq_file("signal_prn007_12s.iq", iq, sample_rate=10.23e6, prn_id=7)

The CLI offers a one-line alternative.  It writes the *headerless* float32
shape used at live interop workshops, not the ``LSISIQ``-headed format::

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

    # every level, into a checkout of the shared vector repository
    uv run python scripts/export_test_vectors_all.py <path-to-lsis-afs-test-vectors>

    # LunaLink's offline fixtures under test-vectors/
    uv run python tests/generate_test_vectors.py

A new release of the shared vectors has to pass the repository's own
``check-*`` commands.  LunaLink then moves the pin in
``scripts/interop/conformance.py`` and has to pass ``task conformance``
against it.
