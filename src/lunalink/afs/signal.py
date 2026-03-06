"""AFS signal generation: PRN codes and BPSK modulation."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._afs import BCH_CODEWORD_LENGTH as BCH_CODEWORD_LENGTH
from lunalink.afs._afs import EPOCHS_PER_FRAME as EPOCHS_PER_FRAME
from lunalink.afs._afs import FRAME_DURATION_S as FRAME_DURATION_S
from lunalink.afs._afs import FRAME_LENGTH as FRAME_LENGTH
from lunalink.afs._afs import INTERIM_ASSIGNMENT_MAX_PRN as INTERIM_ASSIGNMENT_MAX_PRN
from lunalink.afs._afs import IQ_SAMPLES_PER_EPOCH as IQ_SAMPLES_PER_EPOCH
from lunalink.afs._afs import IQ_UPSAMPLE_FACTOR as IQ_UPSAMPLE_FACTOR
from lunalink.afs._afs import PAYLOAD_LENGTH as PAYLOAD_LENGTH
from lunalink.afs._afs import SB1_LENGTH as SB1_LENGTH
from lunalink.afs._afs import SECONDARY_CODE_COUNT as SECONDARY_CODE_COUNT
from lunalink.afs._afs import SECONDARY_CODE_LENGTH as SECONDARY_CODE_LENGTH
from lunalink.afs._afs import SYMBOL_RATE as SYMBOL_RATE
from lunalink.afs._afs import SYNC_LENGTH as SYNC_LENGTH
from lunalink.afs._afs import TERTIARY_CODE_LENGTH as TERTIARY_CODE_LENGTH
from lunalink.afs._afs import FrameStatus as FrameStatus
from lunalink.afs._afs import bch_encode as _bch_encode
from lunalink.afs._afs import frame_build_partial as _frame_build_partial
from lunalink.afs._afs import modulate_i as _modulate_i
from lunalink.afs._afs import modulate_q as _modulate_q
from lunalink.afs._afs import multiplex_iq as _multiplex_iq
from lunalink.afs._afs import prn_code as _prn_code
from lunalink.afs._afs import tiered_code_epoch as _tiered_code_epoch
from lunalink.afs._afs import tiered_code_epoch_assigned as _tiered_code_epoch_assigned
from lunalink.afs._afs import weil1500_code as _weil1500_code
from lunalink.afs._afs import weil10230_code as _weil10230_code

__all__ = [
    "bch_encode",
    "frame_build_partial",
    "BCH_CODEWORD_LENGTH",
    "FRAME_LENGTH",
    "SYNC_LENGTH",
    "SB1_LENGTH",
    "PAYLOAD_LENGTH",
    "SYMBOL_RATE",
    "FRAME_DURATION_S",
    "prn_code",
    "weil10230_code",
    "weil1500_code",
    "modulate_i",
    "modulate_q",
    "multiplex_iq",
    "tiered_code_epoch",
    "tiered_code_epoch_assigned",
    "EPOCHS_PER_FRAME",
    "SECONDARY_CODE_LENGTH",
    "SECONDARY_CODE_COUNT",
    "INTERIM_ASSIGNMENT_MAX_PRN",
    "TERTIARY_CODE_LENGTH",
    "IQ_UPSAMPLE_FACTOR",
    "IQ_SAMPLES_PER_EPOCH",
    "FrameStatus",
]


def bch_encode(fid: int, toi: int) -> NDArray[np.uint8]:
    """Encode SB1 header using BCH(51,8).

    Per LSIS V1.0 §2.4.2: the 9-bit SB1 field (FID + TOI) is encoded into
    52 symbols using an 8-stage LFSR with the MSB prepended raw.

    Parameters
    ----------
    fid : int
        Frame Identifier, in [0, 3].
    toi : int
        Time of Interval, in [0, 99].

    Returns
    -------
    numpy.ndarray
        Shape (52,), dtype uint8, values in {0, 1}.

    Raises
    ------
    ValueError
        If fid or toi is out of range.
    """
    return _bch_encode(fid, toi)


def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]:
    """Build a partial AFS navigation frame (§2.4).

    Layout: 68-symbol sync + 52-symbol BCH-encoded SB1 + 5880 zero-padded
    symbols (placeholder for LDPC-encoded, interleaved SB2–SB4).

    Returns 6000 symbols as a uint8 array of {0, 1}.

    Args:
        fid: Frame Identifier (0–3)
        toi: Time of Interval (0–99)

    Returns
    -------
        NDArray[np.uint8]: The 6000-symbol frame.
    """
    return _frame_build_partial(fid, toi)


def prn_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Gold-2046 chip sequence for the given PRN (1-indexed).

    Parameters
    ----------
    prn_id : int
        PRN index in [1, 210].

    Returns
    -------
    numpy.ndarray
        Shape (2046,), dtype uint8, values in {0, 1}.

    Raises
    ------
    ValueError
        If prn_id is not in [1, 210].
    """
    return _prn_code(prn_id)


