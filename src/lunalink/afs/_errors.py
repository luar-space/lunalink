"""AFS high-level exception hierarchy.

All exceptions inherit from :class:`AfsError`, so you can catch broadly
or narrowly:

.. code-block:: python

    from lunalink import receive_frame, AfsError, SyncNotFoundError, DecodeError

    try:
        result = receive_frame(symbols)
    except SyncNotFoundError:
        # No sync pattern detected — signal may be absent or too weak.
        ...
    except DecodeError as e:
        # FEC or CRC failure — partial result available for diagnostics.
        print(f"Failed at stage: {e.status.name}")
        print(f"SB2 CRC ok? {e.result.sb2.crc_ok}")
    except AfsError:
        # Catch-all for any future AFS exception subclass.
        ...
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from lunalink.afs._afs import FrameDecodeStatus
    from lunalink.afs._results import DecodeResult


class AfsError(Exception):
    """Base exception for all AFS operations."""


class SyncNotFoundError(AfsError):
    """Sync pattern not detected in the symbol stream."""


class DecodeError(AfsError):
    """Frame decoding failed (BCH, LDPC, or CRC).

    The partial :attr:`result` is attached so callers can inspect
    which stages succeeded before the failure.
    """

    def __init__(self, status: FrameDecodeStatus, result: DecodeResult) -> None:
        self.status = status
        self.result = result
        super().__init__(f"Frame decode failed: {status.name}")
