"""AFS signal generation: PRN codes and BPSK modulation."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from lunalink.afs._afs import BCH_CODEWORD_LENGTH as BCH_CODEWORD_LENGTH
from lunalink.afs._afs import EPOCHS_PER_FRAME as EPOCHS_PER_FRAME
from lunalink.afs._afs import FRAME_DURATION_S as FRAME_DURATION_S
from lunalink.afs._afs import FRAME_LENGTH as FRAME_LENGTH
from lunalink.afs._afs import INTERIM_ASSIGNMENT_MAX_PRN as INTERIM_ASSIGNMENT_MAX_PRN
from lunalink.afs._afs import IQ_SAMPLES_PER_EPOCH as IQ_SAMPLES_PER_EPOCH
from lunalink.afs._afs import IQ_UPSAMPLE_FACTOR as IQ_UPSAMPLE_FACTOR
from lunalink.afs._afs import PAYLOAD_LENGTH as PAYLOAD_LENGTH
from lunalink.afs._afs import SB1_LENGTH as SB1_LENGTH
from lunalink.afs._afs import SECONDARY_CODE_COUNT as SECONDARY_CODE_COUNT
from lunalink.afs._afs import SECONDARY_CODE_LENGTH as SECONDARY_CODE_LENGTH
from lunalink.afs._afs import SYMBOL_RATE as SYMBOL_RATE
from lunalink.afs._afs import SYNC_LENGTH as SYNC_LENGTH
from lunalink.afs._afs import TERTIARY_CODE_LENGTH as TERTIARY_CODE_LENGTH
from lunalink.afs._afs import BchResult as BchResult
from lunalink.afs._afs import BchStatus as BchStatus
from lunalink.afs._afs import Fid as Fid
from lunalink.afs._afs import FrameStatus as FrameStatus
from lunalink.afs._afs import Toi as Toi
from lunalink.afs._afs import bch_codebook_checksum as _bch_codebook_checksum
from lunalink.afs._afs import bch_decode as _bch_decode
from lunalink.afs._afs import bch_encode as _bch_encode
from lunalink.afs._afs import frame_build_partial as _frame_build_partial
from lunalink.afs._afs import modulate_i as _modulate_i
from lunalink.afs._afs import modulate_q as _modulate_q
from lunalink.afs._afs import multiplex_iq as _multiplex_iq
from lunalink.afs._afs import prn_code as _prn_code
from lunalink.afs._afs import tiered_code_epoch as _tiered_code_epoch
from lunalink.afs._afs import tiered_code_epoch_assigned as _tiered_code_epoch_assigned
from lunalink.afs._afs import weil1500_code as _weil1500_code
from lunalink.afs._afs import weil10230_code as _weil10230_code

__all__ = [
    "bch_encode",
    "bch_decode",
    "bch_codebook_checksum",
    "BchResult",
    "BchStatus",
    "Fid",
    "Toi",
    "frame_build_partial",
    "modulate_i",
    "modulate_q",
    "multiplex_iq",
    "prn_code",
    "tiered_code_epoch",
    "tiered_code_epoch_assigned",
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


def tiered_code_epoch(prn_id: int, epoch_idx: int) -> NDArray[np.uint8]:
    """Return one primary epoch (10230 chips) of the tiered AFS-Q code."""
    return _tiered_code_epoch(prn_id, epoch_idx)


def tiered_code_epoch_assigned(
    primary_prn: int,
    secondary_code_idx: int,
    tertiary_prn: int,
    tertiary_phase_offset: int,
    epoch_idx: int,
) -> NDArray[np.uint8]:
    """Return one primary epoch using explicit AFS-Q code assignments."""
    return _tiered_code_epoch_assigned(
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


def frame_build_partial(fid: int, toi: int) -> NDArray[np.uint8]:
    """Build a partial AFS navigation frame (§2.4)."""
    return _frame_build_partial(fid, toi)
