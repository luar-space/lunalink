# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""High-level RX convenience API."""

from __future__ import annotations

import math

from beartype import beartype
from jaxtyping import jaxtyped

from lunalink.afs._array_types import SoftSymbols
from lunalink.afs._errors import DecodeError, SyncNotFoundError
from lunalink.afs._results import (
    DecodeResult,
    SubframeResult,
    TimeOfTransmission,
    _readonly,
)
from lunalink.afs.signal import (
    BchStatus,
    FrameDecodeStatus,
    TotStatus,
    compute_tot,
    parse_sb2,
)
from lunalink.afs.signal import (
    frame_decode as _frame_decode_low,
)

# Largest finite float32: frame_decode takes the LLR scale as a float32.
_FLOAT32_MAX = 3.4028234663852886e38


@jaxtyped(typechecker=beartype)
def receive_frame(
    rx_symbols: SoftSymbols,
    *,
    ldpc_iterations: int = 50,
    sync_threshold: float = 0.0,
    noise_variance: float | None = None,
    raise_on_error: bool = True,
) -> DecodeResult:
    """Decode an AFS navigation frame from soft-decision symbols.

    This is the one-call RX entry point.  Given a stream of bipolar
    soft-decision symbols, it finds the sync pattern, decodes all
    subframes, parses timing information, and returns a structured
    :class:`~lunalink.afs.DecodeResult`.

    Parameters
    ----------
    rx_symbols : SoftSymbols
        Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1), at
        unit signal amplitude.  Must contain at least 6000 symbols after
        the sync offset.  A NaN or infinity in the located frame fails
        the decode, with status ``INVALID_INPUT`` in the payload or
        ``BCH_FAILED`` in SB1 (a ``DecodeError``, or a failed result when
        *raise_on_error* is False).
    ldpc_iterations : int
        LDPC decoder iteration limit (default: 50).
    sync_threshold : float
        Sync detection threshold (0 uses the C++ default of 50.0).
    noise_variance : float, optional
        Variance of the noise on ``rx_symbols``.  When given, the payload
        is scaled by ``2 / noise_variance`` before LDPC decoding, which
        turns the symbols into the channel log-likelihood ratios the
        decoder expects.  Sync detection and BCH decoding use the
        symbols as passed.  When omitted the symbols reach the LDPC
        decoder unscaled, which is only adequate for clean or nearly
        clean input.
    raise_on_error : bool
        If True (default), raises on failure.
        If False, returns a partial DecodeResult with error indicators.

    Returns
    -------
    DecodeResult
        Structured decode result with all subframe data and timing.

    Raises
    ------
    SyncNotFoundError
        If sync pattern is not detected (and *raise_on_error* is True).
    DecodeError
        If BCH, LDPC, or CRC decoding fails (and *raise_on_error* is True).
    ValueError
        If *noise_variance* is not finite and positive, or is so small that
        ``2 / noise_variance`` does not fit in a float32.

    Examples
    --------
    >>> from lunalink.afs import frame_build, receive_frame
    >>> import numpy as np
    >>> sb2 = np.zeros(1176, dtype=np.uint8)
    >>> sb3 = np.zeros(846, dtype=np.uint8)
    >>> sb4 = np.zeros(846, dtype=np.uint8)
    >>> frame = frame_build(0, 42, sb2, sb3, sb4)
    >>> bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
    >>> result = receive_frame(bipolar)
    >>> result.toi
    42
    """
    llr_scale = 1.0
    if noise_variance is not None:
        if not math.isfinite(noise_variance) or noise_variance <= 0.0:
            raise ValueError(
                f"noise_variance must be finite and positive, got {noise_variance}"
            )
        llr_scale = 2.0 / noise_variance
        if llr_scale > _FLOAT32_MAX:
            raise ValueError(
                f"noise_variance {noise_variance} is too small: the LLR scale "
                "2 / noise_variance must fit in a float32"
            )

    raw, sb2_data, sb3_data, sb4_data, status = _frame_decode_low(
        rx_symbols, ldpc_iterations, sync_threshold, llr_scale
    )

    # Fail fast: when sync is not found the remaining fields are meaningless.
    if status == FrameDecodeStatus.SYNC_NOT_FOUND:
        if raise_on_error:
            raise SyncNotFoundError("Sync pattern not detected in symbol stream")
        return DecodeResult(
            fid=0,
            toi=0,
            sb2=SubframeResult(
                data=_readonly(sb2_data), ldpc_converged=False, crc_ok=False
            ),
            sb3=SubframeResult(
                data=_readonly(sb3_data), ldpc_converged=False, crc_ok=False
            ),
            sb4=SubframeResult(
                data=_readonly(sb4_data), ldpc_converged=False, crc_ok=False
            ),
            time=None,
            sync_offset=raw.sync.frame_offset,
            sync_metric=raw.sync.peak_metric,
        )

    sb2 = SubframeResult(
        data=_readonly(sb2_data),
        ldpc_converged=raw.sb2.ldpc_converged,
        crc_ok=raw.sb2.crc_ok,
    )
    sb3 = SubframeResult(
        data=_readonly(sb3_data),
        ldpc_converged=raw.sb3.ldpc_converged,
        crc_ok=raw.sb3.crc_ok,
    )
    sb4 = SubframeResult(
        data=_readonly(sb4_data),
        ldpc_converged=raw.sb4.ldpc_converged,
        crc_ok=raw.sb4.crc_ok,
    )

    # Parse timing from SB2 when possible.
    time: TimeOfTransmission | None = None
    if raw.bch.status == BchStatus.OK and sb2.crc_ok:
        sb2_parsed = parse_sb2(sb2_data)
        tot = compute_tot(sb2_parsed.wn, sb2_parsed.itow, int(raw.bch.toi))
        if tot.status == TotStatus.OK:
            time = TimeOfTransmission(
                wn=sb2_parsed.wn,
                itow=sb2_parsed.itow,
                toi=int(raw.bch.toi),
                t_f=tot.t_f,
            )

    result = DecodeResult(
        fid=int(raw.bch.fid),
        toi=int(raw.bch.toi),
        sb2=sb2,
        sb3=sb3,
        sb4=sb4,
        time=time,
        sync_offset=raw.sync.frame_offset,
        sync_metric=raw.sync.peak_metric,
    )

    if raise_on_error and status != FrameDecodeStatus.OK:
        raise DecodeError(status, result)

    return result
