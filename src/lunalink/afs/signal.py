"""AFS signal generation: PRN codes and BPSK modulation."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._afs import AFS_I_CODE_LENGTH as AFS_I_CODE_LENGTH
from lunalink.afs._afs import BCH_CODEWORD_LENGTH as BCH_CODEWORD_LENGTH
from lunalink.afs._afs import CHIP_RATE_HZ as CHIP_RATE_HZ
from lunalink.afs._afs import CODE_PERIOD_S as CODE_PERIOD_S
from lunalink.afs._afs import DEFAULT_SYNC_THRESHOLD as DEFAULT_SYNC_THRESHOLD
from lunalink.afs._afs import DOPPLER_BIN_SPACING_HZ as DOPPLER_BIN_SPACING_HZ
from lunalink.afs._afs import EPOCHS_PER_FRAME as EPOCHS_PER_FRAME
from lunalink.afs._afs import FFT_SIZE as FFT_SIZE
from lunalink.afs._afs import FRAME_DURATION_S as FRAME_DURATION_S
from lunalink.afs._afs import FRAME_LENGTH as FRAME_LENGTH
from lunalink.afs._afs import GOLD_CHIP_LENGTH as GOLD_CHIP_LENGTH
from lunalink.afs._afs import INTERIM_ASSIGNMENT_MAX_PRN as INTERIM_ASSIGNMENT_MAX_PRN
from lunalink.afs._afs import IQ_SAMPLES_PER_EPOCH as IQ_SAMPLES_PER_EPOCH
from lunalink.afs._afs import IQ_UPSAMPLE_FACTOR as IQ_UPSAMPLE_FACTOR
from lunalink.afs._afs import MAX_CHANNELS as MAX_CHANNELS
from lunalink.afs._afs import PAYLOAD_LENGTH as PAYLOAD_LENGTH
from lunalink.afs._afs import PRN_COUNT as PRN_COUNT
from lunalink.afs._afs import SAMPLE_RATE_HZ as SAMPLE_RATE_HZ
from lunalink.afs._afs import SB1_LENGTH as SB1_LENGTH
from lunalink.afs._afs import SB2_DATA_BITS as SB2_DATA_BITS
from lunalink.afs._afs import SB2_LENGTH as SB2_LENGTH
from lunalink.afs._afs import SB2_MSG_BITS as SB2_MSG_BITS
from lunalink.afs._afs import SB3_DATA_BITS as SB3_DATA_BITS
from lunalink.afs._afs import SB3_LENGTH as SB3_LENGTH
from lunalink.afs._afs import SB3_MSG_BITS as SB3_MSG_BITS
from lunalink.afs._afs import SB4_DATA_BITS as SB4_DATA_BITS
from lunalink.afs._afs import SB4_LENGTH as SB4_LENGTH
from lunalink.afs._afs import SB4_MSG_BITS as SB4_MSG_BITS
from lunalink.afs._afs import SECONDARY_CODE_COUNT as SECONDARY_CODE_COUNT
from lunalink.afs._afs import SECONDARY_CODE_LENGTH as SECONDARY_CODE_LENGTH
from lunalink.afs._afs import SYMBOL_RATE as SYMBOL_RATE
from lunalink.afs._afs import SYNC_LENGTH as SYNC_LENGTH
from lunalink.afs._afs import TERTIARY_CODE_LENGTH as TERTIARY_CODE_LENGTH
from lunalink.afs._afs import WEIL_1500_CHIP_LENGTH as WEIL_1500_CHIP_LENGTH
from lunalink.afs._afs import WEIL_10230_CHIP_LENGTH as WEIL_10230_CHIP_LENGTH
from lunalink.afs._afs import AcquisitionResult as AcquisitionResult
from lunalink.afs._afs import BchResult as BchResult
from lunalink.afs._afs import BchStatus as BchStatus
from lunalink.afs._afs import ChannelState as ChannelState
from lunalink.afs._afs import Correlator as Correlator
from lunalink.afs._afs import CorrelatorStatus as CorrelatorStatus
from lunalink.afs._afs import Fid as Fid
from lunalink.afs._afs import FrameDecodeResult as FrameDecodeResult
from lunalink.afs._afs import FrameDecodeStatus as FrameDecodeStatus
from lunalink.afs._afs import FrameStatus as FrameStatus
from lunalink.afs._afs import IqMuxStatus as IqMuxStatus
from lunalink.afs._afs import LdpcEncodeWorkspace as LdpcEncodeWorkspace
from lunalink.afs._afs import LdpcStatus as LdpcStatus
from lunalink.afs._afs import LdpcSubframe as LdpcSubframe
from lunalink.afs._afs import LdpcWorkspace as LdpcWorkspace
from lunalink.afs._afs import LegendreStatus as LegendreStatus
from lunalink.afs._afs import MatchedCodeStatus as MatchedCodeStatus
from lunalink.afs._afs import ModulationStatus as ModulationStatus
from lunalink.afs._afs import PrnId as PrnId
from lunalink.afs._afs import PrnStatus as PrnStatus
from lunalink.afs._afs import SubframeDecodeResult as SubframeDecodeResult
from lunalink.afs._afs import SyncDetectorStatus as SyncDetectorStatus
from lunalink.afs._afs import SyncResult as SyncResult
from lunalink.afs._afs import Toi as Toi
from lunalink.afs._afs import TrackingState as TrackingState
from lunalink.afs._afs import UniformPrior as UniformPrior
from lunalink.afs._afs import bch_codebook_checksum as _bch_codebook_checksum
from lunalink.afs._afs import bch_decode as _bch_decode
from lunalink.afs._afs import bch_encode as _bch_encode
from lunalink.afs._afs import block_deinterleave_soft as _block_deinterleave_soft
from lunalink.afs._afs import commit_frame as _commit_frame
from lunalink.afs._afs import default_matched_assignment as _default_matched_assignment
from lunalink.afs._afs import frame_build as _frame_build
from lunalink.afs._afs import frame_build_partial as _frame_build_partial
from lunalink.afs._afs import frame_decode as _frame_decode
from lunalink.afs._afs import ldpc_decode as _ldpc_decode
from lunalink.afs._afs import ldpc_encode as _ldpc_encode
from lunalink.afs._afs import legendre_1499_checksum as _legendre_1499_checksum
from lunalink.afs._afs import legendre_10223_checksum as _legendre_10223_checksum
from lunalink.afs._afs import legendre_cache_init as _legendre_cache_init
from lunalink.afs._afs import matched_code_epoch as _matched_code_epoch
from lunalink.afs._afs import (
    matched_code_epoch_assigned as _matched_code_epoch_assigned,
)
from lunalink.afs._afs import modulate_i as _modulate_i
from lunalink.afs._afs import modulate_q as _modulate_q
from lunalink.afs._afs import multiplex_iq as _multiplex_iq
from lunalink.afs._afs import prn_code as _prn_code
from lunalink.afs._afs import subframe_build_sb2 as _subframe_build_sb2
from lunalink.afs._afs import subframe_build_sb3 as _subframe_build_sb3
from lunalink.afs._afs import subframe_build_sb4 as _subframe_build_sb4
from lunalink.afs._afs import sync_detect as _sync_detect
from lunalink.afs._afs import weil1500_code as _weil1500_code
from lunalink.afs._afs import weil10230_code as _weil10230_code

__all__ = [
    "bch_encode",
    "bch_decode",
    "bch_codebook_checksum",
    "ldpc_encode",
    "ldpc_decode",
    "LdpcWorkspace",
    "LdpcEncodeWorkspace",
    "LdpcSubframe",
    "LdpcStatus",
    "BchResult",
    "BchStatus",
    "Fid",
    "Toi",
    "frame_build",
    "frame_build_partial",
    "subframe_build_sb2",
    "subframe_build_sb3",
    "subframe_build_sb4",
    "modulate_i",
    "modulate_q",
    "multiplex_iq",
    "prn_code",
    "matched_code_epoch",
    "matched_code_epoch_assigned",
    "weil10230_code",
    "weil1500_code",
    "BCH_CODEWORD_LENGTH",
    "FRAME_LENGTH",
    "SYNC_LENGTH",
    "SB1_LENGTH",
    "PAYLOAD_LENGTH",
    "SYMBOL_RATE",
    "FRAME_DURATION_S",
    "EPOCHS_PER_FRAME",
    "SECONDARY_CODE_LENGTH",
    "SECONDARY_CODE_COUNT",
    "INTERIM_ASSIGNMENT_MAX_PRN",
    "TERTIARY_CODE_LENGTH",
    "IQ_UPSAMPLE_FACTOR",
    "IQ_SAMPLES_PER_EPOCH",
    "FrameStatus",
    "SB2_DATA_BITS",
    "SB2_MSG_BITS",
    "SB2_LENGTH",
    "SB3_DATA_BITS",
    "SB3_MSG_BITS",
    "SB3_LENGTH",
    "SB4_DATA_BITS",
    "SB4_MSG_BITS",
    "SB4_LENGTH",
    "DEFAULT_SYNC_THRESHOLD",
    "SyncDetectorStatus",
    "SyncResult",
    "sync_detect",
    "FrameDecodeStatus",
    "FrameDecodeResult",
    "SubframeDecodeResult",
    "frame_decode",
    "CorrelatorStatus",
    "ChannelState",
    "AcquisitionResult",
    "TrackingState",
    "UniformPrior",
    "Correlator",
    "AFS_I_CODE_LENGTH",
    "FFT_SIZE",
    "SAMPLE_RATE_HZ",
    "CODE_PERIOD_S",
    "MAX_CHANNELS",
    "GOLD_CHIP_LENGTH",
    "WEIL_10230_CHIP_LENGTH",
    "WEIL_1500_CHIP_LENGTH",
    "PRN_COUNT",
    "CHIP_RATE_HZ",
    "DOPPLER_BIN_SPACING_HZ",
    "PrnId",
    "PrnStatus",
    "MatchedCodeStatus",
    "ModulationStatus",
    "IqMuxStatus",
    "LegendreStatus",
    "block_deinterleave_soft",
    "commit_frame",
    "default_matched_assignment",
    "legendre_cache_init",
    "legendre_10223_checksum",
    "legendre_1499_checksum",
    "generate_iq_epoch",
    "generate_iq_frame",
    "write_iq_file",
    "AFS_I_CHIP_RATE",
    "AFS_Q_CHIP_RATE",
]


def prn_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Gold-2046 chip sequence for PRN prn_id (1-indexed)."""
    return _prn_code(prn_id)


