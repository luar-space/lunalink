Quickstart
==========

The LunaLink Python API, from generating your first AFS signal to decoding
navigation data and producing interoperability test vectors.

Installation
------------

.. code-block:: bash

   git clone https://github.com/luar-space/lsis-afs && cd lsis-afs
   task install   # uv sync --group dev — builds the C++ core + Python package

   # library-only alternative (no dev tooling):
   pip install .

LunaLink's runtime dependencies are NumPy plus two lightweight typing
libraries (``jaxtyping``, ``beartype``); all are installed automatically
with the package.

Transmitting a Signal
---------------------

Generate a 12-second AFS I/Q signal file in three steps:

.. code-block:: python

   from lunalink.afs import (
       AFS_Q_CHIP_RATE,
       make_test_payload,
       transmit_frame,
       write_iq_file,
   )

   # 1. Create subframe payloads (all zeros for testing)
   sb2, sb3, sb4 = make_test_payload("zeros")

   # 2. Build the frame and modulate onto I/Q carriers
   frame, iq = transmit_frame(
       prn_id=1, fid=0, toi=42,
       sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
   )

   # 3. Write to disk with the LSISIQ metadata header
   write_iq_file("signal.iq", iq, AFS_Q_CHIP_RATE, prn_id=1)

Pass ``sample_format="int16"`` to store the samples at full scale as 16-bit
integers instead — half the file size, the convention used by SDR capture
files, and the format LANS-AFS-SIM emits:

.. code-block:: python

   write_iq_file("signal.iq", iq, AFS_Q_CHIP_RATE, prn_id=1,
                 sample_format="int16")

Read either format back with :func:`~lunalink.afs.read_iq_file`, which takes
the format from the file header and always returns normalised float32:

.. code-block:: python

   from lunalink.afs import read_iq_file

   sig = read_iq_file("signal.iq")
   sig.iq             # float32 (N, 2), in [-1.0, +1.0]
   sig.sample_format  # 'int16' or 'float32'
   sig.sample_rate, sig.duration_s, sig.prn_id

For external analysis tools (spreadsheets, MATLAB/Octave, pandas), the same
samples can be exported as CSV with :func:`~lunalink.afs.write_iq_csv`:

.. code-block:: python

   from lunalink.afs import write_iq_csv

   write_iq_csv("signal.csv", iq)   # header row "sample,i,q", one row per sample

The returned ``frame`` is a :class:`~lunalink.afs.TransmitFrame` with the raw
6000 binary symbols and metadata (``fid``, ``toi``, ``prn_id``).  The ``iq``
array has shape ``(N, 2)`` with interleaved I/Q float32 samples at the native
AFS-Q chip rate of 5.115 MHz.

Test Payloads
~~~~~~~~~~~~~

:func:`~lunalink.afs.make_test_payload` generates standard subframe data for
the competition test scenarios (see :doc:`test_vectors` for the full
MSG-1…MSG-5 definitions):

.. code-block:: python

   sb2, sb3, sb4 = make_test_payload("zeros")        # MSG-1: all zeros
   sb2, sb3, sb4 = make_test_payload("ones")         # MSG-2: all ones
   sb2, sb3, sb4 = make_test_payload("alternating")  # MSG-3: 0101...
   sb2, sb3, sb4 = make_test_payload("random", seed=42)  # MSG-5: Random (PRNG)

(MSG-4, the ephemeris scenario, carries structured CED data and is produced
by the vector export scripts rather than by ``make_test_payload``.)

Each returns a tuple of three NumPy ``uint8`` arrays with the correct lengths
for SB2 (1176 bits), SB3 (846 bits), and SB4 (846 bits).

Custom sample rates
~~~~~~~~~~~~~~~~~~~

By default, I/Q samples are generated at the native AFS-Q chip rate (5.115 MHz).
Pass ``sample_rate`` to resample:

.. code-block:: python

   frame, iq = transmit_frame(
       prn_id=1, fid=0, toi=42,
       sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
       sample_rate=10.23e6,   # 2× oversampled
   )

Decoding a Frame
----------------

Given a stream of soft-decision bipolar symbols, decode in one call:

