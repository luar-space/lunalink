"""Type stubs for the _afs C++ extension module."""

import numpy as np
from numpy.typing import NDArray

def prn_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Gold-2046 chip sequence for PRN prn_id (1-indexed)."""
    ...

def weil10230_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-10230 chip sequence for PRN prn_id (1-indexed)."""
    ...

def weil1500_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-1500 chip sequence for PRN prn_id (1-indexed)."""
    ...

def modulate_i(prn: NDArray[np.uint8], data_symbol: int) -> NDArray[np.int8]:
    """Modulate a chip sequence with a BPSK data symbol (AFS-I channel)."""
    ...
