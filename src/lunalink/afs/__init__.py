"""LunaLink AFS subpackage — Augmented Forward Signal implementation."""

from lunalink.afs.signal import (
    EPOCHS_PER_FRAME,
    INTERIM_ASSIGNMENT_MAX_PRN,
    IQ_SAMPLES_PER_EPOCH,
    IQ_UPSAMPLE_FACTOR,
    SECONDARY_CODE_COUNT,
    SECONDARY_CODE_LENGTH,
    TERTIARY_CODE_LENGTH,
    modulate_i,
    modulate_q,
    multiplex_iq,
    prn_code,
    tiered_code_epoch,
    tiered_code_epoch_assigned,
    weil1500_code,
    weil10230_code,
)

__all__ = [
    "prn_code",
    "weil10230_code",
    "weil1500_code",
    "modulate_i",
    "modulate_q",
    "multiplex_iq",
    "tiered_code_epoch",
    "tiered_code_epoch_assigned",
    "EPOCHS_PER_FRAME",
    "SECONDARY_CODE_LENGTH",
    "SECONDARY_CODE_COUNT",
    "INTERIM_ASSIGNMENT_MAX_PRN",
    "TERTIARY_CODE_LENGTH",
    "IQ_UPSAMPLE_FACTOR",
    "IQ_SAMPLES_PER_EPOCH",
]
