"""AFS signal generation: PRN codes and BPSK modulation."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._afs import modulate_i as _modulate_i
from lunalink.afs._afs import prn_code as _prn_code

__all__ = ["prn_code", "modulate_i"]


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
