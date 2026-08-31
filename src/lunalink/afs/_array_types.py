# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
# ruff: noqa: UP040, F722 — jaxtyping aliases require TypeAlias (not `type`
# keyword) for runtime evaluation, and ruff misparses jaxtyping dimension
# strings like "samples 2".
"""Semantic array types for the AFS signal chain.

Type aliases named after LSIS V1.0 spec constructs.  Used in type stubs
and Python wrappers for IDE documentation and runtime shape validation
via jaxtyping + beartype.

Each alias encodes dtype and shape — hover in your IDE to see exactly
what a function expects.  Runtime enforcement comes from the
``@jaxtyped(typechecker=beartype)`` decorator on public entry points.

Examples
--------
>>> from lunalink.afs._array_types import Sb2Payload, FrameSymbols
>>> import numpy as np
>>> payload: Sb2Payload = np.zeros(1176, dtype=np.uint8)  # OK
"""

from typing import TypeAlias

import numpy as np
from jaxtyping import Complex64, Float32, Float64, Int8, Int16, UInt8

# ── Spreading codes ──────────────────────────────────────────────────────────
#: AFS-I Gold code — 2046 chips per epoch.
GoldCode: TypeAlias = UInt8[np.ndarray, "2046"]
#: AFS-Q Weil primary code — 10230 chips.
WeilPrimary: TypeAlias = UInt8[np.ndarray, "10230"]
#: AFS-Q Weil tertiary code — 1500 chips.
WeilTertiary: TypeAlias = UInt8[np.ndarray, "1500"]
#: Combined AFS-Q spreading code for one epoch (primary + secondary + tertiary).
MatchedCode: TypeAlias = UInt8[np.ndarray, "10230"]

# ── Modulation ───────────────────────────────────────────────────────────────
#: BPSK-modulated I-channel chips (+1/-1).
BpskChipsI: TypeAlias = Int8[np.ndarray, "2046"]
#: BPSK-modulated Q-channel chips (+1/-1).
BpskChipsQ: TypeAlias = Int8[np.ndarray, "10230"]
#: Multiplexed I/Q output at native rate — (10230, 2) int16 pairs.
IqMuxNative: TypeAlias = Int16[np.ndarray, "10230 2"]

# ── Forward error correction ─────────────────────────────────────────────────
#: BCH(51,8) codeword — 52 hard-decision bits.
BchCodeword: TypeAlias = UInt8[np.ndarray, "52"]
#: BCH soft-decision input — 52 log-likelihood ratios.
BchSoftInput: TypeAlias = Float32[np.ndarray, "52"]
#: Block interleaver input/output — 60x98 = 5880 bits.
InterleaverBlock: TypeAlias = UInt8[np.ndarray, "5880"]
#: Soft-decision interleaver block — 5880 LLRs.
InterleaverBlockSoft: TypeAlias = Float32[np.ndarray, "5880"]

# ── Subframe payloads (user data before FEC encoding) ────────────────────────
#: SB2 user data — 1176 bits (WN + ITOW + CED + health + time-conv + spare).
Sb2Payload: TypeAlias = UInt8[np.ndarray, "1176"]
#: SB3 user data — 846 bits (6-bit type + 840-bit message).
Sb3Payload: TypeAlias = UInt8[np.ndarray, "846"]
#: SB4 user data — 846 bits (6-bit type + 840-bit message).
Sb4Payload: TypeAlias = UInt8[np.ndarray, "846"]

# ── Frame ────────────────────────────────────────────────────────────────────
#: Complete navigation frame — SP(68) + SB1(52) + payload(5880) = 6000 symbols.
FrameSymbols: TypeAlias = UInt8[np.ndarray, "6000"]

# ── Signal generation (variable-length) ──────────────────────────────────────
#: Resampled float32 I/Q pairs — shape (N, 2).
IqPairFloat: TypeAlias = Float32[np.ndarray, "samples 2"]
#: Native-rate int16 I/Q pairs — shape (N, 2).
IqPairNative: TypeAlias = Int16[np.ndarray, "samples 2"]

# ── Receiver (variable-length) ───────────────────────────────────────────────
#: Baseband complex samples for correlator input.
RxSamples: TypeAlias = Complex64[np.ndarray, " samples"]
#: Demodulated soft-decision symbols (bipolar float).
SoftSymbols: TypeAlias = Float32[np.ndarray, " symbols"]

# ── Generic variable-length ──────────────────────────────────────────────────
#: Generic bit array (uint8, any length).
BitArray: TypeAlias = UInt8[np.ndarray, " bits"]
#: Error time series for SISE computation.
ErrorSeries: TypeAlias = Float64[np.ndarray, " points"]

__all__ = [
    # Spreading codes
    "GoldCode",
    "WeilPrimary",
    "WeilTertiary",
    "MatchedCode",
    # Modulation
    "BpskChipsI",
    "BpskChipsQ",
    "IqMuxNative",
    # FEC
    "BchCodeword",
    "BchSoftInput",
    "InterleaverBlock",
    "InterleaverBlockSoft",
    # Subframe payloads
    "Sb2Payload",
    "Sb3Payload",
    "Sb4Payload",
    # Frame
    "FrameSymbols",
    # Signal generation
    "IqPairFloat",
    "IqPairNative",
    # Receiver
    "RxSamples",
    "SoftSymbols",
    # Generic
    "BitArray",
    "ErrorSeries",
]
