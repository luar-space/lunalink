"""LunaLink AFS subpackage — Augmented Forward Signal implementation."""

from lunalink.afs.signal import (
    EPOCHS_PER_FRAME,
    SECONDARY_CODE_COUNT,
    SECONDARY_CODE_LENGTH,
    TERTIARY_CODE_LENGTH,
    modulate_i,
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
    "tiered_code_epoch",
    "tiered_code_epoch_assigned",
    "EPOCHS_PER_FRAME",
    "SECONDARY_CODE_LENGTH",
    "SECONDARY_CODE_COUNT",
    "TERTIARY_CODE_LENGTH",
]
