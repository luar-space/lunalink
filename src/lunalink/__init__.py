"""LunaLink — LSIS-AFS reference implementation.

Top-level convenience re-exports for the most common entry points.
The full low-level API lives in :mod:`lunalink.afs`.

Example
-------
>>> from lunalink import transmit_frame, receive_frame, make_test_payload
>>> import numpy as np
>>> sb2, sb3, sb4 = make_test_payload("zeros")
>>> frame, iq = transmit_frame(
...     prn_id=1, fid=0, toi=42,
...     sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
... )
>>> symbols = np.where(frame.symbols == 0, 1.0, -1.0).astype(np.float32)
>>> result = receive_frame(symbols)
>>> result.all_ok
True
"""

from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as _get_version

from lunalink.afs._errors import AfsError, DecodeError, SyncNotFoundError
from lunalink.afs._receiver import receive_frame
from lunalink.afs._results import (
    DecodeResult,
    SubframeResult,
    TimeOfTransmission,
    TransmitFrame,
)
from lunalink.afs._transmitter import make_test_payload, transmit_frame

__all__ = [
    "__version__",
    # High-level TX / RX
    "transmit_frame",
    "receive_frame",
    "make_test_payload",
    # Result types
    "DecodeResult",
    "SubframeResult",
    "TimeOfTransmission",
    "TransmitFrame",
    # Errors
    "AfsError",
    "DecodeError",
    "SyncNotFoundError",
]

try:
    __version__: str = _get_version("lunalink")
except PackageNotFoundError:  # pragma: no cover
    __version__ = "unknown"
