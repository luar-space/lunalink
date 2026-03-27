"""High-level TX convenience API."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._results import TransmitFrame, _readonly
from lunalink.afs.signal import (
    SB2_DATA_BITS,
    SB3_DATA_BITS,
    SB4_DATA_BITS,
    frame_build,
    generate_iq_frame,
)


def transmit_frame(
    *,
    prn_id: int,
    fid: int,
    toi: int,
    sb2_data: NDArray[np.uint8],
    sb3_data: NDArray[np.uint8],
    sb4_data: NDArray[np.uint8],
    sample_rate: float | None = None,
) -> tuple[TransmitFrame, NDArray[np.float32]]:
    """Build and modulate a complete AFS navigation frame.

    Combines :func:`~lunalink.afs.frame_build` and
    :func:`~lunalink.afs.generate_iq_frame` into a single call.

    Parameters
    ----------
    prn_id : int
        PRN ID (1-12 for interim codes).
    fid : int
        Frame ID / node identity (0-3).
    toi : int
        Time of Interval (0-99).
    sb2_data, sb3_data, sb4_data : NDArray[np.uint8]
        Raw subframe payload bits (1176, 846, 846 respectively).
    sample_rate : float, optional
        Output sample rate in Hz.  ``None`` for native 5.115 MHz.

    Returns
    -------
    tuple[TransmitFrame, NDArray[np.float32]]
        (frame_metadata, iq_samples).

    Examples
    --------
    >>> from lunalink.afs import transmit_frame, make_test_payload, write_iq_file
    >>> sb2, sb3, sb4 = make_test_payload("zeros")
    >>> frame, iq = transmit_frame(
    ...     prn_id=1, fid=0, toi=42,
    ...     sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
    ... )
    """
    symbols = frame_build(fid, toi, sb2_data, sb3_data, sb4_data)
    iq = generate_iq_frame(prn_id, symbols, sample_rate)
    meta = TransmitFrame(symbols=_readonly(symbols), fid=fid, toi=toi, prn_id=prn_id)
    return meta, iq


def make_test_payload(
    pattern: str = "zeros",
    seed: int = 0x12345678,
) -> tuple[NDArray[np.uint8], NDArray[np.uint8], NDArray[np.uint8]]:
    """Generate standard test payloads for SB2, SB3, SB4.

    Parameters
    ----------
    pattern : str
        One of ``"zeros"``, ``"ones"``, ``"alternating"``, ``"random"``.
    seed : int
        PRNG seed for the ``"random"`` pattern (default: ``0x12345678``).

    Returns
    -------
    tuple[NDArray, NDArray, NDArray]
        ``(sb2_data, sb3_data, sb4_data)`` ready for
        :func:`~lunalink.afs.frame_build` or :func:`transmit_frame`.
    """
    n2, n3, n4 = SB2_DATA_BITS, SB3_DATA_BITS, SB4_DATA_BITS

    if pattern == "zeros":
        return (
            np.zeros(n2, dtype=np.uint8),
            np.zeros(n3, dtype=np.uint8),
            np.zeros(n4, dtype=np.uint8),
        )

    if pattern == "ones":
        return (
            np.ones(n2, dtype=np.uint8),
            np.ones(n3, dtype=np.uint8),
            np.ones(n4, dtype=np.uint8),
        )

    if pattern == "alternating":
        return (
            np.array([i % 2 for i in range(n2)], dtype=np.uint8),
            np.array([i % 2 for i in range(n3)], dtype=np.uint8),
            np.array([i % 2 for i in range(n4)], dtype=np.uint8),
        )

    if pattern == "random":
        rng = np.random.default_rng(seed)
        return (
            rng.integers(0, 2, size=n2, dtype=np.uint8),
            rng.integers(0, 2, size=n3, dtype=np.uint8),
            rng.integers(0, 2, size=n4, dtype=np.uint8),
        )

    valid = "'zeros', 'ones', 'alternating', or 'random'"
    raise ValueError(f"Unknown pattern {pattern!r}; expected {valid}")
