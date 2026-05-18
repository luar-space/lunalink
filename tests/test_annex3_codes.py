"""Byte-exact cross-check of Level 1 code vectors against LNIS Annex 3.

Guards the claim that the LunaLink reference implementation produces hex
representations of Gold-2046, Weil-10230 and Weil-1500 codes that match the
normative Annex 3 attachments for every PRN 1-210.  This is the oracle the
public ``lsis-afs-test-vectors`` package relies on.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from lunalink.afs import PRN_COUNT, prn_code, weil1500_code, weil10230_code
from lunalink.afs.vectors import _bits_to_hex_annex3

REPO = Path(__file__).resolve().parents[1]
GOLD_REF = REPO / "docs/references/006_GoldCode2046hex210prns.txt"
WEIL_P_REF = REPO / "docs/references/007_l1cp_hex210prns.txt"
WEIL_T_REF = REPO / "docs/references/008_Weil1500hex210prns.txt"

pytestmark = pytest.mark.skipif(
    not (GOLD_REF.exists() and WEIL_P_REF.exists() and WEIL_T_REF.exists()),
    reason="Annex 3 reference files not present under docs/references/",
)


def _parse_quoted(path: Path) -> list[str]:
    return [s.upper() for s in re.findall(r'"([0-9A-Fa-f]+)"', path.read_text())]


def _parse_plain(path: Path) -> list[str]:
    return [line.strip().upper() for line in path.read_text().splitlines() if line.strip()]


@pytest.fixture(scope="module")
def gold_ref() -> list[str]:
    return _parse_quoted(GOLD_REF)


@pytest.fixture(scope="module")
def weil_primary_ref() -> list[str]:
    return _parse_plain(WEIL_P_REF)


@pytest.fixture(scope="module")
def weil_tertiary_ref() -> list[str]:
    return _parse_quoted(WEIL_T_REF)


@pytest.mark.parametrize("prn", range(1, PRN_COUNT + 1))
def test_gold_code_matches_annex3(prn: int, gold_ref: list[str]) -> None:
    got = _bits_to_hex_annex3(prn_code(prn), prepend_zeros=2)
    assert got == gold_ref[prn - 1], f"PRN {prn}: Gold mismatch"


@pytest.mark.parametrize("prn", range(1, PRN_COUNT + 1))
def test_weil_primary_matches_annex3(prn: int, weil_primary_ref: list[str]) -> None:
    got = _bits_to_hex_annex3(weil10230_code(prn), prepend_zeros=2)
    assert got == weil_primary_ref[prn - 1], f"PRN {prn}: Weil-10230 mismatch"


@pytest.mark.parametrize("prn", range(1, PRN_COUNT + 1))
def test_weil_tertiary_matches_annex3(prn: int, weil_tertiary_ref: list[str]) -> None:
    got = _bits_to_hex_annex3(weil1500_code(prn), prepend_zeros=0)
    assert got == weil_tertiary_ref[prn - 1], f"PRN {prn}: Weil-1500 mismatch"
