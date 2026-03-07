"""Tests for AFS signal generation (C1–C5)."""

import numpy as np
import pytest

from lunalink.afs import (
    EPOCHS_PER_FRAME,
    INTERIM_ASSIGNMENT_MAX_PRN,
    SECONDARY_CODE_COUNT,
    SECONDARY_CODE_LENGTH,
    TERTIARY_CODE_LENGTH,
    BchStatus,
    bch_decode,
    bch_encode,
    frame_build_partial,
    matched_code_epoch,
    matched_code_epoch_assigned,
    modulate_i,
    modulate_q,
    multiplex_iq,
    prn_code,
    weil1500_code,
    weil10230_code,
)


class TestPrnCode:
    """Tests for Gold-2046 PRN code loader (C1)."""

    def test_shape_and_dtype(self):
        """PRN code returns correct shape and dtype."""
        chips = prn_code(1)
        assert chips.shape == (2046,)
        assert chips.dtype == np.uint8

    def test_chips_binary(self):
        """All chip values are 0 or 1."""
        chips = prn_code(1)
        assert set(chips.tolist()).issubset({0, 1})

    def test_all_210_prns_load(self):
        """All 210 PRNs load successfully with correct shape."""
        for prn_id in range(1, 211):
            assert prn_code(prn_id).shape == (2046,)

    def test_prns_are_distinct(self):
        """Different PRN IDs produce different sequences."""
        assert not np.array_equal(prn_code(1), prn_code(2))

    def test_out_of_range_raises(self):
        """Out-of-range PRN IDs are saturated safely (C-Pattern 4)."""
        # PRN 0 -> saturates to 1
        chips0 = prn_code(0)
        assert chips0.shape == (2046,)
        # PRN 211 -> saturates to 210
        chips211 = prn_code(211)
        assert chips211.shape == (2046,)


class TestWeil10230Code:
    """Tests for Weil-10230 PRN code loader (C1, AFS-Q primary)."""

    def test_shape_and_dtype(self):
        """Weil-10230 code returns correct shape and dtype."""
        chips = weil10230_code(1)
        assert chips.shape == (10230,)
        assert chips.dtype == np.uint8

    def test_chips_binary(self):
        """All chip values are 0 or 1."""
        chips = weil10230_code(1)
        assert set(chips.tolist()).issubset({0, 1})

    def test_all_210_prns_load(self):
        """All 210 PRNs load successfully with correct shape."""
        for prn_id in range(1, 211):
            assert weil10230_code(prn_id).shape == (10230,)

    def test_prns_are_distinct(self):
        """Different PRN IDs produce different sequences."""
        assert not np.array_equal(weil10230_code(1), weil10230_code(2))

    def test_out_of_range_raises(self):
        """Out-of-range PRN IDs are saturated safely (C-Pattern 4)."""
        chips0 = weil10230_code(0)  # saturates to 1
        assert chips0.shape == (10230,)
        chips211 = weil10230_code(211)  # saturates to 210
        assert chips211.shape == (10230,)


class TestWeil1500Code:
    """Tests for Weil-1500 PRN code loader (C1, AFS-Q tertiary)."""

    def test_shape_and_dtype(self):
        """Weil-1500 code returns correct shape and dtype."""
        chips = weil1500_code(1)
        assert chips.shape == (1500,)
        assert chips.dtype == np.uint8

    def test_chips_binary(self):
        """All chip values are 0 or 1."""
        chips = weil1500_code(1)
        assert set(chips.tolist()).issubset({0, 1})

    def test_all_210_prns_load(self):
        """All 210 PRNs load successfully with correct shape."""
        for prn_id in range(1, 211):
            assert weil1500_code(prn_id).shape == (1500,)

    def test_prns_are_distinct(self):
        """Different PRN IDs produce different sequences."""
        assert not np.array_equal(weil1500_code(1), weil1500_code(2))

    def test_out_of_range_raises(self):
        """Out-of-range PRN IDs are saturated safely (C-Pattern 4)."""
        chips0 = weil1500_code(0)  # saturates to 1
        assert chips0.shape == (1500,)
        chips211 = weil1500_code(211)  # saturates to 210
        assert chips211.shape == (1500,)


