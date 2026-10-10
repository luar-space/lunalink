Learning and teaching with LunaLink
===================================

LunaLink can be used to learn how a satellite navigation signal is built, and
to teach it.  Every stage of the LSIS-AFS chain is a function you can call
from Python, and its docstring names the requirement it implements.

This page assumes you can program in Python and have met binary arithmetic and
the idea of a noisy channel.  It does not assume a background in navigation
signals or error-correcting codes; the notebooks introduce both.

A reading order
---------------

1. :doc:`notebooks/01_spreading_codes` explains why a navigation signal is
   spread, then builds the Gold and Weil codes and looks at their correlation
   properties.
2. :doc:`notebooks/02_error_correction` covers the BCH code, the CRC, the LDPC
   codes and the interleaver, and decodes through noise.
3. :doc:`notebooks/03_navigation_message` assembles a complete frame, decodes
   it, and recovers the time of transmission.

Each notebook has a matching guide that goes one level down, to the source
files and the tests: :doc:`verification/spreading_codes`,
:doc:`verification/error_correction`, :doc:`verification/framing` and
:doc:`verification/message_parsing`.  :doc:`signal/signal_chain` shows how the
stages connect.

There is no notebook yet for baseband signal generation, acquisition or the
receive chain.  :doc:`verification/baseband` and
:doc:`verification/sync_decoding` cover those blocks as reference pages.

Running it
----------

The notebooks are published in this documentation with their outputs, so they
can be read without installing anything.

To run them, build LunaLink as described in :doc:`quickstart` and start
Jupyter from the repository root:

.. code-block:: bash

   task install
   uv run --with jupyterlab jupyter lab docs/notebooks

Building needs a C++20 compiler, CMake and uv; the full list is in
:doc:`developing`.  The ``Dockerfile`` in the repository builds an image that
contains the ``lunalink`` command-line tool.  It does not contain Jupyter or
the notebooks.

The three scripts in ``examples/`` are shorter starting points: transmit a
frame, decode one, and run a round trip.

Exercises
---------

Each exercise uses only functions that already exist.  The snippets build on
one another: later ones reuse ``np`` and ``rng`` from earlier ones.

Correlation of spreading codes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A receiver finds a satellite by
correlating the incoming signal with a local copy of its code.  Map the chips
of two Gold codes to ±1, then compute the autocorrelation of one and the
cross-correlation of the two at every shift.

.. code-block:: python

   import numpy as np
   from lunalink.afs import prn_code

   a = 1 - 2 * prn_code(1).astype(np.int32)
   b = 1 - 2 * prn_code(2).astype(np.int32)
   auto = [np.dot(a, np.roll(a, k)) for k in range(len(a))]
   cross = [np.dot(a, np.roll(b, k)) for k in range(len(a))]

How large is the peak of ``auto``, and how large is everything else?  Repeat
with ``weil10230_code``.  What does the difference in code length buy?

How many errors can the frame header survive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Subframe 1 carries the
frame ID and the time of interval in a 52-symbol BCH codeword.  Encode one,
flip some symbols, and decode.

.. code-block:: python

   from lunalink.afs import bch_decode, bch_encode

   codeword = np.asarray(bch_encode(fid=2, toi=77))
   rng = np.random.default_rng(0)
   corrupted = codeword.copy()
   corrupted[rng.choice(52, size=3, replace=False)] ^= 1
   result = bch_decode(corrupted)
   print(result.status, result.fid, result.toi, result.hamming_distance)

Increase the number of flips.  At what point does the status stop being
``OK``, and at what point is the decoded header wrong?  Compare with
``bch_decode_soft`` on noisy symbols.

Noise, and why the decoder wants likelihoods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Build a frame, add
Gaussian noise, and decode it.

.. code-block:: python

   from lunalink.afs import frame_decode, make_test_payload, transmit_frame

   sb2, sb3, sb4 = make_test_payload("random", seed=42)
   frame, _ = transmit_frame(prn_id=1, fid=0, toi=5,
                             sb2_data=sb2, sb3_data=sb3, sb4_data=sb4)
   clean = np.where(frame.symbols == 0, 1.0, -1.0)

   sigma = 0.8
   y = (clean + rng.normal(0, sigma, clean.shape)).astype(np.float32)
   _, decoded_sb2, _, _, status = frame_decode(y, llr_scale=2 / sigma**2)
   print(status, np.array_equal(decoded_sb2, sb2))

Decode ``y`` again without ``llr_scale``.  The LDPC decoder works on
log-likelihood ratios, and the scale is what tells it how far to trust each
symbol.  Find the largest ``sigma`` at which each version still decodes.  The
reason is in the section on channel LLR scaling in
:doc:`signal/ldpc_performance`.

How many iterations does the LDPC decoder need
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``frame_decode`` takes
the iteration limit as ``ldpc_iterations``.  At a fixed ``sigma``, decode with
limits of 1, 2, 5, 15 and 50.  Compare what you find with the convergence
results in :doc:`signal/ldpc_algo_card`.

Time of transmission
~~~~~~~~~~~~~~~~~~~~

The receiver reconstructs time from three
fields: the week number, the interval time of week and the time of interval.

.. code-block:: python

   from lunalink.afs import compute_tot

   print(compute_tot(wn=100, itow=42, toi=17).t_f)

Derive the same number by hand from the durations of a week, an interval and
a frame.  Which field comes from subframe 1 and which from subframe 2?

Decode someone else's frame
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Clone the `shared test vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__.  Each file in
``frames/`` is a 64-byte header followed by 6000 symbols.

.. code-block:: python

   from lunalink.afs import receive_frame

   raw = open("frames/frame_message_1.bin", "rb").read()
   symbols = np.frombuffer(raw[64:], dtype=np.uint8)
   result = receive_frame(np.where(symbols == 0, 1.0, -1.0).astype(np.float32))
   print(result.fid, result.toi, result.all_ok)

Write your own generator and check it
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement the Gold code
generator from the description in the standard, write the codes in the format
described in :doc:`test_vectors`, and compare them with the shared set using
``python validate.py diff``.  :doc:`interoperate` has the details.  Any part
of the chain can be checked the same way.

Citing LunaLink
---------------

``CITATION.cff`` in the repository root has the citation in a form that
GitHub and reference managers read.
