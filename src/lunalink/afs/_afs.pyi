"""Type stubs for the _afs C++ extension module."""

from enum import IntEnum

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._array_types import (
    BchCodeword,
    BchSoftInput,
    BitArray,
    BpskChipsI,
    BpskChipsQ,
    ErrorSeries,
    FrameSymbols,
    GoldCode,
    InterleaverBlock,
    InterleaverBlockSoft,
    IqMuxNative,
    IqPairFloat,
    IqPairNative,
    MatchedCode,
    Sb2Payload,
    Sb3Payload,
    Sb4Payload,
    SoftSymbols,
    WeilPrimary,
    WeilTertiary,
)

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
    def __int__(self) -> int: ...

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

# ── Spreading codes ──────────────────────────────────────────────────────────

def prn_code(prn_id: int) -> GoldCode: ...
def weil10230_code(prn_id: int) -> WeilPrimary: ...
def weil1500_code(prn_id: int) -> WeilTertiary: ...
def matched_code_epoch(prn_id: int, epoch: int) -> MatchedCode: ...
def matched_code_epoch_assigned(
    p_prn: int, s_idx: int, t_prn: int, t_phase: int, epoch: int
) -> MatchedCode: ...
def default_matched_assignment(
    prn_id: int,
) -> tuple[int, int, int, int]: ...

# ── Modulation & multiplexing ────────────────────────────────────────────────

def modulate_i(prn: GoldCode, data: int) -> BpskChipsI: ...
def modulate_q(chips: WeilPrimary) -> BpskChipsQ: ...
def multiplex_iq(i: BpskChipsI, q: BpskChipsQ) -> IqMuxNative: ...

# ── Forward error correction ─────────────────────────────────────────────────

def bch_encode(fid: int, toi: int) -> BchCodeword: ...
def bch_decode(codeword: BchCodeword) -> BchResult: ...
def bch_decode_soft(llr: BchSoftInput) -> BchResult: ...
def bch_codebook_checksum() -> int: ...
def crc24_calculate(data: BitArray) -> BitArray: ...
def crc24_verify(data_with_crc: BitArray) -> bool: ...
def block_interleave(data: InterleaverBlock) -> InterleaverBlock: ...
def block_deinterleave(data: InterleaverBlock) -> InterleaverBlock: ...
def block_deinterleave_soft(
    data: InterleaverBlockSoft,
) -> InterleaverBlockSoft: ...
def commit_frame(frame: FrameSymbols) -> FrameStatus: ...
def ldpc_encode(type: LdpcSubframe, msg: NDArray[np.uint8]) -> NDArray[np.uint8]: ...
def ldpc_decode(
    type: LdpcSubframe,
    symbols: NDArray[np.float32],
    iterations: int,
    workspace: LdpcWorkspace | None = None,
) -> tuple[NDArray[np.uint8], LdpcStatus]: ...

# ── Frame building ───────────────────────────────────────────────────────────

def frame_build_partial(fid: int, toi: int) -> FrameSymbols: ...
def frame_build(
    fid: int,
    toi: int,
    sb2_data: Sb2Payload,
    sb3_data: Sb3Payload,
    sb4_data: Sb4Payload,
) -> FrameSymbols: ...
def subframe_build_sb2(data: Sb2Payload) -> NDArray[np.uint8]: ...
def subframe_build_sb3(data: Sb3Payload) -> NDArray[np.uint8]: ...
def subframe_build_sb4(data: Sb4Payload) -> NDArray[np.uint8]: ...

class LdpcEncodeWorkspace:
    def __init__(self) -> None: ...

# ── Constants ────────────────────────────────────────────────────────────────

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

# ── Sync detection ───────────────────────────────────────────────────────────

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
    rx_symbols: SoftSymbols,
    threshold: float = 0.0,
) -> tuple[SyncResult, SyncDetectorStatus]: ...

# ── Frame decode ─────────────────────────────────────────────────────────────

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
    rx_symbols: SoftSymbols,
    ldpc_iterations: int = 50,
    sync_threshold: float = 0.0,
) -> tuple[
    FrameDecodeResult,
    Sb2Payload,
    Sb3Payload,
    Sb4Payload,
    FrameDecodeStatus,
]: ...

# ── Signal generation ────────────────────────────────────────────────────────

