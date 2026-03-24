"""I5 — L3 Interoperability: cross-implementation signal decode.

Generates I/Q from LANS-AFS-SIM, then feeds it through LunaLink's RX chain
(correlator → symbol extraction → frame decode) to demonstrate cross-decode.

This is the primary interoperability deliverable: an independent implementation
generated the signal, and LunaLink's receiver decoded it successfully.

Prerequisites:
    python scripts/interop/build_lans_afs_sim.py
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import numpy as np
import pytest

from lunalink.afs import receive_frame
from lunalink.afs._afs import (
    FFT_SIZE,
    ChannelState,
    Correlator,
    CorrelatorStatus,
    UniformPrior,
)

REPO_ROOT = Path(__file__).resolve().parents[1]
AFS_SIM = REPO_ROOT / "third_party" / "LANS-AFS-SIM" / "afs_sim"

pytestmark = pytest.mark.skipif(
    not AFS_SIM.is_file(),
    reason="LANS-AFS-SIM not built — run scripts/interop/build_lans_afs_sim.py",
)

# Generate at 5.115 MHz (5× chip rate), decimate to 1.023 MSPS for all processing.
LANS_SAMPLE_RATE_HZ = 5_115_000.0
CHIP_RATE_HZ = 1_023_000.0
DECIMATION_FACTOR = int(LANS_SAMPLE_RATE_HZ / CHIP_RATE_HZ)  # 5
CHIPS_PER_EPOCH = 2046
FRAME_DURATION_S = 25.0  # >2 frames ensures we capture at least one complete


def _generate_lans_iq(duration: float, outdir: Path) -> Path:
    """Generate I/Q from LANS-AFS-SIM."""
    outfile = outdir / "lans_interop.bin"
    subprocess.run(
        [
            str(AFS_SIM),
            "-t",
            str(duration),
            "-s",
            str(LANS_SAMPLE_RATE_HZ),
            str(outfile),
        ],
        cwd=AFS_SIM.parent,
        check=True,
        capture_output=True,
    )
    return outfile


def _load_and_decimate(path: Path) -> np.ndarray:
    """Load LANS-AFS-SIM int16 I/Q, decimate to 1.023 MSPS, return complex64.

    All processing — Python and C++ — works at the chip rate.
    """
    raw = np.fromfile(path, dtype=np.int16)
    iq_full = raw[0::2].astype(np.float32) + 1j * raw[1::2].astype(np.float32)
    return iq_full[::DECIMATION_FACTOR]


def _epoch_block(iq: np.ndarray, epoch: int) -> np.ndarray:
    """Extract one 2048-sample epoch from decimated I/Q (2046 chips + 2 zero-pad)."""
    start = epoch * CHIPS_PER_EPOCH
    chunk = iq[start : start + CHIPS_PER_EPOCH]
    padded = np.zeros(FFT_SIZE, dtype=np.complex64)
    padded[: len(chunk)] = chunk
    return padded


def _pcps_acquire(
    iq: np.ndarray,
    prn_id: int,
    doppler_range_hz: float = 5000.0,
    doppler_step_hz: float = 250.0,
) -> tuple[int, float]:
    """PCPS acquisition at 1.023 MSPS: find code delay and Doppler.

    Returns (code_delay_chips, doppler_hz).
    """
    from lunalink.afs import prn_code

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


def _despread_symbols(
    iq: np.ndarray,
    prn_id: int,
    doppler_hz: float,
    code_delay: int,
) -> np.ndarray:
    """Despread I-channel data symbols at 1.023 MSPS with PLL tracking.

    Returns one soft symbol per 2 ms code period.
    """
    from lunalink.afs import prn_code

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


class TestInteropDecode:
    """L3 interop: decode LANS-AFS-SIM signal with LunaLink RX."""

    @pytest.fixture(scope="class")
    def lans_iq(self, tmp_path_factory: pytest.TempPathFactory) -> np.ndarray:
        """Generate LANS-AFS-SIM signal and return decimated I/Q at 1.023 MSPS."""
        tmpdir = tmp_path_factory.mktemp("lans_iq")
        path = _generate_lans_iq(FRAME_DURATION_S, tmpdir)
        return _load_and_decimate(path)

    @pytest.fixture(scope="class")
    def acquired(self, lans_iq: np.ndarray) -> tuple[int, float]:
        """PCPS acquisition on PRN 3 (highest elevation, Doppler ~ 0 Hz)."""
        delay, doppler = _pcps_acquire(lans_iq, 3)
        return delay, doppler

    def test_acquisition(self, acquired: tuple[int, float]) -> None:
        """Verify PCPS finds a valid code delay and Doppler."""
        delay, doppler = acquired
        assert 0 <= delay < CHIPS_PER_EPOCH, f"Invalid delay: {delay}"
        assert abs(doppler) < 500, f"Doppler too far from expected: {doppler}"

    def test_python_despread_decode(
        self,
        lans_iq: np.ndarray,
        acquired: tuple[int, float],
    ) -> None:
        """Python PLL despreading → frame decode.

        Verifies the signal processing chain independently of the C++
        correlator, using the same 1.023 MSPS data.
        """
        delay, doppler = acquired
        symbols = _despread_symbols(lans_iq, 3, doppler, delay)
        bipolar = np.sign(symbols).astype(np.float32)

        assert len(bipolar) >= 6000
        result = receive_frame(bipolar, raise_on_error=True)

        assert result.fid == 0, f"Expected FID=0, got {result.fid}"
        assert result.sb2.crc_ok, "SB2 CRC failed"
        assert result.sb3.crc_ok, "SB3 CRC failed"
        assert result.sb4.crc_ok, "SB4 CRC failed"
        if result.time is not None:
            assert result.time.wn == 2277

    def test_cpp_correlator_acquire(self, lans_iq: np.ndarray) -> None:
        """C++ PCPS correlator acquires a LANS-AFS-SIM signal."""
        prior = UniformPrior()
        correlator = Correlator(prior)
        status = correlator.assign_channel(0, 3)
        assert status == CorrelatorStatus.OK

        padded = _epoch_block(lans_iq, 0)
        result, status = correlator.acquire(0, padded)
        assert status == CorrelatorStatus.OK, f"acquire failed: {status}"
        assert result.locked, (
            f"Acquisition did not lock (metric={result.peak_metric:.1f}, "
            f"delay={result.code_delay:.1f}, doppler={result.doppler_hz:.1f})"
        )

    def test_cpp_correlator_track_decode(self, lans_iq: np.ndarray) -> None:
        """C++ correlator acquire + track + frame decode.

        The headline test: LunaLink's flight-grade C++ correlator
        (PCPS acquisition, DLL+PLL tracking, data symbol extraction)
        processes an independently generated LSIS-AFS signal end-to-end.
        """
        prior = UniformPrior()
        correlator = Correlator(prior)
        correlator.assign_channel(0, 3)

        n_epochs = len(lans_iq) // CHIPS_PER_EPOCH
        symbols = []

        for epoch in range(n_epochs):
            padded = _epoch_block(lans_iq, epoch)
            state = correlator.channel_state(0)

            if state in (ChannelState.IDLE, ChannelState.ACQUIRING):
                result, _ = correlator.acquire(0, padded)
                if result.locked:
                    correlator.track(0, padded)
            elif state == ChannelState.TRACKING:
                status = correlator.track(0, padded)
                if status == CorrelatorStatus.OK:
                    symbols.append(correlator.data_symbol(0))
            elif state == ChannelState.LOST:
                correlator.reset_channel(0)
                correlator.assign_channel(0, 3)

        symbols = np.array(symbols, dtype=np.float32)

        assert len(symbols) >= 6068, (
            f"Only {len(symbols)} symbols tracked, need >= 6068"
        )

        bipolar = np.sign(symbols).astype(np.float32)
        result = receive_frame(bipolar, raise_on_error=True)

        assert result.fid == 0, f"Expected FID=0, got {result.fid}"
        assert result.sb2.crc_ok, "SB2 CRC failed — C++ PLL tracking error"
        assert result.sb3.crc_ok, "SB3 CRC failed"
        assert result.sb4.crc_ok, "SB4 CRC failed"

    @pytest.mark.xfail(
        reason="Correlator V1 operates at 1.023 MSPS — aliases Q-channel pilot "
        "as noise. 2-bit 7-SV acquisition requires pilot-channel "
        "processing at composite rate (V2).",
        strict=True,
    )
    def test_cpp_correlator_awgn_decode(
        self, tmp_path_factory: pytest.TempPathFactory
    ) -> None:
        """C++ correlator decode of LANS-AFS-SIM 2-bit quantised signal with AWGN.

        Uses -b 2 mode (thermal noise + 2-bit ADC: {-3, -1, +1, +3}) with
        7 visible SVs. Non-coherent integration (N=10 epochs, 20 ms) enables
        acquisition at the low per-SV SNR. Demonstrates realistic receiver
        performance under conditions more challenging than the operational
        Moonlight constellation (3-4 SVs).
        """
        tmpdir = tmp_path_factory.mktemp("lans_awgn")
        outfile = tmpdir / "lans_awgn.bin"
        subprocess.run(
            [
                str(AFS_SIM),
                "-t",
                str(FRAME_DURATION_S),
                "-s",
                str(LANS_SAMPLE_RATE_HZ),
                "-b",
                "2",
                str(outfile),
            ],
            cwd=AFS_SIM.parent,
            check=True,
            capture_output=True,
        )

        # Load int8 I/Q (2-bit mode writes int8, not int16)
        raw = np.fromfile(str(outfile), dtype=np.int8)
        iq_full = raw[0::2].astype(np.float32) + 1j * raw[1::2].astype(np.float32)
        iq = iq_full[::DECIMATION_FACTOR]

        prior = UniformPrior()
        correlator = Correlator(prior)
        correlator.assign_channel(0, 3)

        n_epochs = len(iq) // CHIPS_PER_EPOCH
        integration_epochs = 10  # non-coherent integration for weak signal
        symbols = []

        epoch = 0
        while epoch < n_epochs:
            state = correlator.channel_state(0)

            if state in (ChannelState.IDLE, ChannelState.ACQUIRING):
                # Multi-epoch acquisition: feed N epochs at once
                end = min(epoch + integration_epochs, n_epochs)
                acq_samples = []
                for e in range(epoch, end):
                    acq_samples.append(_epoch_block(iq, e))
                acq_block = np.concatenate(acq_samples)
                result, _ = correlator.acquire(0, acq_block)
                if result.locked:
                    # Start tracking from the last epoch used for acquisition
                    epoch = end - 1
                    correlator.track(0, _epoch_block(iq, epoch))
                else:
                    epoch = end
                    continue
            elif state == ChannelState.TRACKING:
                padded = _epoch_block(iq, epoch)
                status = correlator.track(0, padded)
                if status == CorrelatorStatus.OK:
                    symbols.append(correlator.data_symbol(0))
            elif state == ChannelState.LOST:
                correlator.reset_channel(0)
                correlator.assign_channel(0, 3)

            epoch += 1

        symbols_arr = np.array(symbols, dtype=np.float32)
        assert len(symbols_arr) >= 6068, (
            f"Only {len(symbols_arr)} symbols tracked with AWGN, need >= 6068"
        )

        bipolar = np.sign(symbols_arr).astype(np.float32)
        result = receive_frame(bipolar, raise_on_error=True)

        assert result.fid == 0, f"Expected FID=0, got {result.fid}"
        assert result.sb2.crc_ok, "SB2 CRC failed under AWGN"
        assert result.sb3.crc_ok, "SB3 CRC failed under AWGN"
        assert result.sb4.crc_ok, "SB4 CRC failed under AWGN"
