# ruff: noqa: D102
"""Roundtrip: validate LunaLink's own published test vectors as foreign input.

Treats test-vectors/ as if produced by another team and verifies that
LunaLink can decode every frame and match every decode-direction vector.

Prerequisites:
    python tests/generate_test_vectors.py
"""

from __future__ import annotations

import gzip
import json
from pathlib import Path

import numpy as np
import pytest

from lunalink.afs import (
    prn_code,
    receive_frame,
)
from lunalink.afs._afs import crc24_calculate
from lunalink.afs.signal import bch_decode, bch_encode

VECTORS_DIR = Path(__file__).resolve().parents[1] / "test-vectors"

pytestmark = pytest.mark.skipif(
    not VECTORS_DIR.exists(),
    reason="test-vectors/ not generated — run tests/generate_test_vectors.py",
)


def _read_json(path: Path) -> dict:
    if path.suffix == ".gz":
        return json.loads(gzip.open(path, "rb").read())
    return json.loads(path.read_text())


class TestSelfFrames:
    """Verify published frame vectors decode correctly."""

    @pytest.fixture(scope="class")
    def frame_files(self) -> list[Path]:
        return sorted((VECTORS_DIR / "frames").glob("*.bin"))

    def test_all_frames_decode(self, frame_files: list[Path]) -> None:
        assert len(frame_files) >= 5, "Expected at least 5 frame files"
        for path in frame_files:
            raw = path.read_bytes()
            # Strip 64-byte LSISAFS header if present
            if raw[:7] == b"LSISAFS":
                raw = raw[64:]
            data = np.frombuffer(raw, dtype=np.uint8)
            assert len(data) == 6000, f"{path.name}: wrong length {len(data)}"

            bipolar = np.where(data == 0, 1.0, -1.0).astype(np.float32)
            result = receive_frame(bipolar, raise_on_error=True)
            assert result.sb2.crc_ok, f"{path.name}: SB2 CRC failed"
            assert result.sb3.crc_ok, f"{path.name}: SB3 CRC failed"
            assert result.sb4.crc_ok, f"{path.name}: SB4 CRC failed"


class TestSelfDecodeVectors:
    """Verify published decode-direction vectors match live decode."""

    @pytest.fixture(scope="class")
    def decode_files(self) -> list[Path]:
        return sorted((VECTORS_DIR / "decode").glob("*.json*"))

    def test_all_decode_vectors(self, decode_files: list[Path]) -> None:
        assert len(decode_files) >= 5, "Expected at least 5 decode vectors"
        for path in decode_files:
            vec = _read_json(path)
            expected = vec["expected"]

            # Load the corresponding frame
            frame_rel = expected.get("input_file") or vec.get("input_file", "")
            if frame_rel:
                frame_path = path.parent / frame_rel
            else:
                # Fallback: match by index
                idx = path.stem.split("_")[1] if "_" in path.stem else "1"
                frame_path = VECTORS_DIR / "frames" / f"test_message_{idx}*.bin"
                candidates = sorted(VECTORS_DIR.glob(f"frames/*{idx}*.bin"))
                if not candidates:
                    pytest.skip(f"No matching frame for {path.name}")
                frame_path = candidates[0]

            raw = frame_path.read_bytes()
            if raw[:7] == b"LSISAFS":
                raw = raw[64:]
            data = np.frombuffer(raw, dtype=np.uint8)
            bipolar = np.where(data == 0, 1.0, -1.0).astype(np.float32)

            result = receive_frame(bipolar, raise_on_error=True)

            assert result.fid == expected["fid"], (
                f"{path.name}: FID {result.fid} != {expected['fid']}"
            )
            assert result.toi == expected["toi"], (
                f"{path.name}: TOI {result.toi} != {expected['toi']}"
            )
            if result.time is not None and "wn" in expected:
                assert result.time.wn == expected["wn"], (
                    f"{path.name}: WN {result.time.wn} != {expected['wn']}"
                )


