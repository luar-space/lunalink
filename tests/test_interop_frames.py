# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
# ruff: noqa: D102
"""I4 — L2 Interoperability: frame encoding comparison with LANS-AFS-SIM.

Runs the dump_lans_frame harness to encode frames with known payloads using
LANS-AFS-SIM's pipeline (BCH + CRC-24 + LDPC + interleaver), then compares
the 6000-symbol output bit-exact against LunaLink's frame_build().

Pass criteria: every symbol of every test frame matches bit-exact.

Prerequisites:
    python scripts/interop/build_lans_afs_sim.py
"""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

import numpy as np
import pytest

from lunalink.afs import frame_build

REPO_ROOT = Path(__file__).resolve().parents[1]
DUMP_BIN = REPO_ROOT / "third_party" / "LANS-AFS-SIM" / "dump_lans_frame"

pytestmark = pytest.mark.skipif(
    not DUMP_BIN.is_file(),
    reason="LANS-AFS-SIM not built — run scripts/interop/build_lans_afs_sim.py",
)

# Test cases: (fid, toi, pattern_name, sb2_fill, sb3_fill, sb4_fill)
TEST_CASES = [
    (0, 42, "zeros"),
    (0, 42, "ones"),
    # alternating1 → first packed byte 0xAA (matches generator's "alternating",
    # frame_3.bin, and the public lsis-afs-test-vectors v0.2.0 convention).
    (0, 42, "alternating1"),
    (1, 0, "zeros"),
    (3, 99, "zeros"),
]


def _make_payload(pattern: str, length: int) -> np.ndarray:
    """Create a payload matching the C harness fill_pattern()."""
    if pattern == "zeros":
        return np.zeros(length, dtype=np.uint8)
    if pattern == "ones":
        return np.ones(length, dtype=np.uint8)
    if pattern == "alternating1":
        return np.array([(i + 1) % 2 for i in range(length)], dtype=np.uint8)
    raise ValueError(f"Unknown pattern: {pattern}")


def _case_name(pattern: str, fid: int, toi: int) -> str:
    """Output basename for the C harness's ``<name>`` argument."""
    return f"{pattern}_fid{fid}_toi{toi}"


@pytest.fixture(scope="module")
def lans_frames_dir() -> Path:
    """Run dump_lans_frame for all test cases and return the output directory."""
    tmpdir = Path(tempfile.mkdtemp(prefix="lans_frames_"))
    for fid, toi, pattern in TEST_CASES:
        subprocess.run(
            [
                str(DUMP_BIN),
                str(tmpdir),
                _case_name(pattern, fid, toi),
                str(fid),
                str(toi),
                pattern,
            ],
            check=True,
        )
    return tmpdir


class TestInteropFrames:
    """L2 interop: frame encoding bit-exact comparison."""

    @pytest.mark.parametrize("fid,toi,pattern", TEST_CASES)
    def test_frame_bitexact(
        self,
        fid: int,
        toi: int,
        pattern: str,
        lans_frames_dir: Path,
    ) -> None:
        # Load LANS-AFS-SIM frame
        lans_path = lans_frames_dir / f"lans_frame_{_case_name(pattern, fid, toi)}.bin"
        lans_frame = np.frombuffer(lans_path.read_bytes(), dtype=np.uint8)
        assert lans_frame.shape == (6000,), f"LANS frame size: {lans_frame.shape}"

        # Build LunaLink frame with same payload
        sb2_data = _make_payload(pattern, 1176)
        sb3_data = _make_payload(pattern, 846)
        sb4_data = _make_payload(pattern, 846)
        ll_frame = frame_build(fid, toi, sb2_data, sb3_data, sb4_data)
        assert ll_frame.shape == (6000,)

        # Compare
        mismatches = int(np.sum(lans_frame != ll_frame))
        if mismatches > 0:
            first_idx = int(np.argmax(lans_frame != ll_frame))
            # Identify which section the first mismatch is in
            if first_idx < 68:
                section = "sync"
            elif first_idx < 120:
                section = "SF1 (BCH)"
            else:
                section = "SF2-4 (LDPC+interleave)"
            pytest.fail(
                f"FID={fid} TOI={toi} pattern={pattern}: "
                f"{mismatches}/6000 symbol mismatches "
                f"(first at index {first_idx} [{section}])"
            )
