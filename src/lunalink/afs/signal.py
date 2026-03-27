"""AFS signal generation, codec wrappers, and I/Q file export.

This module re-exports all symbols from the C++ ``_afs`` extension and adds
three Python-level functions that have logic beyond simple delegation:

- ``generate_iq_epoch`` — C++ epoch generation + optional Python resampling
- ``generate_iq_frame`` — C++ frame generation with optional C++ resampling
- ``write_iq_file`` — binary file I/O with LSISIQ header
"""

from __future__ import annotations

import os

import numpy as np
from numpy.typing import NDArray

# ── Direct re-exports from the C++ extension ────────────────────────────────
# These are the public API — no Python wrapper overhead.
# The `as X` form makes each symbol explicitly public (PEP 484 / pyright).
# Constants
from lunalink.afs._afs import AFS_I_CHIP_RATE_HZ as AFS_I_CHIP_RATE_HZ
from lunalink.afs._afs import AFS_I_CODE_LENGTH as AFS_I_CODE_LENGTH
from lunalink.afs._afs import AFS_Q_CHIP_RATE_HZ as AFS_Q_CHIP_RATE_HZ
from lunalink.afs._afs import BCH_CODEWORD_LENGTH as BCH_CODEWORD_LENGTH
from lunalink.afs._afs import BCH_GENERATOR_POLY as BCH_GENERATOR_POLY
from lunalink.afs._afs import BCH_INPUT_BITS as BCH_INPUT_BITS
from lunalink.afs._afs import BLOCK_INTERVAL_DURATION as BLOCK_INTERVAL_DURATION
from lunalink.afs._afs import CENTER_FREQUENCY_HZ as CENTER_FREQUENCY_HZ
from lunalink.afs._afs import CHIP_RATE_HZ as CHIP_RATE_HZ
from lunalink.afs._afs import CODE_PERIOD_S as CODE_PERIOD_S
from lunalink.afs._afs import DEFAULT_SYNC_THRESHOLD as DEFAULT_SYNC_THRESHOLD
from lunalink.afs._afs import DOPPLER_BIN_SPACING_HZ as DOPPLER_BIN_SPACING_HZ
from lunalink.afs._afs import EPOCHS_PER_FRAME as EPOCHS_PER_FRAME
from lunalink.afs._afs import FFT_SIZE as FFT_SIZE
from lunalink.afs._afs import FRAM_HEADER_BYTES as FRAM_HEADER_BYTES
from lunalink.afs._afs import FRAME_DURATION_S as FRAME_DURATION_S
from lunalink.afs._afs import FRAME_DURATION_SEC as FRAME_DURATION_SEC
from lunalink.afs._afs import FRAME_LENGTH as FRAME_LENGTH
from lunalink.afs._afs import GOLD_CHIP_LENGTH as GOLD_CHIP_LENGTH
from lunalink.afs._afs import INTERIM_ASSIGNMENT_MAX_PRN as INTERIM_ASSIGNMENT_MAX_PRN
from lunalink.afs._afs import INTERLEAVER_COLS as INTERLEAVER_COLS
from lunalink.afs._afs import INTERLEAVER_ROWS as INTERLEAVER_ROWS
from lunalink.afs._afs import INTERLEAVER_SIZE as INTERLEAVER_SIZE
from lunalink.afs._afs import IQ_HEADER_BYTES as IQ_HEADER_BYTES
from lunalink.afs._afs import IQ_PAIRS_PER_EPOCH as IQ_PAIRS_PER_EPOCH
from lunalink.afs._afs import IQ_PAIRS_PER_FRAME as IQ_PAIRS_PER_FRAME
from lunalink.afs._afs import IQ_SAMPLES_PER_EPOCH as IQ_SAMPLES_PER_EPOCH
from lunalink.afs._afs import IQ_UPSAMPLE_FACTOR as IQ_UPSAMPLE_FACTOR
from lunalink.afs._afs import ITOW_BITS as ITOW_BITS
from lunalink.afs._afs import ITOW_MAX as ITOW_MAX
from lunalink.afs._afs import LDPC_BG2_COLS as LDPC_BG2_COLS
from lunalink.afs._afs import LDPC_BG2_KB as LDPC_BG2_KB
from lunalink.afs._afs import LDPC_BG2_ROWS as LDPC_BG2_ROWS
from lunalink.afs._afs import LDPC_SF2_ILS as LDPC_SF2_ILS
from lunalink.afs._afs import LDPC_SF2_ZC as LDPC_SF2_ZC
from lunalink.afs._afs import LDPC_SF34_ILS as LDPC_SF34_ILS
from lunalink.afs._afs import LDPC_SF34_ZC as LDPC_SF34_ZC
from lunalink.afs._afs import LSIS_HEADER_BYTES as LSIS_HEADER_BYTES
from lunalink.afs._afs import MAGIC_FRAM as MAGIC_FRAM
from lunalink.afs._afs import MAGIC_IQS2 as MAGIC_IQS2
from lunalink.afs._afs import MAGIC_IQSG as MAGIC_IQSG
from lunalink.afs._afs import MAGIC_LSIS as MAGIC_LSIS
from lunalink.afs._afs import MAX_CHANNELS as MAX_CHANNELS
from lunalink.afs._afs import PAYLOAD_LENGTH as PAYLOAD_LENGTH
from lunalink.afs._afs import PRN_COUNT as PRN_COUNT
from lunalink.afs._afs import SAMPLE_RATE_HZ as SAMPLE_RATE_HZ
from lunalink.afs._afs import SB1_LENGTH as SB1_LENGTH
from lunalink.afs._afs import SB2_CED_BITS as SB2_CED_BITS
from lunalink.afs._afs import SB2_CED_OFFSET as SB2_CED_OFFSET
from lunalink.afs._afs import SB2_DATA_BITS as SB2_DATA_BITS
from lunalink.afs._afs import SB2_HEALTH_BITS as SB2_HEALTH_BITS
from lunalink.afs._afs import SB2_HEALTH_OFFSET as SB2_HEALTH_OFFSET
from lunalink.afs._afs import SB2_LENGTH as SB2_LENGTH
from lunalink.afs._afs import SB2_MSG_BITS as SB2_MSG_BITS
from lunalink.afs._afs import SB2_PAYLOAD_BITS as SB2_PAYLOAD_BITS
from lunalink.afs._afs import SB2_SPARE_BITS as SB2_SPARE_BITS
from lunalink.afs._afs import SB2_SPARE_OFFSET as SB2_SPARE_OFFSET
from lunalink.afs._afs import SB2_TIMECONV_BITS as SB2_TIMECONV_BITS
from lunalink.afs._afs import SB2_TIMECONV_OFFSET as SB2_TIMECONV_OFFSET
from lunalink.afs._afs import SB3_DATA_BITS as SB3_DATA_BITS
from lunalink.afs._afs import SB3_LENGTH as SB3_LENGTH
from lunalink.afs._afs import SB3_MSG_BITS as SB3_MSG_BITS
from lunalink.afs._afs import SB3_PAYLOAD_BITS as SB3_PAYLOAD_BITS
from lunalink.afs._afs import SB4_DATA_BITS as SB4_DATA_BITS
from lunalink.afs._afs import SB4_LENGTH as SB4_LENGTH
from lunalink.afs._afs import SB4_MSG_BITS as SB4_MSG_BITS
from lunalink.afs._afs import SB4_PAYLOAD_BITS as SB4_PAYLOAD_BITS
from lunalink.afs._afs import SEC_WEEK as SEC_WEEK
from lunalink.afs._afs import SECONDARY_CODE_COUNT as SECONDARY_CODE_COUNT
from lunalink.afs._afs import SECONDARY_CODE_LENGTH as SECONDARY_CODE_LENGTH
from lunalink.afs._afs import SISE_POSITION_LIMIT_M as SISE_POSITION_LIMIT_M
from lunalink.afs._afs import SISE_VELOCITY_LIMIT_MPS as SISE_VELOCITY_LIMIT_MPS
from lunalink.afs._afs import SPEED_OF_LIGHT_MPS as SPEED_OF_LIGHT_MPS
from lunalink.afs._afs import SUBFRAME_TYPE_BITS as SUBFRAME_TYPE_BITS
from lunalink.afs._afs import SYMBOL_RATE as SYMBOL_RATE
from lunalink.afs._afs import SYNC_LENGTH as SYNC_LENGTH
from lunalink.afs._afs import TERTIARY_CODE_LENGTH as TERTIARY_CODE_LENGTH
from lunalink.afs._afs import TOI_COUNT as TOI_COUNT
from lunalink.afs._afs import TOT_HEADER_BITS as TOT_HEADER_BITS
from lunalink.afs._afs import TOT_TOI_MAX as TOT_TOI_MAX
from lunalink.afs._afs import WEIL_1500_CHIP_LENGTH as WEIL_1500_CHIP_LENGTH
from lunalink.afs._afs import WEIL_10230_CHIP_LENGTH as WEIL_10230_CHIP_LENGTH
from lunalink.afs._afs import WN_BITS as WN_BITS
from lunalink.afs._afs import WN_MAX as WN_MAX