class TestSelfFEC:
    """L2 FEC: verify BCH, CRC-24, and LDPC vectors against live encode."""

    def test_bch_vectors(self) -> None:
        vec = _read_json(VECTORS_DIR / "fec" / "bch_vectors.json")
        for v in vec["vectors"]:
            live_cw = bch_encode(v["fid"], v["toi"])
            published = np.array(v["codeword_bits"], dtype=np.uint8)
            assert np.array_equal(live_cw, published), (
                f"BCH mismatch: FID={v['fid']} TOI={v['toi']}"
            )
            result = bch_decode(published)
            assert result.fid == v["decoded_fid"]
            assert result.toi == v["decoded_toi"]

    def test_crc24_vectors(self) -> None:
        vec = _read_json(VECTORS_DIR / "fec" / "crc24_vectors.json")
        for v in vec["vectors"]:
            input_bits = np.array(v["input_bits"], dtype=np.uint8)
            live_crc = crc24_calculate(input_bits)
            published = np.array(v["crc_bits"], dtype=np.uint8)
            assert np.array_equal(live_crc, published), (
                f"CRC-24 mismatch: {v['description']}"
            )


class TestSelfSignals:
    """L3: verify published I/Q signals can be correlated back."""

    @pytest.mark.parametrize("prn_id", range(1, 13))
    def test_iq_correlates_with_code(self, prn_id: int) -> None:
        """Correlate published I/Q with the matching PRN code.

        The auto-correlation peak should be maximal at zero lag.
        """
        import gzip

        path = VECTORS_DIR / "signals" / f"signal_prn{prn_id}_5115ksps.iq.gz"
        raw = gzip.open(path, "rb").read()

        # Skip 128-byte LSISIQ header
        header = raw[:128]
        assert header[:8] == b"LSISIQ\x00\x00", f"Bad magic: {header[:8]}"

        iq_bytes = raw[128:]
        iq = np.frombuffer(iq_bytes, dtype=np.float32).reshape(-1, 2)
        signal_i = iq[:, 0]  # I channel

        # Generate live code, map to bipolar, and correlate
        code = prn_code(prn_id).astype(np.float32)
        code_bipolar = 1.0 - 2.0 * code  # {0,1} → {+1,-1}

        # The I/Q is at 5.115 MHz (5 samples/chip). Decimate to 1 sample/chip.
        signal_decimated = signal_i[::5][:2046]
        assert len(signal_decimated) == 2046

        corr = np.abs(np.correlate(signal_decimated, code_bipolar, mode="full"))
        peak_idx = np.argmax(corr)
        peak_val = corr[peak_idx]
        mean_val = np.mean(corr)

        # Peak should be significantly above mean (>10× for noiseless signal)
        assert peak_val > 10 * mean_val, (
            f"PRN {prn_id}: weak correlation peak "
            f"(peak={peak_val:.0f}, mean={mean_val:.0f})"
        )


class TestSelfParsed:
    """L5: verify parsed JSON fields match live decode."""

    def test_all_parsed_vectors(self) -> None:
        parsed_files = sorted((VECTORS_DIR / "parsed").glob("*.json"))
        frame_files = sorted((VECTORS_DIR / "frames").glob("*.bin"))
        assert len(parsed_files) >= 5
        assert len(frame_files) >= 5

        for parsed_path, frame_path in zip(parsed_files, frame_files):
            parsed = _read_json(parsed_path)

            # Decode the corresponding frame
            raw = frame_path.read_bytes()
            if raw[:7] == b"LSISAFS":
                raw = raw[64:]
            data = np.frombuffer(raw, dtype=np.uint8)
            bipolar = np.where(data == 0, 1.0, -1.0).astype(np.float32)
            result = receive_frame(bipolar, raise_on_error=True)

            # Verify parsed fields match live decode
            assert result.fid == parsed["subframe1"]["fid"], (
                f"{parsed_path.name}: FID mismatch"
            )
            assert result.toi == parsed["subframe1"]["toi"], (
                f"{parsed_path.name}: TOI mismatch"
            )
