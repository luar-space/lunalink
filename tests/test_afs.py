"""Tests for AFS signal generation (C1–C5)."""

import numpy as np
import pytest

from lunalink.afs import (
    BCH_CODEWORD_LENGTH,
    EPOCHS_PER_FRAME,
    INTERIM_ASSIGNMENT_MAX_PRN,
    IQ_SAMPLES_PER_EPOCH,
    IQ_UPSAMPLE_FACTOR,
    SECONDARY_CODE_COUNT,
    SECONDARY_CODE_LENGTH,
    TERTIARY_CODE_LENGTH,
    bch_encode,
    modulate_i,
    modulate_q,
    multiplex_iq,
    prn_code,
    tiered_code_epoch,
    tiered_code_epoch_assigned,
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
        """Out-of-range PRN IDs raise ValueError."""
        with pytest.raises((ValueError, Exception)):
            prn_code(0)
        with pytest.raises((ValueError, Exception)):
            prn_code(211)


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
        """Out-of-range PRN IDs raise ValueError."""
        with pytest.raises((ValueError, Exception)):
            weil10230_code(0)
        with pytest.raises((ValueError, Exception)):
            weil10230_code(211)


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
        """Out-of-range PRN IDs raise ValueError."""
        with pytest.raises((ValueError, Exception)):
            weil1500_code(0)
        with pytest.raises((ValueError, Exception)):
            weil1500_code(211)


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


class TestTieredCodeEpoch:
    """Tests for AFS-Q tiered code combiner (C3)."""

    def test_shape_and_dtype(self):
        """Tiered code epoch returns correct shape and dtype."""
        chips = tiered_code_epoch(1, 0)
        assert chips.shape == (10230,)
        assert chips.dtype == np.uint8

    def test_chips_binary(self):
        """All chip values are 0 or 1."""
        chips = tiered_code_epoch(1, 0)
        assert set(chips.tolist()).issubset({0, 1})

    def test_xor_identity(self):
        """Tiered code equals primary XOR secondary XOR tertiary."""
        primary = weil10230_code(1)
        tertiary = weil1500_code(1)
        # PRN 1 -> S0 = [1,1,1,0]; epoch 0 -> sec chip = 1, tert chip idx 0
        sec_chip = 1  # S0[0]
        tert_chip = int(tertiary[0])
        expected = (primary ^ sec_chip ^ tert_chip).astype(np.uint8)
        actual = tiered_code_epoch(1, 0)
        np.testing.assert_array_equal(actual, expected)

    def test_different_epochs_differ(self):
        """Different epochs with different modifiers produce different output."""
        # Epoch 0: S0[0]=1, epoch 3: S0[3]=0 -> modifier differs
        e0 = tiered_code_epoch(1, 0)
        e3 = tiered_code_epoch(1, 3)
        assert not np.array_equal(e0, e3)

    def test_different_prns_differ(self):
        """Different PRNs produce different sequences."""
        assert not np.array_equal(tiered_code_epoch(1, 0), tiered_code_epoch(2, 0))

    def test_epochs_per_frame_constant(self):
        """EPOCHS_PER_FRAME is 6000 (4 secondary x 1500 tertiary)."""
        assert EPOCHS_PER_FRAME == 6000
        assert SECONDARY_CODE_LENGTH == 4
        assert SECONDARY_CODE_COUNT == 4
        assert INTERIM_ASSIGNMENT_MAX_PRN == 12
        assert TERTIARY_CODE_LENGTH == 1500

    def test_out_of_range_raises(self):
        """Out-of-range inputs raise ValueError."""
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch(0, 0)
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch(1, 6000)
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch(1, -1)
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch(13, 0)

    def test_assigned_matches_default_interim_mapping(self):
        """Explicit interim assignment matches tiered_code_epoch output."""
        expected = tiered_code_epoch(1, 0)
        actual = tiered_code_epoch_assigned(
            primary_prn=1,
            secondary_code_idx=0,
            tertiary_prn=1,
            tertiary_phase_offset=0,
            epoch_idx=0,
        )
        np.testing.assert_array_equal(actual, expected)

    def test_assigned_tertiary_phase_offset_applies(self):
        """Changing tertiary phase offset affects output when chips differ."""
        base = tiered_code_epoch_assigned(1, 0, 1, 0, 0)
        shifted = tiered_code_epoch_assigned(1, 0, 1, 1, 0)
        tertiary = weil1500_code(1)
        if int(tertiary[0]) != int(tertiary[1]):
            assert not np.array_equal(base, shifted)

    def test_assigned_out_of_range_raises(self):
        """Invalid explicit assignment values raise ValueError."""
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch_assigned(0, 0, 1, 0, 0)
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch_assigned(1, 4, 1, 0, 0)
        with pytest.raises((ValueError, Exception)):
            tiered_code_epoch_assigned(1, 0, 1, 1500, 0)


class TestModulateQ:
    """Tests for AFS-Q BPSK(5) pilot modulator (C2-Q)."""

    def test_shape_and_dtype(self):
        """Modulated output has correct shape and dtype."""
        chips = tiered_code_epoch(1, 0)
        out = modulate_q(chips)
        assert out.shape == (10230,)
        assert out.dtype == np.int8

    def test_values_bipolar(self):
        """All modulated values are -1 or +1."""
        out = modulate_q(tiered_code_epoch(1, 0))
        assert set(out.tolist()).issubset({-1, 1})

    def test_chip_mapping(self):
        """Chip mapping: logic 0 -> +1, logic 1 -> -1 (spec Table 8)."""
        chips = np.array([0, 1, 0, 1], dtype=np.uint8)
        out = modulate_q(chips)
        np.testing.assert_array_equal(out, [1, -1, 1, -1])

    def test_equals_modulate_i_with_symbol_plus1(self):
        """Q modulation equals I modulation with data_symbol=+1."""
        chips = tiered_code_epoch(1, 0)
        np.testing.assert_array_equal(modulate_q(chips), modulate_i(chips, 1))


class TestMultiplexIq:
    """Tests for IQ multiplexer (C4)."""

    def test_shape_and_dtype(self):
        """Output has correct shape and dtype."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(tiered_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        assert iq.shape == (10230, 2)
        assert iq.dtype == np.int16

    def test_i_upsampled_5x(self):
        """AFS-I samples are repeated 5x to match Q rate."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(tiered_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        i_out = iq[:, 0]
        # Each I chip should be repeated 5 times
        for chip_idx in range(10):
            block = i_out[chip_idx * 5 : (chip_idx + 1) * 5]
            assert np.all(block == i_samples[chip_idx])

    def test_q_passthrough(self):
        """AFS-Q samples pass through at native chip rate."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(tiered_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        np.testing.assert_array_equal(iq[:, 1], q_samples.astype(np.int16))

    def test_equal_power(self):
        """Both channels have equal mean-square power (50/50 per LSIS-103)."""
        i_samples = modulate_i(prn_code(1), 1)
        q_samples = modulate_q(tiered_code_epoch(1, 0))
        iq = multiplex_iq(i_samples, q_samples)
        power_i = np.mean(iq[:, 0].astype(np.float64) ** 2)
        power_q = np.mean(iq[:, 1].astype(np.float64) ** 2)
        assert power_i == pytest.approx(power_q)

    def test_constants(self):
        """IQ constants match spec chip rates."""
        assert IQ_UPSAMPLE_FACTOR == 5
        assert IQ_SAMPLES_PER_EPOCH == 10230

    def test_wrong_i_length_raises(self):
        """Wrong I sample length raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            multiplex_iq(np.ones(100, dtype=np.int8), np.ones(10230, dtype=np.int8))

    def test_wrong_q_length_raises(self):
        """Wrong Q sample length raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            multiplex_iq(np.ones(2046, dtype=np.int8), np.ones(100, dtype=np.int8))


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

    def test_output_binary(self):
        """All output values are 0 or 1."""
        out = bch_encode(1, 50)
        assert set(out.tolist()).issubset({0, 1})

    def test_constant(self):
        """BCH_CODEWORD_LENGTH is 52."""
        assert BCH_CODEWORD_LENGTH == 52

    def test_different_inputs_differ(self):
        """Different FID/TOI produce different codewords."""
        assert not np.array_equal(bch_encode(0, 1), bch_encode(0, 2))

    def test_all_fid_values(self):
        """All four FID values produce valid distinct codewords."""
        cws = [bch_encode(fid, 42) for fid in range(4)]
        for i, cw in enumerate(cws):
            assert set(cw.tolist()).issubset({0, 1})
            assert cw[0] == (i >> 1)  # bit0 = FID MSB
        # All must be distinct.
        for i in range(4):
            for j in range(i + 1, 4):
                assert not np.array_equal(cws[i], cws[j])

    def test_bit0_xor_property(self):
        """FID=2 (bit0=1) flips all 51 LFSR outputs vs FID=0 (bit0=0)."""
        a = bch_encode(0, 69)
        b = bch_encode(2, 69)
        assert a[0] == 0 and b[0] == 1
        np.testing.assert_array_equal(b[1:], 1 - a[1:])

    def test_invalid_fid_raises(self):
        """FID > 3 raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            bch_encode(4, 0)

    def test_invalid_toi_raises(self):
        """TOI > 99 raises ValueError."""
        with pytest.raises((ValueError, Exception)):
            bch_encode(0, 100)