class SignalGenStatus(IntEnum):
    OK = 0x5A
    INVALID_PRN = 0xA5
    INVALID_INPUT = 0x33
    MODULATION_FAILED = 0xCC
    MUX_FAILED = 0x99
    FAULT_DETECTED = 0x66

def generate_iq_epoch_native(
    prn_id: int,
    epoch_idx: int,
    data_symbol: int = 1,
) -> IqPairNative: ...
def generate_iq_epoch_float(
    prn_id: int,
    epoch_idx: int,
    data_symbol: int = 1,
    sample_rate: float = ...,
) -> IqPairFloat: ...
def generate_iq_frame_native(
    prn_id: int,
    frame_symbols: FrameSymbols,
) -> IqPairNative: ...
def generate_iq_frame_float(
    prn_id: int,
    frame_symbols: FrameSymbols,
    sample_rate: float,
) -> IqPairFloat: ...

IQ_PAIRS_PER_EPOCH: int
IQ_PAIRS_PER_FRAME: int

AFS_I_CODE_LENGTH: int
GOLD_CHIP_LENGTH: int
WEIL_10230_CHIP_LENGTH: int
WEIL_1500_CHIP_LENGTH: int
PRN_COUNT: int
CHIP_RATE_HZ: float

# Signal parameters (signal_parameters.csv)
CENTER_FREQUENCY_HZ: float
AFS_I_CHIP_RATE_HZ: float
AFS_Q_CHIP_RATE_HZ: float

# BCH metadata (encoding_parameters.csv)
BCH_GENERATOR_POLY: int
BCH_INPUT_BITS: int

# LDPC metadata (encoding_parameters.csv)
LDPC_BG2_ROWS: int
LDPC_BG2_COLS: int
LDPC_BG2_KB: int
LDPC_SF2_ZC: int
LDPC_SF2_ILS: int
LDPC_SF34_ZC: int
LDPC_SF34_ILS: int

# Time system (time_system_parameters.csv)
TOI_COUNT: int

# SB2 field layout (sb2_bit_layout.csv)
SB2_HEALTH_OFFSET: int
SB2_HEALTH_BITS: int
SB2_CED_OFFSET: int
SB2_CED_BITS: int
SB2_TIMECONV_OFFSET: int
SB2_TIMECONV_BITS: int
SB2_SPARE_OFFSET: int
SB2_SPARE_BITS: int

# File format magic numbers (file_format_headers.csv)
MAGIC_IQSG: int
MAGIC_IQS2: int
MAGIC_LSIS: int
MAGIC_FRAM: int
IQ_HEADER_BYTES: int
LSIS_HEADER_BYTES: int
FRAM_HEADER_BYTES: int

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

def parse_sb2(sb2_data: Sb2Payload) -> Sb2ParseResult: ...
def parse_sb3(sb3_data: Sb3Payload) -> Sb3ParseResult: ...
def parse_sb4(sb4_data: Sb4Payload) -> Sb4ParseResult: ...
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

# Gateway 7: SISE (Signal-In-Space Error)

class SiseStatus(IntEnum):
    OK = 0x5A
    INVALID_INPUT = 0xA5
    EMPTY_DATA = 0x33
    FAULT_DETECTED = 0x99

class SiseVec:
    x: float
    y: float
    z: float
    ct: float
    def __init__(
        self, x: float = 0.0, y: float = 0.0, z: float = 0.0, ct: float = 0.0
    ) -> None: ...

class SiseResult:
    status: SiseStatus
    position_error_m: float
    velocity_error_mps: float

class SisePercentileResult:
    status: SiseStatus
    position_percentile_m: float
    velocity_percentile_mps: float
    position_pass: bool
    velocity_pass: bool

def compute_sise(
    pos_true: SiseVec,
    pos_broadcast: SiseVec,
    vel_true: SiseVec,
    vel_broadcast: SiseVec,
) -> SiseResult: ...
def compute_sise_percentile(
    pos_errors_m: ErrorSeries,
    vel_errors_mps: ErrorSeries,
    percentile: float = 0.95,
) -> SisePercentileResult: ...

SISE_POSITION_LIMIT_M: float
SISE_VELOCITY_LIMIT_MPS: float
SPEED_OF_LIGHT_MPS: float
