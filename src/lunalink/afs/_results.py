"""Structured result types for the high-level AFS API."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray


@dataclass(frozen=True, slots=True)
class SubframeResult:
    """Decoded subframe with quality metadata."""

    data: NDArray[np.uint8]
    ldpc_converged: bool
    crc_ok: bool


@dataclass(frozen=True, slots=True)
class TimeOfTransmission:
    """Parsed time of transmission per LSIS-720.

    Formula: ``t_F = WN * 604800 + ITOW * 1200 + TOI * 12 + delta_t_lrt``
    """

    wn: int  # Week Number (0..8191)
    itow: int  # Interval Time of Week (0..503)
    toi: int  # Time of Interval (0..99)
    t_f: float  # seconds since LRT epoch

    @property
    def seconds_of_week(self) -> float:
        """Time within the current week in seconds."""
        return self.itow * 1200.0 + self.toi * 12.0


@dataclass(frozen=True, slots=True)
class DecodeResult:
    """Complete result of decoding one AFS navigation frame.

    Returned by :func:`~lunalink.afs.receive_frame`.  Inspect
    :attr:`all_ok` for a quick pass/fail, or drill into individual
    subframe results for diagnostics.
    """

    fid: int
    toi: int
    sb2: SubframeResult
    sb3: SubframeResult
    sb4: SubframeResult
    time: TimeOfTransmission | None  # None if SB2 parse failed
    sync_offset: int
    sync_metric: float

    @property
    def all_ok(self) -> bool:
        """True if every subframe decoded and CRC-verified successfully."""
        return self.sb2.crc_ok and self.sb3.crc_ok and self.sb4.crc_ok


@dataclass(frozen=True, slots=True)
class TransmitFrame:
    """A ready-to-modulate AFS frame with metadata."""

    symbols: NDArray[np.uint8]  # 6000 binary symbols
    fid: int
    toi: int
    prn_id: int