class TestModulateI:
    """Tests for AFS-I BPSK modulator (C2)."""

    def test_shape_and_dtype(self):
        """Modulated output has correct shape and dtype."""
        out = modulate_i(prn_code(1), 1)
        assert out.shape == (2046,)
        assert out.dtype == np.int8

    def test_values_binary(self):
        """All modulated values are -1 or +1."""
        out = modulate_i(prn_code(1), 1)
        assert set(out.tolist()).issubset({-1, 1})

    def test_chip_mapping(self):
        """Chip mapping: logic 0 -> +1, logic 1 -> -1 (spec Table 8)."""
        chips = np.array([0, 1, 0, 1], dtype=np.uint8)
        out = modulate_i(chips, 1)
        np.testing.assert_array_equal(out, [1, -1, 1, -1])

    def test_symbol_flip(self):
        """Data symbol -1 inverts all samples."""
        chips = prn_code(1)
        assert np.array_equal(modulate_i(chips, -1), -modulate_i(chips, 1))

    def test_invalid_symbol_raises(self):
        """Invalid data symbol raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            modulate_i(prn_code(1), 0)


class TestMatchedCodeEpoch:
    """Tests for AFS-Q matched code combiner (C3)."""

    def test_shape_and_dtype(self):
        """Matched code epoch returns correct shape and dtype."""
        chips = matched_code_epoch(1, 0)
        assert chips.shape == (10230,)
        assert chips.dtype == np.uint8

    def test_chips_binary(self):
        """All chip values are 0 or 1."""
        chips = matched_code_epoch(1, 0)
        assert set(chips.tolist()).issubset({0, 1})

    def test_xor_identity(self):
        """Matched code equals primary XOR secondary XOR tertiary."""
        primary = weil10230_code(1)
        tertiary = weil1500_code(1)
        # PRN 1 -> S0 = [1,1,1,0]; epoch 0 -> sec chip = 1, tert chip idx 0
        sec_chip = 1  # S0[0]
        tert_chip = int(tertiary[0])
        expected = (primary ^ sec_chip ^ tert_chip).astype(np.uint8)
        actual = matched_code_epoch(1, 0)
        np.testing.assert_array_equal(actual, expected)

    def test_epochs_per_frame_constant(self):
        """EPOCHS_PER_FRAME is 6000 (4 secondary x 1500 tertiary)."""
        assert EPOCHS_PER_FRAME == 6000
        assert SECONDARY_CODE_LENGTH == 4
        assert SECONDARY_CODE_COUNT == 4
        assert INTERIM_ASSIGNMENT_MAX_PRN == 12
        assert TERTIARY_CODE_LENGTH == 1500

    def test_out_of_range_raises(self):
        """Out-of-range inputs are saturated safely (C-Pattern 4)."""
        # PRN 0 -> saturates to 1
        chips = matched_code_epoch(0, 0)
        assert chips.shape == (10230,)
        # Epoch index still raises as it's not a CheckedRange parameter
        # in the binding lambda yet
        with pytest.raises((ValueError, Exception)):
            matched_code_epoch(1, 6000)

    def test_assigned_out_of_range_raises(self):
        """Invalid explicit assignment values are saturated safely (C-Pattern 4)."""
        # PRN 0 -> saturates to 1
        chips = matched_code_epoch_assigned(0, 0, 1, 0, 0)
        assert chips.shape == (10230,)

        # secondary_code_idx 4 -> saturates to 3
        chips2 = matched_code_epoch_assigned(1, 4, 1, 0, 0)
        assert chips2.shape == (10230,)

    def test_matched_code_epoch_assigned_execution(self):
        """Execute matched_code_epoch_assigned to verify binding."""
        chips = matched_code_epoch_assigned(1, 0, 1, 0, 0)
        assert chips.shape == (10230,)
        assert chips.dtype == np.uint8


class TestModulateQ:
    """Tests for AFS-Q BPSK(5) pilot modulator (C2-Q)."""

    def test_shape_and_dtype(self):
        """Modulated output has correct shape and dtype."""
        chips = matched_code_epoch(1, 0)
        out = modulate_q(chips)
        assert out.shape == (10230,)
        assert out.dtype == np.int8

    def test_values_bipolar(self):
        """All modulated values are -1 or +1."""
        out = modulate_q(matched_code_epoch(1, 0))
        assert set(out.tolist()).issubset({-1, 1})

    def test_chip_mapping(self):
        """Chip mapping: logic 0 -> +1, logic 1 -> -1 (spec Table 8)."""
        chips = np.array([0, 1, 0, 1], dtype=np.uint8)
        out = modulate_q(chips)
        np.testing.assert_array_equal(out, [1, -1, 1, -1])


class TestMultiplexIq:
    """Tests for IQ multiplexer (C4)."""

    def test_shape_and_dtype(self):
        """Output has correct shape and dtype."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(matched_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        assert iq.shape == (10230, 2)
        assert iq.dtype == np.int16

    def test_i_upsampled_5x(self):
        """AFS-I samples are repeated 5x to match Q rate."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(matched_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        i_out = iq[:, 0]
        # Each I chip should be repeated 5 times
        for chip_idx in range(10):
            block = i_out[chip_idx * 5 : (chip_idx + 1) * 5]
            assert np.all(block == i_samples[chip_idx])

    def test_equal_power(self):
        """Both channels have equal mean-square power (50/50 per LSIS-103)."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(matched_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        power_i = np.mean(iq[:, 0].astype(np.float64) ** 2)
        power_q = np.mean(iq[:, 1].astype(np.float64) ** 2)
        assert power_i == pytest.approx(power_q)


