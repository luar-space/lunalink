"""Type stubs for the _afs C++ extension module."""

from enum import IntEnum

import numpy as np
from numpy.typing import NDArray

class FrameStatus(IntEnum):
    OK = 0x5A
    OUTPUT_TOO_SMALL = 0xA5
    INVALID_FID = 0x33
    INVALID_TOI = 0xCC
    BCH_FAILED = 0x0F

class BchStatus(IntEnum):
    OK = 0x5A
    OUTPUT_TOO_SMALL = 0xA5
    INVALID_FID = 0x33
    INVALID_TOI = 0xCC
    NULL_OUTPUT = 0x0F
    INVALID_INPUT = 0xF0
    AMBIGUOUS_MATCH = 0x66
    FAULT_DETECTED = 0x99

class Fid:
    value: int
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    @staticmethod
    def NODE1() -> Fid: ...  # noqa: N802
    @staticmethod
    def NODE2() -> Fid: ...  # noqa: N802
    @staticmethod
    def NODE3() -> Fid: ...  # noqa: N802
    @staticmethod
    def NODE4() -> Fid: ...  # noqa: N802

class Toi:
    value: int
    def __init__(self, value: int) -> None: ...

class BchResult:
    status: BchStatus
    fid: Fid
    toi: Toi
    hamming_distance: int

def prn_code(prn_id: int) -> NDArray[np.uint8]: ...
def weil10230_code(prn_id: int) -> NDArray[np.uint8]: ...
def weil1500_code(prn_id: int) -> NDArray[np.uint8]: ...
def modulate_i(prn: NDArray[np.uint8], data: int) -> NDArray[np.int8]: ...
def modulate_q(chips: NDArray[np.uint8]) -> NDArray[np.int8]: ...
def multiplex_iq(i: NDArray[np.int8], q: NDArray[np.int8]) -> NDArray[np.int16]: ...
def matched_code_epoch(prn_id: int, epoch: int) -> NDArray[np.uint8]: ...
def matched_code_epoch_assigned(
    p_prn: int, s_idx: int, t_prn: int, t_phase: int, epoch: int
) -> NDArray[np.uint8]: ...
def bch_encode(fid: int, toi: int) -> NDArray[np.uint8]: ...
def bch_decode(codeword: NDArray[np.uint8]) -> BchResult: ...
def bch_codebook_checksum() -> int: ...
def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]: ...

EPOCHS_PER_FRAME: int
SECONDARY_CODE_LENGTH: int
SECONDARY_CODE_COUNT: int
INTERIM_ASSIGNMENT_MAX_PRN: int
TERTIARY_CODE_LENGTH: int
IQ_UPSAMPLE_FACTOR: int
IQ_SAMPLES_PER_EPOCH: int
FRAME_LENGTH: int
SYNC_LENGTH: int
SB1_LENGTH: int
PAYLOAD_LENGTH: int
SYMBOL_RATE: int
FRAME_DURATION_S: float
BCH_CODEWORD_LENGTH: int
