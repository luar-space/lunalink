Check your implementation against LunaLink
==========================================

If you have an LSIS-AFS code generator, encoder, signal generator or receiver,
you can compare it with LunaLink at every stage of the chain.  There are three
routes:

1. `Compare against the shared vectors`_.  This needs Python and a clone of the
   vector repository.  You do not install LunaLink.
2. `Run the cross-checks from LunaLink`_.  This builds two independent
   implementations on your machine and runs LunaLink against them, or runs
   LunaLink over a directory of your own vectors.
3. `Report what you find`_, whether it agrees or not.

A disagreement is useful whichever side turns out to be wrong.  LSIS V1.0
leaves several fields open, and :doc:`signal/spec_findings_report` records the
choice LunaLink made at each one.  A mismatch at one of those points shows
where two readings of the standard differ.

The five levels
---------------

The comparison is split into five levels, one per stage.  The shared
repository, the interoperability tasks and :doc:`interop_results` all use the
same numbering.

.. list-table::
   :header-rows: 1
   :widths: 8 42 50

   * - Level
     - What is compared
     - Compared against
   * - 1
     - Spreading codes: Gold-2046, Weil-10230, Weil-1500 and the secondary
       codes, for PRN 1 to 210
     - Annex 3 of the standard, and chip dumps from LANS-AFS-SIM
   * - 2
     - Encoded frames: BCH(51,8), CRC-24Q, LDPC and the 60×98 interleaver,
       6000 symbols per frame
     - The frame structure in LSIS V1.0 §2.4, and frame dumps from
       LANS-AFS-SIM
   * - 3
     - Baseband I/Q signals, 12 seconds each
     - The file and sample structure, and the polarity of the first chip
       checked against the level 1 codes and the level 2 frames
   * - 4
     - Cross-decoding: a receiver recovers the data that was transmitted
     - The bits recovered by PocketSDR-AFS, an independent receiver
   * - 5
     - Parsed navigation data
     - The fields LSIS V1.0 defines: FID, TOI, WN, ITOW and Time of
       Transmission

`LANS-AFS-SIM <https://github.com/osqzss/LANS-AFS-SIM>`__ is an independent
transmit-side simulator written in C.  `PocketSDR-AFS
<https://github.com/osqzss/PocketSDR-AFS>`__ is an independent
software-defined receiver.  Neither shares code with LunaLink.

Compare against the shared vectors
----------------------------------

The vectors for all five levels are published at
`luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__, with the reference
data they were checked against and a validator, ``validate.py``.  The
validator is one file that uses only the Python standard library, and it runs
on CPython 3.10 or later.

.. code-block:: bash

   git clone https://github.com/luar-space/lsis-afs-test-vectors
   cd lsis-afs-test-vectors
   python validate.py verify-manifest   # the files are the ones published
   python validate.py check-annex3      # the codes match Annex 3

The ``signals/`` directory is by far the largest part of the repository.  If
you only need levels 1 and 2, a sparse checkout leaves it out:

.. code-block:: bash

   git clone --filter=blob:none https://github.com/luar-space/lsis-afs-test-vectors
   cd lsis-afs-test-vectors
   git sparse-checkout init --cone
   git sparse-checkout set codes frames inputs references

Files in the repository root, ``validate.py`` and ``manifest.json`` among
them, are always checked out.  On a sparse checkout ``verify-manifest``
reports the files you left out as missing; ``check-annex3`` is unaffected.

To compare your own output, write it in the formats described in
:doc:`test_vectors` and point the matching ``diff`` command at the directory:

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Level
     - Your output
     - Command
   * - 1
     - Spreading code files
     - ``python validate.py diff /path/to/your/codes/``
   * - 2
     - Encoded frames
     - ``python validate.py diff-frames /path/to/your/frames/``
   * - 2
     - The subframe bits you fed your encoder
     - ``python validate.py diff-inputs /path/to/your/inputs/``
   * - 3
     - I/Q signal files
     - ``python validate.py diff-signals /path/to/your/signals/``
   * - 4
     - The bits your receiver recovered
     - ``python validate.py diff-decode /path/to/your/decoded/``
   * - 5
     - Parsed navigation JSON
     - ``python validate.py diff-parsed /path/to/your/parsed/``

Three of these need a word of explanation.

To test an encoder, read the SB2, SB3 and SB4 bits from
``inputs/frame_*_input.bin``, encode them, and run ``diff-frames`` on the
result.  Both sides then start from the same bits, so a mismatch is in the
BCH, CRC, LDPC or interleaver stage.

To test a receiver, decode the files in ``signals/`` and run ``diff-decode``.
It compares what you recovered with the bits that were originally encoded.  It
does not compare your decoder with LunaLink's.

``diff-parsed`` compares only the fields LSIS V1.0 defines.  Where the
standard leaves a payload to be written, the shared JSON files carry the raw
bits and name the open item, so use ``diff-inputs`` to compare those bits.

The repository's own README lists every file, the ``check-*`` commands that
verify each level against its reference data, and the licence of each bundled
reference.

Run the cross-checks from LunaLink
----------------------------------

With LunaLink installed (:doc:`quickstart`), two tasks reproduce the results
published in :doc:`interop_results`:

.. code-block:: bash

   task interop            # levels 1 to 3, against LANS-AFS-SIM
   task interop-receiver   # levels 4 and 5, against PocketSDR-AFS

``task interop`` clones and builds LANS-AFS-SIM on its first run.  It compares
the spreading codes and the encoded frames of the two implementations bit for
bit, then acquires, tracks and decodes a signal that LANS-AFS-SIM generated.

``task interop-receiver`` builds PocketSDR-AFS, has it decode a signal that
LunaLink generated, and compares the payload it recovers with the one
transmitted.  It needs ``libfftw3`` and ``libusb``.

Neither project is vendored into this repository.  The tasks fetch them.

To run LunaLink over your own vectors, lay them out in any of the directories
``codes/``, ``frames/``, ``signals/``, ``parsed/`` and ``decode/``, and run:

.. code-block:: bash

   uv run python scripts/interop/validate_foreign_vectors.py /path/to/your/vectors

The script checks the format and size of every file it finds, and accepts
gzip-compressed files.  For each encoded frame it also checks the
synchronisation pattern and runs LunaLink's decoder, then prints the FID and
TOI it recovered and which subframes passed their CRC.  It exits with status 0
when every check passes.

Report what you find
--------------------

Open an `interoperability report
<https://github.com/luar-space/lunalink/issues/new?template=interop_report.yml>`__
for any cross-check, passing or failing.  The form asks which implementation
you tested, in which direction, and the deepest level you reached.  Published
results are collected in :doc:`interop_results`.

If the disagreement comes from reading the standard differently, open a
`specification finding
<https://github.com/luar-space/lunalink/issues/new?template=spec_finding.yml>`__
instead.  The open items LunaLink has already recorded are in
:doc:`signal/spec_findings_report`.

Vectors that disagree with the shared set, or that cover a case it lacks,
belong in the `vector repository
<https://github.com/luar-space/lsis-afs-test-vectors>`__ itself.
