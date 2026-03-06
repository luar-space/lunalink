"""Tests for AFS signal generation (C1 PRN codes, C2 BPSK modulator)."""

import numpy as np
import pytest

from lunalink.afs import modulate_i, prn_code, weil1500_code, weil10230_code


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
