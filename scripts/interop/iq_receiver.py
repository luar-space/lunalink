# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""AFS-I acquisition and despreading in NumPy, for interoperability decodes.

LunaLink's C++ core starts at the symbol stream (:func:`lunalink.afs.receive_frame`
does sync, deinterleaving, LDPC, CRC and parsing).  Turning baseband I/Q into
that symbol stream needs acquisition and tracking, which these helpers provide
at one sample per chip (1.023 MSPS):

- :func:`pcps_acquire` finds the code delay and Doppler of one PRN;
- :func:`despread_symbols` despreads the AFS-I data channel with a PLL,
  returning one soft symbol per 2 ms code period;
- :func:`decode_iq_file` runs both, then ``receive_frame``, on an ``LSISIQ``
  file (optionally gzip-compressed).

Used by ``tests/test_interop_decode.py`` (signals from LANS-AFS-SIM) and
``scripts/interop/conformance.py`` (Level 4 against the shared vectors).
"""

from __future__ import annotations

import gzip
import tempfile
from pathlib import Path

import numpy as np

from lunalink.afs import DecodeResult, prn_code, read_iq_file, receive_frame

CHIP_RATE_HZ = 1_023_000.0
CHIPS_PER_EPOCH = 2046


def pcps_acquire(
    iq: np.ndarray,
    prn_id: int,
    doppler_range_hz: float = 5000.0,
    doppler_step_hz: float = 250.0,
) -> tuple[int, float]:
    """PCPS acquisition at 1.023 MSPS: find code delay and Doppler.

    Returns (code_delay_chips, doppler_hz).
    """
    code_chips = prn_code(prn_id).astype(np.float32)
    code_bipolar = 1.0 - 2.0 * code_chips  # {0,1} → {+1,-1}

    chunk = iq[:CHIPS_PER_EPOCH]
    fft_code_conj = np.conj(np.fft.fft(code_bipolar, n=CHIPS_PER_EPOCH))

    best_peak = 0.0
    best_delay = 0
    best_doppler = 0.0

    ts = np.arange(CHIPS_PER_EPOCH, dtype=np.float64) / CHIP_RATE_HZ
    doppler_bins = np.arange(
        -doppler_range_hz, doppler_range_hz + doppler_step_hz, doppler_step_hz
    )

    for doppler in doppler_bins:
        carrier = np.exp(-1j * 2.0 * np.pi * doppler * ts).astype(np.complex64)
        baseband = chunk * carrier
        corr = np.abs(
            np.fft.ifft(np.fft.fft(baseband, n=CHIPS_PER_EPOCH) * fft_code_conj)
        )
        peak_idx = int(np.argmax(corr))
        peak_val = corr[peak_idx]
        if peak_val > best_peak:
            best_peak = peak_val
            best_delay = peak_idx
            best_doppler = doppler

    return best_delay, best_doppler


def despread_symbols(
    iq: np.ndarray,
    prn_id: int,
    doppler_hz: float,
    code_delay: int,
) -> np.ndarray:
    """Despread I-channel data symbols at 1.023 MSPS with PLL tracking.

    Returns one soft symbol per 2 ms code period.
    """
    code_chips = prn_code(prn_id).astype(np.float32)
    code_bipolar = 1.0 - 2.0 * code_chips  # {0,1} → {+1,-1}
    code_aligned = np.roll(code_bipolar, code_delay)

    n_epochs = len(iq) // CHIPS_PER_EPOCH
    symbols = np.zeros(n_epochs, dtype=np.float32)

    ts = np.arange(CHIPS_PER_EPOCH, dtype=np.float64) / CHIP_RATE_HZ
    epoch_duration = CHIPS_PER_EPOCH / CHIP_RATE_HZ

    # PLL state
    phase_est = 0.0
    freq_est = doppler_hz * 2.0 * np.pi
    pll_bw = 10.0
    pll_gain = 4.0 * pll_bw * epoch_duration

    for epoch in range(n_epochs):
        start = epoch * CHIPS_PER_EPOCH
        chunk = iq[start : start + CHIPS_PER_EPOCH]
        if len(chunk) < CHIPS_PER_EPOCH:
            break

        phase_ramp = phase_est + freq_est * ts
        carrier = np.exp(-1j * phase_ramp).astype(np.complex64)
        baseband = chunk * carrier

        corr = np.sum(baseband * code_aligned)
        symbols[epoch] = np.real(corr)

        data_sign = 1.0 if np.real(corr) >= 0 else -1.0
        phase_error = np.arctan2(
            np.imag(corr) * data_sign,
            np.abs(np.real(corr)),
        )

        freq_est += pll_gain * phase_error / epoch_duration * 0.1
        phase_est += freq_est * epoch_duration + pll_gain * phase_error

    return symbols


def decode_iq_file(path: Path, prn_id: int) -> DecodeResult:
    """Acquire, despread and decode one PRN from an ``LSISIQ`` file.

    A ``.gz`` file is decompressed to a temporary file first, since
    :func:`lunalink.afs.read_iq_file` reads plain files.  The I/Q is reduced to
    one sample per chip by taking the middle sample of each chip.
    """
    if path.suffix == ".gz":
        with tempfile.NamedTemporaryFile(suffix=".iq") as tmp:
            with gzip.open(path) as src:
                tmp.write(src.read())
            tmp.flush()
            sig = read_iq_file(tmp.name)
    else:
        sig = read_iq_file(path)

    step = int(round(sig.sample_rate / CHIP_RATE_HZ))
    iq = (sig.iq[:, 0] + 1j * sig.iq[:, 1]).astype(np.complex64)[step // 2 :: step]
    delay, doppler = pcps_acquire(iq, prn_id)
    symbols = despread_symbols(iq, prn_id, doppler, delay)
    return receive_frame(np.sign(symbols).astype(np.float32), raise_on_error=False)
