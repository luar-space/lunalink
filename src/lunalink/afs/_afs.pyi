"""Type stubs for the _afs C++ extension module."""

from enum import IntEnum

import numpy as np
from numpy.typing import NDArray

class FrameStatus(IntEnum):
    OK = 0
    NULL_OUTPUT = 1
    OUTPUT_TOO_SMALL = 2
    INVALID_FID = 3
    INVALID_TOI = 4
    BCH_FAILED = 5

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

def tiered_code_epoch(prn_id: int, epoch_idx: int) -> NDArray[np.uint8]:
    """Return one epoch of tiered AFS-Q code using interim default mapping.

    Interim default mapping is defined only for PRN 1-12.
    """
    ...

def tiered_code_epoch_assigned(
    primary_prn: int,
    secondary_code_idx: int,
    tertiary_prn: int,
    tertiary_phase_offset: int,
    epoch_idx: int,
) -> NDArray[np.uint8]:
    """Return one primary epoch using explicit AFS-Q code assignments."""
    ...

def modulate_q(chips: NDArray[np.uint8]) -> NDArray[np.int8]:
    """Modulate a chip sequence for AFS-Q pilot channel (BPSK(5), no data)."""
    ...

def multiplex_iq(
    i_samples: NDArray[np.int8],
    q_samples: NDArray[np.int8],
) -> NDArray[np.int16]:
    """Multiplex AFS-I and AFS-Q into baseband IQ at 5.115 MSPS."""
    ...

def bch_encode(fid: int, toi: int) -> NDArray[np.uint8]:
    """Encode SB1 (FID + TOI) using BCH(51,8) (§2.4.2.1). Returns 52 symbols."""
    ...

def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]:
    """Build a partial AFS navigation frame (§2.4).

    sync + BCH SB1 + zero-padded SB2-SB4. Returns 6000 symbols.
    """
    ...

BCH_CODEWORD_LENGTH: int
FRAME_LENGTH: int
SYNC_LENGTH: int
SB1_LENGTH: int
PAYLOAD_LENGTH: int
SYMBOL_RATE: int
FRAME_DURATION_S: int
EPOCHS_PER_FRAME: int
SECONDARY_CODE_LENGTH: int
SECONDARY_CODE_COUNT: int
INTERIM_ASSIGNMENT_MAX_PRN: int
TERTIARY_CODE_LENGTH: int
IQ_UPSAMPLE_FACTOR: int
IQ_SAMPLES_PER_EPOCH: int