.. code-block:: python

   from lunalink.afs import receive_frame

   result = receive_frame(bipolar_symbols)

   print(result.fid)              # Frame ID (0-3)
   print(result.toi)              # Time of Interval (0-99)
   print(result.all_ok)           # True if all subframes decoded + CRC passed
   print(result.time.t_f)         # Seconds since LRT epoch
   print(result.sb2.data)         # Decoded SB2 bits (numpy uint8 array)
   print(result.sb3.crc_ok)       # Per-subframe CRC status
   print(result.sb3.ldpc_converged)  # LDPC convergence flag

The input ``bipolar_symbols`` is a ``float32`` array where ``+1.0`` represents
bit 0 and ``-1.0`` represents bit 1.  Must contain at least 6000 symbols.

The :class:`~lunalink.afs.DecodeResult` dataclass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:func:`~lunalink.afs.receive_frame` returns a frozen
:class:`~lunalink.afs.DecodeResult` with these fields:

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Field
     - Type
     - Description
   * - ``fid``
     - ``int``
     - Frame ID / node identity (0-3)
   * - ``toi``
     - ``int``
     - Time of Interval (0-99)
   * - ``sb2``, ``sb3``, ``sb4``
     - :class:`~lunalink.afs.SubframeResult`
     - Per-subframe decode result (see below)
   * - ``time``
     - :class:`~lunalink.afs.TimeOfTransmission` or ``None``
     - Parsed timing from SB2; ``None`` if SB2 decode failed
   * - ``sync_offset``
     - ``int``
     - Symbol index where the sync pattern was found
   * - ``sync_metric``
     - ``float``
     - Sync correlation peak strength
   * - ``all_ok``
     - ``bool`` (property)
     - ``True`` when all three subframes passed CRC

Each :class:`~lunalink.afs.SubframeResult` contains:

- ``data`` — decoded payload bits (``Sb2Payload`` / ``Sb3Payload`` / ``Sb4Payload``)
- ``ldpc_converged`` — whether the LDPC decoder converged
- ``crc_ok`` — whether the CRC-24 check passed

The :class:`~lunalink.afs.TimeOfTransmission` contains:

- ``wn`` — Week Number (0–8191)
- ``itow`` — Interval Time of Week (0–503)
- ``toi`` — Time of Interval (0–99)
- ``t_f`` — seconds since LRT epoch (computed via LSIS-720)
- ``seconds_of_week`` — property: ``itow * 1200 + toi * 12``

Error Handling
~~~~~~~~~~~~~~

By default, :func:`~lunalink.afs.receive_frame` raises on failure:

.. code-block:: python

   from lunalink.afs import SyncNotFoundError, DecodeError

   try:
       result = receive_frame(noisy_symbols)
   except SyncNotFoundError:
       print("No sync pattern detected")
   except DecodeError as e:
       print(f"Decode failed: {e.status.name}")
       # Partial result is still available for diagnostics:
       print(f"SB2 LDPC converged: {e.result.sb2.ldpc_converged}")

For batch processing where you want to handle errors yourself, disable
exceptions:

.. code-block:: python

   result = receive_frame(symbols, raise_on_error=False)
   if not result.all_ok:
       print("Decode incomplete")

LDPC tuning
~~~~~~~~~~~

The LDPC decoder defaults to 50 iterations of layered sum-product belief propagation.
You can adjust the iteration count:

.. code-block:: python

   # Fewer iterations (faster, lower decode margin)
   result = receive_frame(symbols, ldpc_iterations=20)

   # More iterations (slower, better at low SNR)
   result = receive_frame(symbols, ldpc_iterations=100)

End-to-End Round-Trip
---------------------

A complete transmit → decode → verify cycle:

.. code-block:: python

   import numpy as np
   from lunalink.afs import receive_frame, make_test_payload, transmit_frame

   # Transmit
   sb2, sb3, sb4 = make_test_payload("random", seed=42)
   frame, iq = transmit_frame(
       prn_id=1, fid=2, toi=77,
       sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
   )

   # Perfect channel (convert frame symbols to bipolar)
   bipolar = np.where(frame.symbols == 0, 1.0, -1.0).astype(np.float32)

   # Decode
   result = receive_frame(bipolar)

   # Verify
   assert result.all_ok
   assert result.fid == 2
   assert result.toi == 77
   np.testing.assert_array_equal(result.sb2.data, sb2)

