# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
# ruff: noqa: D101, D102  — test classes and methods are self-documenting by name.
"""Tests for the high-level Python API (receive_frame, transmit_frame, vectors)."""

from __future__ import annotations

import json
import os
import struct
import tempfile

import numpy as np
import pytest

from lunalink.afs import (
    SB2_DATA_BITS,
    SB2_SPARE_BITS,
    SB2_SPARE_OFFSET,
    SB3_DATA_BITS,
    SB4_DATA_BITS,
    DecodeError,
    DecodeResult,
    SubframeResult,
    SyncNotFoundError,
    TimeOfTransmission,
    TransmitFrame,
    frame_build,
    make_test_payload,
    receive_frame,
    transmit_frame,
)

# ── Result dataclass tests ──────────────────────────────────────────────────


class TestSubframeResult:
    def test_frozen(self):
        r = SubframeResult(
            data=np.zeros(10, dtype=np.uint8), ldpc_converged=True, crc_ok=True
        )
        with pytest.raises(AttributeError):
            r.crc_ok = False  # type: ignore[misc]


class TestTimeOfTransmission:
    def test_seconds_of_week(self):
        t = TimeOfTransmission(wn=100, itow=42, toi=17, t_f=0.0)
        assert t.seconds_of_week == 42 * 1200.0 + 17 * 12.0

    def test_frozen(self):
        t = TimeOfTransmission(wn=0, itow=0, toi=0, t_f=0.0)
        with pytest.raises(AttributeError):
            t.wn = 1  # type: ignore[misc]


class TestTransmitFrame:
    def test_frozen(self):
        f = TransmitFrame(symbols=np.zeros(10, dtype=np.uint8), fid=0, toi=0, prn_id=1)
        with pytest.raises(AttributeError):
            f.fid = 1  # type: ignore[misc]


# ── make_test_payload tests ─────────────────────────────────────────────────


class TestMakeTestPayload:
    @staticmethod
    def _sb2_payload_region(sb2):
        """Return SB2 without its spare region.

        The spare region always carries the LSIS-AFS-300 alternating pattern,
        regardless of the requested fill.
        """
        return np.delete(
            sb2, np.arange(SB2_SPARE_OFFSET, SB2_SPARE_OFFSET + SB2_SPARE_BITS)
        )

    @staticmethod
    def _assert_spare_pattern(sb2):
        spare = sb2[SB2_SPARE_OFFSET : SB2_SPARE_OFFSET + SB2_SPARE_BITS]
        expected = np.arange(SB2_SPARE_BITS, dtype=np.uint8) % 2
        np.testing.assert_array_equal(spare, expected)

    def test_zeros(self):
        sb2, sb3, sb4 = make_test_payload("zeros")
        assert sb2.shape == (SB2_DATA_BITS,)
        assert sb3.shape == (SB3_DATA_BITS,)
        assert sb4.shape == (SB4_DATA_BITS,)
        assert np.all(self._sb2_payload_region(sb2) == 0)
        assert np.all(sb3 == 0) and np.all(sb4 == 0)
        self._assert_spare_pattern(sb2)

    def test_ones(self):
        sb2, sb3, sb4 = make_test_payload("ones")
        assert np.all(self._sb2_payload_region(sb2) == 1)
        assert np.all(sb3 == 1) and np.all(sb4 == 1)
        self._assert_spare_pattern(sb2)

    def test_alternating(self):
        sb2, _, _ = make_test_payload("alternating")
        assert sb2[0] == 0 and sb2[1] == 1 and sb2[2] == 0

    def test_random_deterministic(self):
        a2, a3, a4 = make_test_payload("random", seed=99)
        b2, b3, b4 = make_test_payload("random", seed=99)
        np.testing.assert_array_equal(a2, b2)
        np.testing.assert_array_equal(a3, b3)
        np.testing.assert_array_equal(a4, b4)

    def test_random_different_seeds(self):
        a2, _, _ = make_test_payload("random", seed=1)
        b2, _, _ = make_test_payload("random", seed=2)
        assert not np.array_equal(a2, b2)

    def test_invalid_pattern(self):
        with pytest.raises(ValueError, match="Unknown pattern"):
            make_test_payload("garbage")


# ── receive_frame tests ──────────────────────────────────────────────────────


