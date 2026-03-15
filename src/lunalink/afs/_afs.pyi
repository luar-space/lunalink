"""Type stubs for the _afs C++ extension module."""

from enum import IntEnum

import numpy as np
from numpy.typing import NDArray

class CrcStatus(IntEnum):
    OK = 0x5A
    NULL_INPUT = 0xCC
    OUTPUT_TOO_SMALL = 0x33
    FAULT_DETECTED = 0x99

class InterleaverStatus(IntEnum):
    OK = 0x5A
    INPUT_TOO_SMALL = 0xCC
    OUTPUT_TOO_SMALL = 0x33
    FAULT_DETECTED = 0x99

class FrameStatus(IntEnum):
    OK = 0x5A
    OUTPUT_TOO_SMALL = 0xA5
    INVALID_FID = 0x33
    INVALID_TOI = 0xCC
    BCH_FAILED = 0x0F
    FAULT_DETECTED = 0x99
    LDPC_FAILED = 0x3C
    CRC_FAILED = 0xC3

class LdpcSubframe(IntEnum):
    SF2 = 0x5A
    SF3 = 0xA5
    SF4 = 0x33

class LdpcStatus(IntEnum):
    OK = 0x5A
    INVALID_INPUT = 0xA5
    OUTPUT_TOO_SMALL = 0x33
    FAULT_DETECTED = 0x99
    NOT_CONVERGED = 0x66

class LdpcWorkspace:
    alpha_numerator: int
    alpha_denominator: int
    def __init__(self) -> None: ...

class BchStatus(IntEnum):
    OK = 0x5A
    OUTPUT_TOO_SMALL = 0xA5
    INVALID_FID = 0x33
    INVALID_TOI = 0xCC
    NULL_OUTPUT = 0x99
    INVALID_INPUT = 0x66
    AMBIGUOUS_MATCH = 0xF0
    FAULT_DETECTED = 0x0F

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
def bch_decode_soft(llr: NDArray[np.float32]) -> BchResult: ...
def bch_codebook_checksum() -> int: ...
def crc24_calculate(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def crc24_verify(data_with_crc: NDArray[np.uint8]) -> bool: ...
def block_interleave(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def block_deinterleave(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def ldpc_encode(type: LdpcSubframe, msg: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def ldpc_decode(
    type: LdpcSubframe,
    symbols: NDArray[np.float32],
    iterations: int,
    workspace: LdpcWorkspace | None = None,
) -> tuple[NDArray[np.uint8], LdpcStatus]: ...
def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]: ...
def frame_build(
    fid: int,
    toi: int,
    sb2_data: NDArray[np.uint8],
    sb3_data: NDArray[np.uint8],
    sb4_data: NDArray[np.uint8],
) -> NDArray[np.uint8]: ...
def subframe_build_sb2(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def subframe_build_sb3(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def subframe_build_sb4(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...

class LdpcEncodeWorkspace:
    def __init__(self) -> None: ...

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
INTERLEAVER_SIZE: int
INTERLEAVER_ROWS: int
INTERLEAVER_COLS: int
SB2_DATA_BITS: int
SB2_MSG_BITS: int
SB2_LENGTH: int
SB3_DATA_BITS: int
SB3_MSG_BITS: int
SB3_LENGTH: int
SB4_DATA_BITS: int
SB4_MSG_BITS: int
SB4_LENGTH: int
DEFAULT_SYNC_THRESHOLD: float

class SyncDetectorStatus(IntEnum):
    OK = 0x5A
    INVALID_INPUT = 0xA5
    NOT_DETECTED = 0x33
    FAULT_DETECTED = 0xCC

class SyncResult:
    frame_offset: int
    peak_metric: float
    second_peak: float
    detected: bool

def sync_detect(
    rx_symbols: NDArray[np.float32],
    threshold: float = 0.0,
) -> tuple[SyncResult, SyncDetectorStatus]: ...