class TestBchEncode:
    """Tests for BCH(51,8) encoder (C5)."""

    def test_shape_and_dtype(self):
        """Output has correct shape and dtype."""
        out = bch_encode(0, 69)
        assert out.shape == (52,)
        assert out.dtype == np.uint8

    def test_spec_test_vector(self):
        """Matches LSIS V1.0 Figure 8: SB1=0x045, encoded=0x229f61dbb84a0."""
        expected = np.array(
            [
                0,
                0,
                1,
                0,
                0,
                0,
                1,
                0,
                1,
                0,
                0,
                1,
                1,
                1,
                1,
                1,
                0,
                1,
                1,
                0,
                0,
                0,
                0,
                1,
                1,
                1,
                0,
                1,
                1,
                0,
                1,
                1,
                1,
                0,
                1,
                1,
                1,
                0,
                0,
                0,
                0,
                1,
                0,
                0,
                1,
                0,
                1,
                0,
                0,
                0,
                0,
                0,
            ],
            dtype=np.uint8,
        )
        np.testing.assert_array_equal(bch_encode(0, 69), expected)

    def test_invalid_fid_raises(self):
        """FID > 3 is saturated safely (C-Pattern 4)."""
        # FID 4 -> saturates to 3
        out = bch_encode(4, 0)
        assert out.shape == (52,)

    def test_invalid_toi_raises(self):
        """TOI > 99 raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            bch_encode(0, 100)


class TestFrameBuildPartial:
    """Tests for partial frame builder (C8)."""

    def test_frame_build_partial_execution(self):
        """Execute frame_build_partial to verify binding."""
        frame = frame_build_partial(0, 69)
        assert frame.shape == (6000,)
        assert frame.dtype == np.uint8


class TestBchDecode:
    """Tests for BCH(51,8) maximum likelihood decoder (C5)."""

    def test_round_trip_zero_errors(self):
        """Zero-error codewords decode to original FID/TOI."""
        for fid_v in [0, 1, 2, 3]:
            for toi_v in [0, 10, 69, 99]:
                codeword = bch_encode(fid_v, toi_v)
                result = bch_decode(codeword)
                assert int(result.fid) == fid_v
                assert int(result.toi.value) == toi_v
                assert result.hamming_distance == 0

    def test_single_error_correction(self):
        """Single-bit flips are correctly recovered."""
        fid_v, toi_v = 2, 42
        codeword = bch_encode(fid_v, toi_v)
        codeword[10] ^= 1
        result = bch_decode(codeword)
        assert int(result.fid) == fid_v
        assert int(result.toi.value) == toi_v
        assert result.hamming_distance == 1

    def test_double_error_correction(self):
        """Double-bit flips are correctly recovered (d_min >= 5)."""
        fid_v, toi_v = 1, 99
        codeword = bch_encode(fid_v, toi_v)
        codeword[0] ^= 1
        codeword[51] ^= 1
        result = bch_decode(codeword)
        assert int(result.fid) == fid_v
        assert int(result.toi.value) == toi_v
        assert result.hamming_distance == 2

    def test_confidence_threshold(self):
        """Triple-bit flips result in NullOutput status."""
        codeword = bch_encode(0, 0)
        codeword[0] ^= 1
        codeword[1] ^= 1
        codeword[2] ^= 1
        result = bch_decode(codeword)
        # Status should be NullOutput (Hamming dist 3 > 2)
        assert result.status == BchStatus.NULL_OUTPUT
        assert result.hamming_distance == 3

    def test_codebook_checksum(self):
        """Verify the codebook checksum binding."""
        from lunalink.afs import bch_codebook_checksum

        chk = bch_codebook_checksum()
        assert isinstance(chk, int)
        assert chk != 0
