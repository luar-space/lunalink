# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""High-level TX convenience API."""

from __future__ import annotations

import numpy as np
from beartype import beartype
from jaxtyping import jaxtyped

from lunalink.afs._array_types import (
    IqPairFloat,
    Sb2Payload,
    Sb3Payload,
    Sb4Payload,
)
from lunalink.afs._results import TransmitFrame, _readonly
from lunalink.afs.signal import (
    SB2_DATA_BITS,
    SB2_SPARE_BITS,
    SB2_SPARE_OFFSET,
    SB3_DATA_BITS,
    SB4_DATA_BITS,
    frame_build,
    generate_iq_frame,
)


@jaxtyped(typechecker=beartype)
def transmit_frame(
    *,
    prn_id: int,
    fid: int,
    toi: int,
    sb2_data: Sb2Payload,
    sb3_data: Sb3Payload,
    sb4_data: Sb4Payload,
    sample_rate: float | None = None,
) -> tuple[TransmitFrame, IqPairFloat]:
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
    sb2_data, sb3_data, sb4_data : Sb2Payload, Sb3Payload, Sb4Payload
        Raw subframe payload bits (1176, 846, 846 respectively).
    sample_rate : float, optional
        Output sample rate in Hz.  ``None`` for native 5.115 MHz.

    Returns
    -------
    tuple[TransmitFrame, IqPairFloat]
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


def _apply_sb2_spare_pattern(sb2: np.ndarray) -> np.ndarray:
    """Write the LSIS-AFS-300 spare pattern into an SB2 payload.

    SB2 bits ``[SB2_SPARE_OFFSET, +SB2_SPARE_BITS)`` are spare and shall be
    alternating 0/1 starting with 0.  ``frame_build`` enforces this on the
    encode side regardless of what the caller supplies, so a payload that
    does not already carry the pattern cannot round-trip bit-exactly.
    """
    out = np.array(sb2, dtype=np.uint8, copy=True)
    lo = SB2_SPARE_OFFSET
    hi = lo + SB2_SPARE_BITS
    out[lo:hi] = np.arange(SB2_SPARE_BITS, dtype=np.uint8) % 2
    return out


def make_test_payload(
    pattern: str = "zeros",
    seed: int = 0x12345678,
) -> tuple[Sb2Payload, Sb3Payload, Sb4Payload]:
    """Generate standard test payloads for SB2, SB3, SB4.

    Parameters
    ----------
    pattern : str
        One of ``"zeros"``, ``"ones"``, ``"alternating"``, ``"random"``.
    seed : int
        PRNG seed for the ``"random"`` pattern (default: ``0x12345678``).

    Returns
    -------
    tuple[Sb2Payload, Sb3Payload, Sb4Payload]
        ``(sb2_data, sb3_data, sb4_data)`` ready for
        (the SB2 spare region carries the LSIS-AFS-300 alternating
        pattern, so payloads round-trip bit-exactly)
        :func:`~lunalink.afs.frame_build` or :func:`transmit_frame`.
    """
    n2, n3, n4 = SB2_DATA_BITS, SB3_DATA_BITS, SB4_DATA_BITS

    if pattern == "zeros":
        return (
            _apply_sb2_spare_pattern(np.zeros(n2, dtype=np.uint8)),
            np.zeros(n3, dtype=np.uint8),
            np.zeros(n4, dtype=np.uint8),
        )

    if pattern == "ones":
        return (
            _apply_sb2_spare_pattern(np.ones(n2, dtype=np.uint8)),
            np.ones(n3, dtype=np.uint8),
            np.ones(n4, dtype=np.uint8),
        )

    if pattern == "alternating":
        return (
            _apply_sb2_spare_pattern(
                np.array([i % 2 for i in range(n2)], dtype=np.uint8)
            ),
            np.array([i % 2 for i in range(n3)], dtype=np.uint8),
            np.array([i % 2 for i in range(n4)], dtype=np.uint8),
        )

    if pattern == "random":
        rng = np.random.default_rng(seed)
        return (
            _apply_sb2_spare_pattern(rng.integers(0, 2, size=n2, dtype=np.uint8)),
            rng.integers(0, 2, size=n3, dtype=np.uint8),
            rng.integers(0, 2, size=n4, dtype=np.uint8),
        )

    valid = "'zeros', 'ones', 'alternating', or 'random'"
    raise ValueError(f"Unknown pattern {pattern!r}; expected {valid}")
