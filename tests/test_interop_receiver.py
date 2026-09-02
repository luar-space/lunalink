# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""L4/L5 — a third-party receiver decoding a LunaLink signal.

Levels 1-3 have LunaLink decode LANS-AFS-SIM's signal. These are the reverse
direction: PocketSDR-AFS (osqzss), an independent software-defined LANS AFS
receiver, acquires, tracks, LDPC-decodes and CRC-verifies a signal LunaLink
generated, and the navigation payload it recovers is compared against the
payload LunaLink transmitted.

Skipped unless the receiver has been built, because building it clones two
external library trees and needs libfftw3 and libusb:

    python scripts/interop/build_pocketsdr_afs.py

The work itself lives in ``scripts/interop/pocketsdr_afs_decode.py`` so it can
also be run by hand; this module is the pytest face of it.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
PSDR = REPO_ROOT / "third_party" / "PocketSDR-AFS"
POCKET_TRK = PSDR / "app" / "pocket_trk" / "pocket_trk"
DEMO = REPO_ROOT / "scripts" / "interop" / "pocketsdr_afs_decode.py"
EVIDENCE = REPO_ROOT / "docs" / "assurance" / "evidence" / "interop_l4_receiver.json"

pytestmark = pytest.mark.skipif(
    not POCKET_TRK.is_file(),
    reason="PocketSDR-AFS not built — run scripts/interop/build_pocketsdr_afs.py",
)


@pytest.fixture(scope="module")
def receiver_result() -> dict:
    """Generate a LunaLink signal, decode it with PocketSDR-AFS, return the evidence.

    One run serves both levels: it is a minute of wall clock and ~800 MB of
    intermediate I/Q, so it is not repeated per test.
    """
    proc = subprocess.run(
        [sys.executable, str(DEMO)], cwd=REPO_ROOT, capture_output=True, text=True
    )
    assert proc.returncode == 0, f"demonstration failed:\n{proc.stdout}\n{proc.stderr}"
    return json.loads(EVIDENCE.read_text())


class TestL4ThirdPartyDecode:
    """L4 — the receiver decodes a LunaLink signal."""

    def test_receiver_acquires_both_components(self, receiver_result: dict) -> None:
        """AFSD (data) and AFSP (pilot) are both acquired."""
        signals = {a["signal"] for a in receiver_result["received"]["acquired"]}
        assert {"AFSD", "AFSP"} <= signals, f"acquired only {signals}"

    def test_receiver_decodes_a_subframe(self, receiver_result: dict) -> None:
        """At least one subframe passes the receiver's own LDPC and CRC checks.

        PocketSDR-AFS logs ``$SB2`` only when ``sdr_decode_LDPC_AFS_SF2``
        returns a non-negative error count *and* its CRC-24 test passes
        (``sdr_nav.c``), so the presence of the record is the check.
        """
        assert receiver_result["l4_decoded"], "no subframe decoded"
        assert receiver_result["received"]["decoded"], "no $SB2 record logged"


class TestL5PayloadCrossCheck:
    """L5 — what it recovered is what LunaLink sent."""

    def test_payload_matches_transmitted(self, receiver_result: dict) -> None:
        """The decoded SB2 bits equal the transmitted ones."""
        transmitted = receiver_result["transmitted"]["sb2_hex80"]
        for record in receiver_result["received"]["decoded"]:
            assert record["sb2_hex80"] == transmitted

    def test_crc24_matches_transmitted(self, receiver_result: dict) -> None:
        """Both implementations agree on the CRC-24 over the subframe."""
        transmitted = receiver_result["transmitted"]["sb2_crc24"]
        for record in receiver_result["received"]["decoded"]:
            assert record["sb2_crc24"] == transmitted

    def test_evidence_records_both_levels(self, receiver_result: dict) -> None:
        """The committed artefact states the verdict for both levels."""
        assert receiver_result["l4_decoded"] is True
        assert receiver_result["l5_payload_match"] is True
