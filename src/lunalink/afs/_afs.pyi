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

class PrnId:
    value: int
    def __init__(self, value: int) -> None: ...
    def valid(self) -> bool: ...
    def __int__(self) -> int: ...

class PrnStatus(IntEnum):
    OK = 0x5A
    INVALID_PRN = 0xA5
    NULL_OUTPUT = 0x33
    INVALID_CHIP_INDEX = 0xCC
    FAULT_DETECTED = 0x99

class MatchedCodeStatus(IntEnum):
    OK = 0x5A
    INVALID_PRN = 0xA5
    INVALID_EPOCH = 0x33
    OUTPUT_TOO_SMALL = 0xCC
    INVALID_ASSIGNMENT = 0x66
    FAULT_DETECTED = 0x99

class ModulationStatus(IntEnum):
    OK = 0x5A
    INVALID_SYMBOL = 0xA5
    INVALID_CHIP_VALUE = 0x33
    OUTPUT_TOO_SMALL = 0xCC
    FAULT_DETECTED = 0x99

class IqMuxStatus(IntEnum):
    OK = 0x5A
    INPUT_TOO_SMALL = 0xA5
    OUTPUT_TOO_SMALL = 0x33
    INVALID_I_SAMPLE = 0xCC
    INVALID_Q_SAMPLE = 0x66
    NULL_INPUT = 0x99
    FAULT_DETECTED = 0x0F