# Types and enums
from lunalink.afs._afs import AcquisitionResult as AcquisitionResult
from lunalink.afs._afs import BchResult as BchResult
from lunalink.afs._afs import BchStatus as BchStatus
from lunalink.afs._afs import ChannelState as ChannelState
from lunalink.afs._afs import Correlator as Correlator
from lunalink.afs._afs import CorrelatorStatus as CorrelatorStatus
from lunalink.afs._afs import CrcStatus as CrcStatus
from lunalink.afs._afs import Fid as Fid
from lunalink.afs._afs import FrameDecodeResult as FrameDecodeResult
from lunalink.afs._afs import FrameDecodeStatus as FrameDecodeStatus
from lunalink.afs._afs import FrameStatus as FrameStatus
from lunalink.afs._afs import InterleaverStatus as InterleaverStatus
from lunalink.afs._afs import IqMuxStatus as IqMuxStatus
from lunalink.afs._afs import LdpcEncodeWorkspace as LdpcEncodeWorkspace
from lunalink.afs._afs import LdpcStatus as LdpcStatus
from lunalink.afs._afs import LdpcSubframe as LdpcSubframe
from lunalink.afs._afs import LdpcWorkspace as LdpcWorkspace
from lunalink.afs._afs import LegendreStatus as LegendreStatus
from lunalink.afs._afs import MatchedCodeStatus as MatchedCodeStatus
from lunalink.afs._afs import MessageParseStatus as MessageParseStatus
from lunalink.afs._afs import MessageType as MessageType
from lunalink.afs._afs import ModulationStatus as ModulationStatus
from lunalink.afs._afs import PrnId as PrnId
from lunalink.afs._afs import PrnStatus as PrnStatus
from lunalink.afs._afs import Sb2ParseResult as Sb2ParseResult
from lunalink.afs._afs import Sb3ParseResult as Sb3ParseResult
from lunalink.afs._afs import Sb4ParseResult as Sb4ParseResult
from lunalink.afs._afs import SignalGenStatus as SignalGenStatus
from lunalink.afs._afs import SisePercentileResult as SisePercentileResult
from lunalink.afs._afs import SiseResult as SiseResult
from lunalink.afs._afs import SiseStatus as SiseStatus
from lunalink.afs._afs import SiseVec as SiseVec
from lunalink.afs._afs import SubframeDecodeResult as SubframeDecodeResult
from lunalink.afs._afs import SyncDetectorStatus as SyncDetectorStatus
from lunalink.afs._afs import SyncResult as SyncResult
from lunalink.afs._afs import Toi as Toi
from lunalink.afs._afs import TotResult as TotResult
from lunalink.afs._afs import TotStatus as TotStatus
from lunalink.afs._afs import TrackingState as TrackingState
from lunalink.afs._afs import UniformPrior as UniformPrior

