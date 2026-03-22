"""AFS high-level exception hierarchy."""

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
