"""AFS signal generation: PRN codes and BPSK modulation."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._afs import modulate_i as _modulate_i
from lunalink.afs._afs import prn_code as _prn_code
from lunalink.afs._afs import weil1500_code as _weil1500_code
from lunalink.afs._afs import weil10230_code as _weil10230_code

__all__ = ["prn_code", "weil10230_code", "weil1500_code", "modulate_i"]


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