class TestDecodeFrame:
    @staticmethod
    def _pack_sb2(wn: int, itow: int) -> np.ndarray:
        """Pack WN (13 bits) + ITOW (9 bits) into SB2 data bits."""
        sb2 = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
        for i in range(13):
            sb2[i] = (wn >> (12 - i)) & 1
        for i in range(9):
            sb2[13 + i] = (itow >> (8 - i)) & 1
        return sb2

    def test_round_trip_clean(self):
        """Encode then decode a frame over a perfect channel."""
        wn, itow, toi_val = 100, 42, 17
        sb2 = self._pack_sb2(wn, itow)
        sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)

        frame = frame_build(0, toi_val, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)

        result = receive_frame(bipolar)

        assert isinstance(result, DecodeResult)
        assert result.all_ok
        assert result.fid == 0
        assert result.toi == toi_val
        assert result.time is not None
        assert result.time.wn == wn
        assert result.time.itow == itow
        assert result.time.toi == toi_val

    def test_round_trip_zeros(self):
        """Simplest payload: all zeros."""
        sb2, sb3, sb4 = make_test_payload("zeros")
        frame = frame_build(0, 42, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)

        result = receive_frame(bipolar)
        assert result.all_ok
        assert result.toi == 42

    def test_sync_not_found_raises(self):
        """Random noise should not contain a valid sync pattern."""
        rng = np.random.default_rng(12345)
        noise = rng.standard_normal(7000).astype(np.float32)
        with pytest.raises(SyncNotFoundError):
            receive_frame(noise)

    def test_sync_not_found_no_raise(self):
        """With raise_on_error=False, returns partial result."""
        rng = np.random.default_rng(12345)
        noise = rng.standard_normal(7000).astype(np.float32)
        result = receive_frame(noise, raise_on_error=False)
        assert isinstance(result, DecodeResult)
        # Partial result — time will be None since nothing decoded
        assert not result.all_ok

    def test_decode_error_partial_result(self):
        """DecodeError carries the partial result for inspection."""
        # Build a valid frame then corrupt the LDPC payload
        sb2, sb3, sb4 = make_test_payload("zeros")
        frame = frame_build(0, 42, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        # Corrupt the payload region (after sync + SB1 = 120 symbols)
        bipolar[200:400] *= -1

        try:
            receive_frame(bipolar)
        except (DecodeError, SyncNotFoundError) as e:
            if isinstance(e, DecodeError):
                assert hasattr(e, "result")
                assert isinstance(e.result, DecodeResult)

    def test_decode_error_raised(self):
        """Heavy payload corruption triggers DecodeError."""
        sb2, sb3, sb4 = make_test_payload("zeros")
        frame = frame_build(0, 42, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        # Flip ALL payload symbols (after sync=68 + SB1=52 = 120)
        bipolar[120:] *= -1
        with pytest.raises(DecodeError) as exc_info:
            receive_frame(bipolar)
        assert isinstance(exc_info.value.result, DecodeResult)
        assert exc_info.value.result.time is None

    def test_decode_error_no_raise(self):
        """With raise_on_error=False, corrupted frame returns partial result."""
        sb2, sb3, sb4 = make_test_payload("zeros")
        frame = frame_build(0, 42, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        bipolar[120:] *= -1
        result = receive_frame(bipolar, raise_on_error=False)
        assert isinstance(result, DecodeResult)
        assert not result.all_ok


# ── transmit_frame tests ────────────────────────────────────────────────────


class TestTransmitFrameFunction:
    def test_basic(self):
        sb2, sb3, sb4 = make_test_payload("zeros")
        frame, iq = transmit_frame(
            prn_id=1,
            fid=0,
            toi=42,
            sb2_data=sb2,
            sb3_data=sb3,
            sb4_data=sb4,
        )
        assert isinstance(frame, TransmitFrame)
        assert frame.fid == 0
        assert frame.toi == 42
        assert frame.prn_id == 1
        assert len(frame.symbols) == 6000
        assert iq.ndim == 2 and iq.shape[1] == 2

    def test_round_trip_with_decode(self):
        """transmit_frame → receive_frame round-trip."""
        sb2, sb3, sb4 = make_test_payload("ones")
        frame, _iq = transmit_frame(
            prn_id=1,
            fid=1,
            toi=99,
            sb2_data=sb2,
            sb3_data=sb3,
            sb4_data=sb4,
        )
        bipolar = np.where(frame.symbols == 0, 1.0, -1.0).astype(np.float32)
        result = receive_frame(bipolar)
        assert result.all_ok
        assert result.fid == 1
        assert result.toi == 99


# ── vectors module tests ────────────────────────────────────────────────────


class TestVectors:
    def test_export_code_vectors(self):
        from lunalink.afs.vectors import export_code_vectors

        with tempfile.TemporaryDirectory() as tmpdir:
            path = export_code_vectors(1, tmpdir)
            assert os.path.exists(path)
            with open(path, encoding="utf-8") as fh:
                content = fh.read()
            assert "[GOLD_CODE]" in content
            assert "[WEIL_PRIMARY]" in content
            assert "[WEIL_TERTIARY]" in content
            assert "length: 2046" in content
            assert "length: 10230" in content
            assert "length: 1500" in content

    def test_export_frame_vector(self):
        from lunalink.afs.vectors import export_frame_vector

        sb2, sb3, sb4 = make_test_payload("zeros")
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "test.bin")
            export_frame_vector(1, 0, 42, sb2, sb3, sb4, path)
            assert os.path.exists(path)
            with open(path, "rb") as f:
                header = f.read(64)
                assert header[:8] == b"LSISAFS\x00"
                frame_len = struct.unpack_from("<I", header, 12)[0]
                assert frame_len == 6000
                data = f.read()
                assert len(data) == 6000

    def test_export_parsed_json(self):
        from lunalink.afs.vectors import export_parsed_json

        sb2, sb3, sb4 = make_test_payload("zeros")
        frame = frame_build(0, 42, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        result = receive_frame(bipolar)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "parsed.json")
            export_parsed_json(result, path, frame_id="test_zeros")
            with open(path, encoding="utf-8") as fh:
                doc = json.load(fh)
            assert doc["subframe1"]["fid"] == 0
            assert doc["subframe1"]["toi"] == 42
            assert doc["subframe2"]["crc_ok"] is True
            assert doc["subframe2"]["data_hex"] is not None

    def test_generate_standard_vectors(self):
        from lunalink.afs.vectors import generate_standard_vectors

        with tempfile.TemporaryDirectory() as tmpdir:
            paths = generate_standard_vectors(tmpdir, prn_id=1)
            assert len(paths["codes"]) == 1
            assert len(paths["frames"]) == 4
            assert len(paths["signals"]) == 4
            assert len(paths["parsed"]) == 4
            # All files exist
            for file_list in paths.values():
                for p in file_list:
                    assert os.path.exists(p), f"Missing: {p}"
