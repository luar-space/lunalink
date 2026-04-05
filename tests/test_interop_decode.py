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

    # ── C++ correlator tests ─────────────────────────────────────────────
    # These depend on the proprietary receiver::Correlator. They are excluded
    # from the OSS export and will be ported to the V2 API in Phase 3
    # (needs native sample-rate processing, not 1.023 MSPS decimation).

    @pytest.mark.skip(
        reason="Requires proprietary receiver::Correlator — port to V2 API in Phase 3"
    )
    def test_cpp_correlator_acquire(self, lans_iq: np.ndarray) -> None:
        """C++ PCPS correlator acquires a LANS-AFS-SIM signal."""

    @pytest.mark.skip(
        reason="Requires proprietary receiver::Correlator — port to V2 API in Phase 3"
    )
    def test_cpp_correlator_track_decode(self, lans_iq: np.ndarray) -> None:
        """C++ correlator acquire + track + frame decode."""

    @pytest.mark.xfail(
        reason="Requires 5.115 MSPS processing to avoid Q-channel aliasing. "
        "Phase 3 adds native sample-rate support via LuGRE loader.",
        strict=True,
    )
    @pytest.mark.skip(
        reason="Requires proprietary receiver::Correlator — port in Phase 3"
    )
    def test_cpp_correlator_awgn_decode(self, lans_iq: np.ndarray) -> None:
        """C++ correlator decode of 2-bit quantised 7-SV signal with AWGN."""
