API Reference
=============

For a narrative introduction, see :doc:`quickstart`.

High-Level API
--------------

Most users should start here.

.. module:: lunalink.afs._receiver

.. autofunction:: lunalink.afs.receive_frame

.. module:: lunalink.afs._transmitter

.. autofunction:: lunalink.afs.transmit_frame

.. autofunction:: lunalink.afs.make_test_payload

Result Types
~~~~~~~~~~~~

.. automodule:: lunalink.afs._results
   :members:
   :show-inheritance:

Exceptions
~~~~~~~~~~

.. automodule:: lunalink.afs._errors
   :members:
   :show-inheritance:

Signal File I/O
~~~~~~~~~~~~~~~

The ``LSISIQ`` container carries I/Q samples as either ``float32`` or
``int16``; see :doc:`test_vectors` for the on-disk layout.
:func:`~lunalink.afs.write_iq_file` and :func:`~lunalink.afs.read_iq_file`
are documented under `Low-Level API`_ below; the result type they share is:

.. autoclass:: lunalink.afs.IqFile

Test Vector Generation
~~~~~~~~~~~~~~~~~~~~~~

.. automodule:: lunalink.afs.vectors
   :members:
   :show-inheritance:

Low-Level API
-------------

Thin Python wrappers over the C++ nanobind bindings.  Each function
corresponds to a single C++ call.

lunalink
~~~~~~~~

.. automodule:: lunalink
   :members:
   :exclude-members: AfsError, DecodeError, DecodeResult, SubframeResult, SyncNotFoundError, TimeOfTransmission, TransmitFrame, make_test_payload, receive_frame, transmit_frame
   :show-inheritance:

lunalink.afs
~~~~~~~~~~~~

.. automodule:: lunalink.afs
   :members:
   :exclude-members: _afs, FrameDecodeResult, FrameDecodeStatus, LegendreStatus, receive_frame, transmit_frame, make_test_payload, DecodeResult, SubframeResult, TimeOfTransmission, TransmitFrame, AfsError, DecodeError, SyncNotFoundError, MessageParseStatus, MessageType, Sb2ParseResult, Sb3ParseResult, Sb4ParseResult, TotResult, TotStatus, IqFile
   :show-inheritance:

lunalink.afs.signal
~~~~~~~~~~~~~~~~~~~

.. automodule:: lunalink.afs.signal
   :members:
   :exclude-members: FrameStatus, BchResult, bch_decode, BchStatus, Fid, Toi, LdpcStatus, LdpcSubframe, LdpcWorkspace, LdpcEncodeWorkspace, SyncDetectorStatus, SyncResult, MessageParseStatus, MessageType, Sb2ParseResult, Sb3ParseResult, Sb4ParseResult, TotResult, TotStatus, SisePercentileResult, SiseResult, SiseStatus, SiseVec, IqFile
   :show-inheritance:
