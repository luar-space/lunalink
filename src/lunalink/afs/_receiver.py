"""High-level RX convenience API."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._errors import DecodeError, SyncNotFoundError
from lunalink.afs._results import DecodeResult, SubframeResult, TimeOfTransmission
from lunalink.afs.signal import (
    BchStatus,
    FrameDecodeStatus,
    MessageParseStatus,
    TotStatus,
    compute_tot,
    frame_decode as _frame_decode_low,
    parse_sb2,
)


def receive_frame(
    rx_symbols: NDArray[np.float32],
    *,
    ldpc_iterations: int = 50,
    sync_threshold: float = 0.0,
    raise_on_error: bool = True,
) -> DecodeResult:
    """Decode an AFS navigation frame from soft-decision symbols.

    This is the one-call RX entry point.  Given a stream of bipolar
    soft-decision symbols, it finds the sync pattern, decodes all
    subframes, parses timing information, and returns a structured
    :class:`~lunalink.afs.DecodeResult`.

    Parameters
    ----------
    rx_symbols : array of float32
        Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1).
        Must contain at least 6000 symbols after the sync offset.
    ldpc_iterations : int
        LDPC decoder iteration limit (default: 50).
    sync_threshold : float
        Sync detection threshold (0 uses the C++ default of 50.0).
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
    raw, sb2_data, sb3_data, sb4_data, status = _frame_decode_low(
        rx_symbols, ldpc_iterations, sync_threshold
    )

    sb2 = SubframeResult(
        data=sb2_data,
        ldpc_converged=raw.sb2.ldpc_converged,
        crc_ok=raw.sb2.crc_ok,
    )
    sb3 = SubframeResult(
        data=sb3_data,
        ldpc_converged=raw.sb3.ldpc_converged,
        crc_ok=raw.sb3.crc_ok,
    )
    sb4 = SubframeResult(
        data=sb4_data,
        ldpc_converged=raw.sb4.ldpc_converged,
        crc_ok=raw.sb4.crc_ok,
    )

    # Parse timing from SB2 when possible.
    time: TimeOfTransmission | None = None
    if raw.bch.status == BchStatus.OK and sb2.crc_ok:
        sb2_parsed = parse_sb2(sb2_data)
        if sb2_parsed.status == MessageParseStatus.OK:
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

    if raise_on_error:
        if status == FrameDecodeStatus.SYNC_NOT_FOUND:
            raise SyncNotFoundError("Sync pattern not detected in symbol stream")
        if status != FrameDecodeStatus.OK:
            raise DecodeError(status, result)

    return result