# Functions — direct re-exports (no Python wrapper overhead)
from lunalink.afs._afs import bch_codebook_checksum as bch_codebook_checksum
from lunalink.afs._afs import bch_decode as bch_decode
from lunalink.afs._afs import bch_decode_soft as bch_decode_soft
from lunalink.afs._afs import bch_encode as bch_encode
from lunalink.afs._afs import block_deinterleave as block_deinterleave
from lunalink.afs._afs import block_deinterleave_soft as block_deinterleave_soft
from lunalink.afs._afs import block_interleave as block_interleave
from lunalink.afs._afs import commit_frame as commit_frame
from lunalink.afs._afs import compute_sise as compute_sise
from lunalink.afs._afs import compute_sise_percentile as compute_sise_percentile
from lunalink.afs._afs import compute_tot as compute_tot
from lunalink.afs._afs import crc24_calculate as crc24_calculate
from lunalink.afs._afs import crc24_verify as crc24_verify
from lunalink.afs._afs import default_matched_assignment as default_matched_assignment
from lunalink.afs._afs import frame_build as frame_build
from lunalink.afs._afs import frame_build_partial as frame_build_partial
from lunalink.afs._afs import frame_decode as frame_decode
from lunalink.afs._afs import generate_iq_epoch_float as generate_iq_epoch_float
from lunalink.afs._afs import generate_iq_epoch_native as generate_iq_epoch_native
from lunalink.afs._afs import generate_iq_frame_float as generate_iq_frame_float
from lunalink.afs._afs import generate_iq_frame_native as generate_iq_frame_native
from lunalink.afs._afs import ldpc_decode as ldpc_decode
from lunalink.afs._afs import ldpc_encode as ldpc_encode
from lunalink.afs._afs import legendre_1499_checksum as legendre_1499_checksum
from lunalink.afs._afs import legendre_10223_checksum as legendre_10223_checksum
from lunalink.afs._afs import legendre_cache_init as legendre_cache_init
from lunalink.afs._afs import matched_code_epoch as matched_code_epoch
from lunalink.afs._afs import (
    matched_code_epoch_assigned as matched_code_epoch_assigned,
)
from lunalink.afs._afs import modulate_i as modulate_i
from lunalink.afs._afs import modulate_q as modulate_q
from lunalink.afs._afs import multiplex_iq as multiplex_iq
from lunalink.afs._afs import parse_sb2 as parse_sb2
from lunalink.afs._afs import parse_sb3 as parse_sb3
from lunalink.afs._afs import parse_sb4 as parse_sb4
from lunalink.afs._afs import prn_code as prn_code
from lunalink.afs._afs import subframe_build_sb2 as subframe_build_sb2
from lunalink.afs._afs import subframe_build_sb3 as subframe_build_sb3
from lunalink.afs._afs import subframe_build_sb4 as subframe_build_sb4
from lunalink.afs._afs import sync_detect as sync_detect
from lunalink.afs._afs import weil1500_code as weil1500_code
from lunalink.afs._afs import weil10230_code as weil10230_code

