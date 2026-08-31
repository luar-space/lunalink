# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
# ruff: noqa: D102
"""I3 — L1 Interoperability: PRN code bit-exact comparison with LANS-AFS-SIM.

Runs the dump_lans_codes harness (compiled from LANS-AFS-SIM) to extract
Gold and Weil codes, then compares chip-by-chip against LunaLink's tables.

Pass criteria: every chip of every PRN (1–210) matches bit-exact.

Prerequisites:
    python scripts/interop/build_lans_afs_sim.py
"""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

import numpy as np
import pytest

from lunalink.afs import prn_code, weil10230_code

REPO_ROOT = Path(__file__).resolve().parents[1]
DUMP_BIN = REPO_ROOT / "third_party" / "LANS-AFS-SIM" / "dump_lans_codes"

pytestmark = pytest.mark.skipif(
    not DUMP_BIN.is_file(),
    reason="LANS-AFS-SIM not built — run scripts/interop/build_lans_afs_sim.py",
)

MAX_PRN = 210


@pytest.fixture(scope="module")
def lans_codes_dir() -> Path:
    """Run dump_lans_codes once and return the output directory."""
    tmpdir = Path(tempfile.mkdtemp(prefix="lans_codes_"))
    subprocess.run(
        [str(DUMP_BIN), str(tmpdir), str(MAX_PRN)],
        check=True,
    )
    return tmpdir


def _load_lans_code(codes_dir: Path, prefix: str, prn: int) -> np.ndarray:
    path = codes_dir / f"{prefix}_prn_{prn:03d}.bin"
    return np.frombuffer(path.read_bytes(), dtype=np.uint8)


class TestInteropGoldCodes:
    """L1 interop: Gold code (I-channel) bit-exact comparison, PRN 1–210."""

    @pytest.mark.parametrize("prn_id", range(1, MAX_PRN + 1))
    def test_gold_code_bitexact(self, prn_id: int, lans_codes_dir: Path) -> None:
        lunalink = prn_code(prn_id)
        lans = _load_lans_code(lans_codes_dir, "gold", prn_id)
        assert lunalink.shape == lans.shape == (2046,)
        mismatches = int(np.sum(lunalink != lans))
        assert mismatches == 0, (
            f"PRN {prn_id}: {mismatches}/2046 chip mismatches "
            f"(first at index {int(np.argmax(lunalink != lans))})"
        )


class TestInteropWeilCodes:
    """L1 interop: Weil primary code (Q-channel) bit-exact comparison, PRN 1–210."""

    @pytest.mark.parametrize("prn_id", range(1, MAX_PRN + 1))
    def test_weil_primary_bitexact(self, prn_id: int, lans_codes_dir: Path) -> None:
        lunalink = weil10230_code(prn_id)
        lans = _load_lans_code(lans_codes_dir, "weil", prn_id)
        assert lunalink.shape == lans.shape == (10230,)
        mismatches = int(np.sum(lunalink != lans))
        assert mismatches == 0, (
            f"PRN {prn_id}: {mismatches}/10230 chip mismatches "
            f"(first at index {int(np.argmax(lunalink != lans))})"
        )