Generating Interoperability Test Vectors
----------------------------------------

The :mod:`lunalink.afs.vectors` module produces full-length test vectors in the
competition-standard formats for cross-team validation.

Generate all standard scenarios at once
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   from lunalink.afs.vectors import generate_standard_vectors

   paths = generate_standard_vectors("/tmp/lunalink-vectors", prn_id=1)

   # paths["codes"]   → 1 file:  PRN hex dump (Gold, Weil primary, Weil tertiary)
   # paths["frames"]  → 4 files: 64-byte header + 6000 symbols (MSG-1 to MSG-4)
   # paths["signals"] → 4 files: 128-byte LSISIQ header + float32 I/Q samples
   # paths["parsed"]  → 4 files: JSON with decoded fields + timing

This creates 13 files covering four interop levels:

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - Level
     - Format
     - Content
   * - Level 1
     - ``.hex``
     - Full spreading codes in packed hex (Gold-2046, Weil-10230, Weil-1500)
   * - Level 2
     - ``.bin``
     - Encoded frame with 64-byte ``LSISAFS`` header + 6000 raw symbols
   * - Level 3
     - ``.iq``
     - Baseband I/Q with 128-byte ``LSISIQ`` header + interleaved float32
   * - Level 5
     - ``.json``
     - Parsed navigation data (FID, TOI, WN, ITOW, ToT, per-subframe status)

Individual vector export
~~~~~~~~~~~~~~~~~~~~~~~~

For fine-grained control, use the individual export functions:

.. code-block:: python

   from lunalink.afs.vectors import (
       export_code_vectors,
       export_frame_vector,
       export_parsed_json,
       export_signal_vector,
   )

   # Level 1: Spreading codes
   export_code_vectors(prn_id=1, output_dir="vectors/codes")

   # Level 2: Encoded frame
   sb2, sb3, sb4 = make_test_payload("zeros")
   export_frame_vector(1, 0, 42, sb2, sb3, sb4, "vectors/frames/msg1.bin")

   # Level 3: I/Q signal (uses write_iq_file internally)
   export_signal_vector(1, iq_data, 5.115e6, "vectors/signals/msg1.iq")

   # Level 5: Parsed JSON
   export_parsed_json(result, "vectors/parsed/msg1.json", frame_id="msg1_zeros")

Using the Low-Level API
-----------------------

The high-level functions compose from the low-level API in
:mod:`lunalink.afs.signal`, which remains fully accessible.  Use it when you
need step-by-step control:

.. code-block:: python

   from lunalink.afs import (
       bch_encode,
       frame_build,
       frame_decode,
       ldpc_decode,
       modulate_i,
       multiplex_iq,
       parse_sb2,
       prn_code,
   )

   # Step-by-step TX
   gold = prn_code(1)                          # 2046-chip Gold code
   i_mod = modulate_i(gold, data_symbol=1)     # BPSK modulation
   codeword = bch_encode(fid=0, toi=42)        # BCH(51,8) header

   # Step-by-step RX
   result, sb2, sb3, sb4, status = frame_decode(bipolar)
   parsed = parse_sb2(sb2)
   print(parsed.wn, parsed.itow)

See the :doc:`api` page for the complete reference of all exported functions,
classes, and constants.

API Layers
----------

LunaLink's Python API has three layers.  Each layer calls the one below it;
you can enter at whichever level suits your task:

.. code-block:: text

   Layer 2 (high-level):   receive_frame(), transmit_frame()
                              │
   Layer 1 (wrappers):     frame_decode(), frame_build(), parse_sb2(), ...
                              │
   Layer 0 (C++ bindings): lunalink.afs._afs  (nanobind extension)

- **Layer 2** — structured results, exceptions, one-call pipelines.
  Start here.
- **Layer 1** — thin Python wrappers with docstrings over C++ bindings.
  Use when you need individual pipeline stages.
- **Layer 0** — raw C++ extension.  Not part of the public API; prefer
  Layer 1 wrappers.