# ── Public aliases (canonical names, matching spec_tables CSVs) ──────────────
# The C++ binding exports ``AFS_I_CHIP_RATE_HZ`` / ``AFS_Q_CHIP_RATE_HZ``.
# The public Python API uses the shorter spec names without the ``_HZ`` suffix.

#: Native AFS-I chip rate (Hz).  Canonical name — prefer over ``AFS_I_CHIP_RATE_HZ``.
AFS_I_CHIP_RATE = AFS_I_CHIP_RATE_HZ
#: Native AFS-Q chip rate (Hz).  Canonical name — prefer over ``AFS_Q_CHIP_RATE_HZ``.
AFS_Q_CHIP_RATE = AFS_Q_CHIP_RATE_HZ

# ── Python-level functions (add logic beyond simple delegation) ──────────────


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
    rate = float(sample_rate) if sample_rate is not None else float(AFS_Q_CHIP_RATE)
    return generate_iq_epoch_float(prn_id, epoch_idx, data_symbol, rate)


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

    # C++ generates all 6000 epochs as float32 in one call.
    # Handles both native rate and resampled — no Python-side copy or cast.
    rate = float(sample_rate) if sample_rate is not None else float(AFS_Q_CHIP_RATE)
    return generate_iq_frame_float(prn_id, frame_symbols, rate)


def write_iq_file(
    path: str | os.PathLike[str],
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
    path : str or os.PathLike
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
    if len(header) != 128:
        raise RuntimeError(f"IQ header must be 128 bytes, got {len(header)}")

    interleaved = iq_data.astype(np.float32).ravel()
    with open(path, "wb") as f:
        f.write(header)
        f.write(interleaved.tobytes())
