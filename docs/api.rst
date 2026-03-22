API Reference
=============

For a narrative introduction, see :doc:`quickstart`.

High-Level API
--------------

These are the recommended entry points for most users.

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

Test Vector Generation
~~~~~~~~~~~~~~~~~~~~~~

.. automodule:: lunalink.afs.vectors
   :members:
   :show-inheritance:

Low-Level API
-------------

Thin Python wrappers over the C++ pybind11 bindings.  Each function
corresponds to a single C++ call.

lunalink
~~~~~~~~

.. automodule:: lunalink
   :members:
   :show-inheritance:

lunalink.afs
~~~~~~~~~~~~

.. automodule:: lunalink.afs
   :members:
   :exclude-members: _afs, FrameDecodeResult, FrameDecodeStatus, LegendreStatus, receive_frame, transmit_frame, make_test_payload, DecodeResult, SubframeResult, TimeOfTransmission, TransmitFrame, AfsError, DecodeError, SyncNotFoundError, MessageParseStatus, MessageType, Sb2ParseResult, Sb3ParseResult, Sb4ParseResult, TotResult, TotStatus
   :show-inheritance:

lunalink.afs.signal
~~~~~~~~~~~~~~~~~~~

.. automodule:: lunalink.afs.signal
   :members:
   :exclude-members: FrameStatus, BchResult, bch_decode, BchStatus, Fid, Toi, LdpcStatus, LdpcSubframe, LdpcWorkspace, LdpcEncodeWorkspace, SyncDetectorStatus, SyncResult, MessageParseStatus, MessageType, Sb2ParseResult, Sb3ParseResult, Sb4ParseResult, TotResult, TotStatus
   :show-inheritance:
