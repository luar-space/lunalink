# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""L3 Interoperability: cross-implementation signal decode.

Generates I/Q from LANS-AFS-SIM (an independent third-party LSIS-AFS
implementation), then recovers the navigation frame with LunaLink:
PCPS acquisition and PLL despreading in NumPy
(``scripts/interop/iq_receiver.py``), followed by
:func:`lunalink.afs.receive_frame` for sync, deinterleaving, LDPC, CRC,
and message parsing.

This is the primary interoperability deliverable: an independent
implementation generated the signal, and LunaLink decoded it successfully.

Prerequisites:
    python scripts/interop/build_lans_afs_sim.py
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

from lunalink.afs import receive_frame

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "scripts" / "interop"))

from iq_receiver import (  # noqa: E402
    CHIPS_PER_EPOCH,
    despread_symbols,
    pcps_acquire,
)

AFS_SIM = REPO_ROOT / "third_party" / "LANS-AFS-SIM" / "afs_sim"

pytestmark = pytest.mark.skipif(
    not AFS_SIM.is_file(),
    reason="LANS-AFS-SIM not built — run scripts/interop/build_lans_afs_sim.py",
)

# Generate at 5.115 MHz (5× chip rate), decimate to 1.023 MSPS for all processing.
LANS_SAMPLE_RATE_HZ = 5_115_000.0
DECIMATION_FACTOR = int(LANS_SAMPLE_RATE_HZ / 1_023_000.0)  # 5
FRAME_DURATION_S = 25.0  # >2 frames ensures we capture at least one complete


def _generate_lans_iq(
    duration: float, outdir: Path, quant_bits: int | None = None
) -> Path:
    """Generate I/Q from LANS-AFS-SIM.

    Passes ``-b <quant_bits>`` only when the caller overrides the default,
    so existing call sites keep the simulator's native quantisation.
    """
    outfile = outdir / "lans_interop.bin"
    cmd = [str(AFS_SIM), "-t", str(duration), "-s", str(LANS_SAMPLE_RATE_HZ)]
    if quant_bits is not None:
        cmd += ["-b", str(quant_bits)]
    cmd.append(str(outfile))
    subprocess.run(
        cmd,
        cwd=AFS_SIM.parent,
        check=True,
        capture_output=True,
    )
    return outfile


def _load_and_decimate(path: Path) -> np.ndarray:
    """Load LANS-AFS-SIM int16 I/Q, decimate to 1.023 MSPS, return complex64."""
    raw = np.fromfile(path, dtype=np.int16)
    iq_full = raw[0::2].astype(np.float32) + 1j * raw[1::2].astype(np.float32)
    return iq_full[::DECIMATION_FACTOR]


class TestInteropDecode:
    """L3 interop: decode a LANS-AFS-SIM signal with LunaLink."""

    @pytest.fixture(scope="class")
    @classmethod
    def lans_iq(cls, tmp_path_factory: pytest.TempPathFactory) -> np.ndarray:
        """Generate LANS-AFS-SIM signal and return decimated I/Q at 1.023 MSPS."""
        tmpdir = tmp_path_factory.mktemp("lans_iq")
        path = _generate_lans_iq(FRAME_DURATION_S, tmpdir)
        return _load_and_decimate(path)

    @pytest.fixture(scope="class")
    @classmethod
    def acquired(cls, lans_iq: np.ndarray) -> tuple[int, float]:
        """PCPS acquisition on PRN 3 (highest elevation, Doppler ~ 0 Hz)."""
        delay, doppler = pcps_acquire(lans_iq, 3)
        return delay, doppler

    def test_acquisition(self, acquired: tuple[int, float]) -> None:
        """Verify PCPS finds a valid code delay and Doppler."""
        delay, doppler = acquired
        assert 0 <= delay < CHIPS_PER_EPOCH, f"Invalid delay: {delay}"
        assert abs(doppler) < 500, f"Doppler too far from expected: {doppler}"

    def test_despread_decode(
        self,
        lans_iq: np.ndarray,
        acquired: tuple[int, float],
    ) -> None:
        """PLL despreading → frame decode of the third-party signal."""
        delay, doppler = acquired
        symbols = despread_symbols(lans_iq, 3, doppler, delay)
        bipolar = np.sign(symbols).astype(np.float32)

        assert len(bipolar) >= 6000
        result = receive_frame(bipolar, raise_on_error=True)

        assert result.fid == 0, f"Expected FID=0, got {result.fid}"
        assert result.sb2.crc_ok, "SB2 CRC failed"
        assert result.sb3.crc_ok, "SB3 CRC failed"
        assert result.sb4.crc_ok, "SB4 CRC failed"
        if result.time is not None:
            assert result.time.wn == 2277