def weil10230_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-10230 chip sequence for PRN prn_id (1-indexed)."""
    return _weil10230_code(prn_id)


def weil1500_code(prn_id: int) -> NDArray[np.uint8]:
    """Return the Weil-1500 chip sequence for PRN prn_id (1-indexed)."""
    return _weil1500_code(prn_id)


def modulate_i(prn: NDArray[np.uint8], data_symbol: int) -> NDArray[np.int8]:
    """Modulate a chip sequence with a BPSK data symbol (AFS-I channel)."""
    return _modulate_i(prn, data_symbol)


def modulate_q(chips: NDArray[np.uint8]) -> NDArray[np.int8]:
    """Modulate a chip sequence for AFS-Q pilot channel (BPSK(5), no data)."""
    return _modulate_q(chips)


def multiplex_iq(
    i_samples: NDArray[np.int8], q_samples: NDArray[np.int8]
) -> NDArray[np.int16]:
    """Multiplex AFS-I and AFS-Q into baseband IQ at 5.115 MSPS."""
    return _multiplex_iq(i_samples, q_samples)


def matched_code_epoch(prn_id: int, epoch_idx: int) -> NDArray[np.uint8]:
    """Return one primary epoch (10230 chips) of the matched AFS-Q code.

    Uses the default interim mapping for node IDs 1-12.

    Parameters
    ----------
    prn_id : int
        Node ID (1-12).
    epoch_idx : int
        Index of the 2ms epoch within the 12s frame (0-5999).

    Returns
    -------
    numpy.ndarray
        The 10230 binary chips.
    """
    return _matched_code_epoch(prn_id, epoch_idx)


def matched_code_epoch_assigned(
    primary_prn: int,
    secondary_code_idx: int,
    tertiary_prn: int,
    tertiary_phase_offset: int,
    epoch_idx: int,
) -> NDArray[np.uint8]:
    """Return one primary epoch using explicit AFS-Q code assignments."""
    return _matched_code_epoch_assigned(
        primary_prn, secondary_code_idx, tertiary_prn, tertiary_phase_offset, epoch_idx
    )


def bch_encode(fid: int, toi: int) -> NDArray[np.uint8]:
    """Encode SB1 (FID + TOI) using BCH(51,8) (§2.4.2.1)."""
    return _bch_encode(fid, toi)


def bch_decode(codeword: NDArray[np.uint8]) -> BchResult:
    """Decode SB1 header using a Maximum Likelihood decoder."""
    return _bch_decode(codeword)


def bch_codebook_checksum() -> int:
    """Return the XOR-checksum of the static codebook for integrity verification."""
    return _bch_codebook_checksum()


def ldpc_encode(type: LdpcSubframe, msg: NDArray[np.uint8]) -> NDArray[np.uint8]:
    """Encode subframe payload using LDPC(1/2) (§2.4)."""
    return _ldpc_encode(type, msg)


def ldpc_decode(
    type: LdpcSubframe,
    symbols: NDArray[np.float32],
    iterations: int = 50,
    workspace: LdpcWorkspace | None = None,
) -> tuple[NDArray[np.uint8], LdpcStatus]:
    """Decode LDPC codeword using fixed-point Layered Min-Sum decoder.

    Returns (decoded_bits, status) where status is OK or NOT_CONVERGED.
    """
    return _ldpc_decode(type, symbols, iterations, workspace)


def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]:
    """Build a partial AFS navigation frame (§2.4)."""
    return _frame_build_partial(fid, toi)


def frame_build(
    fid: int,
    toi: int,
    sb2_data: NDArray[np.uint8],
    sb3_data: NDArray[np.uint8],
    sb4_data: NDArray[np.uint8],
) -> NDArray[np.uint8]:
    """Build a complete AFS navigation frame (§2.4)."""
    return _frame_build(fid, toi, sb2_data, sb3_data, sb4_data)


def subframe_build_sb2(data: NDArray[np.uint8]) -> NDArray[np.uint8]:
    """Build Subframe 2: CRC-24 + LDPC SF2 encode (§2.4)."""
    return _subframe_build_sb2(data)


def subframe_build_sb3(data: NDArray[np.uint8]) -> NDArray[np.uint8]:
    """Build Subframe 3: CRC-24 + LDPC SF3 encode (§2.4)."""
    return _subframe_build_sb3(data)


def subframe_build_sb4(data: NDArray[np.uint8]) -> NDArray[np.uint8]:
    """Build Subframe 4: CRC-24 + LDPC SF4 encode (§2.4)."""
    return _subframe_build_sb4(data)


def sync_detect(
    rx_symbols: NDArray[np.float32],
    threshold: float = 0.0,
) -> tuple[SyncResult, SyncDetectorStatus]:
    """Detect the 68-symbol sync pattern in a stream of soft symbols.

    Parameters
    ----------
    rx_symbols : array of float32
        Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1).
    threshold : float, optional
        Detection threshold. 0 uses the default (50.0).

    Returns
    -------
    tuple[SyncResult, SyncDetectorStatus]
        Detection result and status code.
    """
    return _sync_detect(rx_symbols, threshold)


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
]:
    """Decode a complete AFS navigation frame from soft-decision symbols.

    Parameters
    ----------
    rx_symbols : array of float32
        Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1).
        Must contain at least kFrameLength (6000) symbols after the sync offset.
    ldpc_iterations : int, optional
        LDPC BP iteration count (default: 50).
    sync_threshold : float, optional
        Sync detection threshold (default: 50.0). Set to 0 to use default.

    Returns
    -------
    tuple[FrameDecodeResult, ndarray, ndarray, ndarray, FrameDecodeStatus]
        (result, sb2_data, sb3_data, sb4_data, status)
    """
    return _frame_decode(rx_symbols, ldpc_iterations, sync_threshold)


def block_deinterleave_soft(
    data: NDArray[np.float32],
) -> NDArray[np.float32]:
    """Soft-decision 60x98 block de-interleaver for LLR values."""
    return _block_deinterleave_soft(data)


def commit_frame(frame: NDArray[np.uint8]) -> FrameStatus:
    """CRC-24 post-generation safety gate (SW-SAF-01)."""
    return _commit_frame(frame)


def default_matched_assignment(
    prn_id: int,
) -> tuple[int, int, int, int]:
    """Return the default interim matched-code assignment for a PRN ID.

    Returns (primary_prn, secondary_code_idx, tertiary_prn, tertiary_phase_offset).
    """
    return _default_matched_assignment(prn_id)


def legendre_cache_init() -> LegendreStatus:
    """Initialise the Legendre sequence cache and verify golden CRCs."""
    return _legendre_cache_init()


def legendre_10223_checksum() -> int:
    """Return CRC-32 checksum of the cached Legendre sequence for prime 10223."""
    return _legendre_10223_checksum()


def legendre_1499_checksum() -> int:
    """Return CRC-32 checksum of the cached Legendre sequence for prime 1499."""
    return _legendre_1499_checksum()


# ---------------------------------------------------------------------------
# I/Q signal generation and file export
# ---------------------------------------------------------------------------

#: Native AFS-I chip rate (Hz).
AFS_I_CHIP_RATE = 1_023_000
#: Native AFS-Q chip rate (Hz).
AFS_Q_CHIP_RATE = 5_115_000


def generate_iq_epoch(
    prn_id: int,
    epoch_idx: int,
    data_symbol: int = 1,
    sample_rate: float | None = None,
) -> NDArray[np.float32]:
    """Generate one 2 ms epoch of baseband I/Q samples.

    Parameters
    ----------
    prn_id : int
        PRN ID (1-12 for interim codes).
    epoch_idx : int
        Epoch index within the 12 s frame (0-5999).
    data_symbol : int
        Navigation data bit in {-1, +1} for AFS-I.
    sample_rate : float, optional
        Output sample rate in Hz. ``None`` uses the native Q-channel rate
        (5.115 MHz, 1 sample per Q-chip = 10230 samples/epoch).
        Common choices: 10.23e6 (2x), 20.46e6 (4x).

    Returns
    -------
    NDArray[np.float32]
        Interleaved I/Q pairs with shape (N, 2) where N depends on sample_rate.
    """
    gold = prn_code(prn_id)
    matched = matched_code_epoch(prn_id, epoch_idx)

    i_mod = modulate_i(gold, data_symbol)
    q_mod = modulate_q(matched)
    iq = multiplex_iq(i_mod, q_mod)  # (10230, 2) int16

    iq_f32 = iq.astype(np.float32)

    if sample_rate is not None and sample_rate != AFS_Q_CHIP_RATE:
        native_rate = float(AFS_Q_CHIP_RATE)
        n_native = iq_f32.shape[0]
        n_out = int(round(n_native * sample_rate / native_rate))
        # Nearest-neighbour resampling (zero-order hold, round-half-up
        # for deterministic chip boundaries across implementations)
        indices = np.floor(np.linspace(0, n_native - 1, n_out) + 0.5).astype(int)
        iq_f32 = iq_f32[indices]

    return iq_f32


def generate_iq_frame(
    prn_id: int,
    frame_symbols: NDArray[np.uint8],
    sample_rate: float | None = None,
) -> NDArray[np.float32]:
    """Generate a full 12 s frame of baseband I/Q samples.

    Parameters
    ----------
    prn_id : int
        PRN ID (1-12 for interim codes).
    frame_symbols : NDArray[np.uint8]
        6000 binary frame symbols (from ``frame_build``).
    sample_rate : float, optional
        Output sample rate in Hz. ``None`` for native 5.115 MHz.

    Returns
    -------
    NDArray[np.float32]
        Interleaved I/Q pairs with shape (N, 2).
    """
    if len(frame_symbols) != FRAME_LENGTH:
        raise ValueError(f"Expected {FRAME_LENGTH} symbols, got {len(frame_symbols)}")

    epochs = []
    for epoch in range(EPOCHS_PER_FRAME):
        data_sym = 1 - 2 * int(frame_symbols[epoch])  # BPSK: 0→+1, 1→-1
        epochs.append(generate_iq_epoch(prn_id, epoch, data_sym, sample_rate))

    return np.concatenate(epochs, axis=0)


def write_iq_file(
    path: str,
    iq_data: NDArray[np.float32],
    sample_rate: float,
    prn_id: int,
    duration_s: float | None = None,
) -> None:
    """Write I/Q samples to a binary file with a 128-byte metadata header.

    Header format (per docs/shaping/test-vector-format.md):
        - Magic:       ``"LSISIQ"`` + 2 null bytes (8 bytes)
        - Version:     ``1`` (uint32 LE, 4 bytes)
        - Sample rate: ``float64`` LE (8 bytes)
        - Duration:    ``float64`` LE seconds (8 bytes)
        - PRN:         ``uint32`` LE (4 bytes)
        - Format:      ``"float32"`` (16 bytes, zero-padded)
        - Reserved:    80 bytes (zeros)

    Data: interleaved float32 [I0, Q0, I1, Q1, ...].

    Parameters
    ----------
    path : str
        Output file path.
    iq_data : NDArray[np.float32]
        I/Q data with shape (N, 2).
    sample_rate : float
        Sample rate in Hz.
    prn_id : int
        PRN identifier.
    duration_s : float, optional
        Signal duration in seconds. Computed from data length if not provided.
    """
    import io
    import struct

    if iq_data.ndim != 2 or iq_data.shape[1] != 2:
        raise ValueError("iq_data must have shape (N, 2)")

    n_samples = iq_data.shape[0]
    if duration_s is None:
        duration_s = n_samples / sample_rate

    # 128-byte header: Magic(8) + Version(4) + SampleRate(8) + Duration(8)
    #                  + PRN(4) + Format(16) + Reserved(80) = 128
    buf = io.BytesIO()
    buf.write(b"LSISIQ\x00\x00")  # 8
    buf.write(struct.pack("<I", 1))  # 4
    buf.write(struct.pack("<d", sample_rate))  # 8
    buf.write(struct.pack("<d", duration_s))  # 8
    buf.write(struct.pack("<I", prn_id))  # 4
    buf.write(b"float32" + b"\x00" * 9)  # 16
    buf.write(b"\x00" * (128 - buf.tell()))  # pad to 128
    header = buf.getvalue()
    assert len(header) == 128

    interleaved = iq_data.astype(np.float32).ravel()
    with open(path, "wb") as f:
        f.write(header)
        f.write(interleaved.tobytes())