def modulate_i(prn: NDArray[np.uint8], data_symbol: int) -> NDArray[np.int8]:
    """Modulate a chip sequence with a BPSK data symbol (AFS-I channel).

    Implements the chip-to-sample mapping for the AFS-I channel per spec
    section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1, then multiplied
    by data_symbol.

    Parameters
    ----------
    prn : numpy.ndarray
        Chip sequence, shape (N,), dtype uint8, values in {0, 1}.
    data_symbol : int
        Data symbol, must be +1 or -1.

    Returns
    -------
    numpy.ndarray
        Shape (N,), dtype int8, values in {-1, +1}.

    Raises
    ------
    ValueError
        If data_symbol is not +/-1, or prn is not 1-D.
    """
    return _modulate_i(prn, data_symbol)


def weil10230_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-10230 chip sequence for the given PRN (1-indexed).

    This is the AFS-Q primary code used on the pilot channel.

    Parameters
    ----------
    prn_id : int
        PRN index in [1, 210].

    Returns
    -------
    numpy.ndarray
        Shape (10230,), dtype uint8, values in {0, 1}.

    Raises
    ------
    ValueError
        If prn_id is not in [1, 210].
    """
    return _weil10230_code(prn_id)


def weil1500_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-1500 chip sequence for the given PRN (1-indexed).

    This is the AFS-Q tertiary code used on the pilot channel.

    Parameters
    ----------
    prn_id : int
        PRN index in [1, 210].

    Returns
    -------
    numpy.ndarray
        Shape (1500,), dtype uint8, values in {0, 1}.

    Raises
    ------
    ValueError
        If prn_id is not in [1, 210].
    """
    return _weil1500_code(prn_id)


def tiered_code_epoch(prn_id: int, epoch_idx: int) -> NDArray[np.uint8]:
    """Return one primary epoch of the tiered AFS-Q code.

    Combines primary (Weil-10230) XOR secondary (4-bit) XOR tertiary
    (Weil-1500) codes per LSIS V1.0 section 2.3.5.2.

    Parameters
    ----------
    prn_id : int
        LNSP node identifier in [1, 12] for the default interim mapping.
        For other PRNs, use ``tiered_code_epoch_assigned``.
    epoch_idx : int
        Primary code epoch index within the 12 s frame, in [0, 5999].

    Returns
    -------
    numpy.ndarray
        Shape (10230,), dtype uint8, values in {0, 1}.

    Raises
    ------
    ValueError
        If prn_id or epoch_idx is out of range.
    """
    return _tiered_code_epoch(prn_id, epoch_idx)


def tiered_code_epoch_assigned(
    primary_prn: int,
    secondary_code_idx: int,
    tertiary_prn: int,
    tertiary_phase_offset: int,
    epoch_idx: int,
) -> NDArray[np.uint8]:
    """Return one primary epoch of the tiered AFS-Q code with explicit assignment.

    This API supports configurable mapping between LNSP node IDs and
    primary/secondary/tertiary code components, including tertiary phase offset.
    """
    return _tiered_code_epoch_assigned(
        primary_prn,
        secondary_code_idx,
        tertiary_prn,
        tertiary_phase_offset,
        epoch_idx,
    )


def modulate_q(chips: NDArray[np.uint8]) -> NDArray[np.int8]:
    """Modulate a chip sequence for the AFS-Q pilot channel (BPSK(5)).

    Applies the chip-to-sample mapping per spec section 2.3.3, Table 8:
    logic 0 -> +1, logic 1 -> -1. No data symbol (pilot channel).

    Parameters
    ----------
    chips : numpy.ndarray
        Chip sequence, shape (N,), dtype uint8, values in {0, 1}.

    Returns
    -------
    numpy.ndarray
        Shape (N,), dtype int8, values in {-1, +1}.

    Raises
    ------
    ValueError
        If chips is not 1-D.
    """
    return _modulate_q(chips)


def multiplex_iq(
    i_samples: NDArray[np.int8],
    q_samples: NDArray[np.int8],
) -> NDArray[np.int16]:
    """Multiplex AFS-I and AFS-Q into baseband IQ at 5.115 MSPS.

    AFS-I samples (2046) are upsampled 5x by sample-and-hold to match the
    AFS-Q chip rate (10230), per LSIS-140 Table 7. Channels are output with
    equal normalized digital amplitude. Absolute power scaling per LSIS-103
    and LSIS-130 is expected in downstream RF/payload stages.

    Parameters
    ----------
    i_samples : numpy.ndarray
        AFS-I modulated samples, shape (2046,), dtype int8, values in {-1, +1}.
    q_samples : numpy.ndarray
        AFS-Q modulated samples, shape (10230,), dtype int8, values in {-1, +1}.

    Returns
    -------
    numpy.ndarray
        Shape (10230, 2), dtype int16. Column 0 is I, column 1 is Q.

    Raises
    ------
    ValueError
        If input shapes are incorrect.
    """
    return _multiplex_iq(i_samples, q_samples)