class LegendreStatus(IntEnum):
    OK = 0x5A
    OUTPUT_TOO_SMALL = 0xCC
    INVALID_PRIME = 0x33
    FAULT_DETECTED = 0x99

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
def default_matched_assignment(
    prn_id: int,
) -> tuple[int, int, int, int]: ...
def bch_encode(fid: int, toi: int) -> NDArray[np.uint8]: ...
def bch_decode(codeword: NDArray[np.uint8]) -> BchResult: ...
def bch_decode_soft(llr: NDArray[np.float32]) -> BchResult: ...
def bch_codebook_checksum() -> int: ...
def crc24_calculate(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def crc24_verify(data_with_crc: NDArray[np.uint8]) -> bool: ...
def block_interleave(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def block_deinterleave(data: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def block_deinterleave_soft(data: NDArray[np.float32]) -> NDArray[np.float32]: ...
def commit_frame(frame: NDArray[np.uint8]) -> FrameStatus: ...
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

class FrameDecodeStatus(IntEnum):
    OK = 0x5A
    SYNC_NOT_FOUND = 0xA5
    BCH_FAILED = 0x33
    LDPC_FAILED = 0xCC
    CRC_FAILED = 0x0F
    INVALID_INPUT = 0x66
    FAULT_DETECTED = 0x99

class SubframeDecodeResult:
    ldpc_converged: bool
    crc_ok: bool

class FrameDecodeResult:
    status: FrameDecodeStatus
    sync: SyncResult
    bch: BchResult
    sb2: SubframeDecodeResult
    sb3: SubframeDecodeResult
    sb4: SubframeDecodeResult

def frame_decode(
    rx_symbols: NDArray[np.float32],
    ldpc_iterations: int = 50,
    sync_threshold: float = 0.0,
) -> tuple[
    FrameDecodeResult,
    NDArray[np.uint8],
    NDArray[np.uint8],
    NDArray[np.uint8],
    FrameDecodeStatus,
]: ...

class CorrelatorStatus(IntEnum):
    OK = 0x5A
    INVALID_INPUT = 0xA5
    ACQ_FAILED = 0x33
    FAULT_DETECTED = 0xCC
    NOT_LOCKED = 0x99
    TIMEOUT = 0x66

class ChannelState(IntEnum):
    IDLE = 0x1E
    ACQUIRING = 0x69
    TRACKING = 0x96
    LOST = 0xE1

class AcquisitionResult:
    code_delay: float
    doppler_hz: float
    peak_metric: float
    locked: bool

class TrackingState:
    code_phase_chips: float
    carrier_phase_rad: float
    doppler_hz: float
    dll_discriminator: float
    pll_discriminator: float
    cn0_estimate: float
    cn0_smooth: float

class UniformPrior:
    def __init__(self) -> None: ...

class Correlator:
    def __init__(self, prior: UniformPrior) -> None: ...
    def assign_channel(self, ch_idx: int, prn_id: int) -> CorrelatorStatus: ...
    def acquire(
        self, ch_idx: int, rx: NDArray[np.complex64]
    ) -> tuple[AcquisitionResult, CorrelatorStatus]: ...
    def track(self, ch_idx: int, rx: NDArray[np.complex64]) -> CorrelatorStatus: ...
    def pseudorange(self, ch_idx: int) -> tuple[float, CorrelatorStatus]: ...
    def channel_state(self, ch_idx: int) -> ChannelState: ...
    def tracking_state(self, ch_idx: int) -> TrackingState: ...
    def data_symbol(self, ch_idx: int) -> float: ...
    def reset_channel(self, ch_idx: int) -> None: ...

AFS_I_CODE_LENGTH: int
FFT_SIZE: int
SAMPLE_RATE_HZ: float
CODE_PERIOD_S: float
MAX_CHANNELS: int
GOLD_CHIP_LENGTH: int
WEIL_10230_CHIP_LENGTH: int
WEIL_1500_CHIP_LENGTH: int
PRN_COUNT: int
CHIP_RATE_HZ: float
DOPPLER_BIN_SPACING_HZ: float

def legendre_cache_init() -> LegendreStatus: ...
def legendre_10223_checksum() -> int: ...
def legendre_1499_checksum() -> int: ...

# ─── Gateway 6: Message Parsing ──────────────────────────────────────────────

class MessageType(IntEnum):
    G1 = 0x01
    G2 = 0x02
    G3 = 0x03
    G4 = 0x04
    G5 = 0x05
    G8 = 0x08
    G10 = 0x0A
    G11 = 0x0B
    G14 = 0x0E
    G15 = 0x0F
    G17 = 0x11
    S19 = 0x13
    S20 = 0x14
    G22 = 0x16
    G23 = 0x17
    G24 = 0x18
    G25 = 0x19
    G27 = 0x1B
    G28 = 0x1C
    G29 = 0x1D
    G30 = 0x1E
    G31 = 0x1F
    G32 = 0x20
    UNKNOWN = 0xFF

class MessageParseStatus(IntEnum):
    OK = 0x5A
    INVALID_INPUT = 0xA5
    FAULT_DETECTED = 0x99

class TotStatus(IntEnum):
    OK = 0x5A
    INVALID_TOI = 0xA5
    INVALID_ITOW = 0x33
    INVALID_WN = 0xCC
    INVALID_DELTA = 0x3C
    FAULT_DETECTED = 0x99

class Sb2ParseResult:
    status: MessageParseStatus
    wn: int
    itow: int
    data_offset: int
    payload_size: int

class Sb3ParseResult:
    status: MessageParseStatus
    type_raw: int
    type: MessageType
    data_offset: int
    payload_size: int

class Sb4ParseResult:
    status: MessageParseStatus
    type_raw: int
    type: MessageType
    data_offset: int
    payload_size: int

class TotResult:
    status: TotStatus
    t_f: float

def parse_sb2(sb2_data: NDArray[np.uint8]) -> Sb2ParseResult: ...
def parse_sb3(sb3_data: NDArray[np.uint8]) -> Sb3ParseResult: ...
def parse_sb4(sb4_data: NDArray[np.uint8]) -> Sb4ParseResult: ...
def compute_tot(
    wn: int, itow: int, toi: int, delta_t_lrt: float = 0.0
) -> TotResult: ...

SUBFRAME_TYPE_BITS: int
WN_BITS: int
ITOW_BITS: int
TOT_HEADER_BITS: int
SEC_WEEK: int
BLOCK_INTERVAL_DURATION: int
FRAME_DURATION_SEC: int
WN_MAX: int
ITOW_MAX: int
TOT_TOI_MAX: int
SB2_PAYLOAD_BITS: int
SB3_PAYLOAD_BITS: int
SB4_PAYLOAD_BITS: int
