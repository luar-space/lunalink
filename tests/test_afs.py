"""Tests for AFS signal generation (C1–C5)."""

import os
import tempfile

import numpy as np
import pytest

from lunalink.afs import (
    AFS_Q_CHIP_RATE,
    BLOCK_INTERVAL_DURATION,
    DEFAULT_SYNC_THRESHOLD,
    EPOCHS_PER_FRAME,
    FRAME_DURATION_SEC,
    FRAME_LENGTH,
    INTERIM_ASSIGNMENT_MAX_PRN,
    ITOW_BITS,
    ITOW_MAX,
    SB2_DATA_BITS,
    SB2_LENGTH,
    SB3_DATA_BITS,
    SB3_LENGTH,
    SB4_DATA_BITS,
    SB4_LENGTH,
    SEC_WEEK,
    SECONDARY_CODE_COUNT,
    SECONDARY_CODE_LENGTH,
    SUBFRAME_TYPE_BITS,
    SYNC_LENGTH,
    TERTIARY_CODE_LENGTH,
    TOT_HEADER_BITS,
    TOT_TOI_MAX,
    WN_BITS,
    WN_MAX,
    BchStatus,
    FrameDecodeStatus,
    FrameStatus,
    MessageParseStatus,
    MessageType,
    SyncDetectorStatus,
    TotStatus,
    bch_decode,
    bch_encode,
    block_deinterleave_soft,
    commit_frame,
    compute_tot,
    default_matched_assignment,
    frame_build,
    frame_build_partial,
    frame_decode,
    generate_iq_epoch,
    generate_iq_frame,
    legendre_1499_checksum,
    legendre_10223_checksum,
    legendre_cache_init,
    matched_code_epoch,
    matched_code_epoch_assigned,
    modulate_i,
    modulate_q,
    multiplex_iq,
    parse_sb2,
    parse_sb3,
    parse_sb4,
    prn_code,
    subframe_build_sb2,
    subframe_build_sb3,
    subframe_build_sb4,
    sync_detect,
    weil1500_code,
    weil10230_code,
    write_iq_file,
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

    def test_saturating_toi(self):
        """TOI > 99 saturates to 99."""
        # This no longer raises ValueError because CheckedRange saturates
        codeword = bch_encode(0, 100)
        assert len(codeword) == 52
        result = bch_decode(codeword)
        assert int(result.toi.value) == 99


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


class TestSyncDetect:
    """Tests for sync pattern detector (GD5.2)."""

    def test_perfect_sync_detected(self):
        """Sync pattern at known offset is detected with high confidence."""
        frame = frame_build_partial(0, 69)
        assert frame.shape == (FRAME_LENGTH,)
        # Convert binary {0,1} to bipolar {+1,-1}: bit 0 -> +1, bit 1 -> -1
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        result, status = sync_detect(bipolar)
        assert status == SyncDetectorStatus.OK
        assert result.detected is True
        assert result.frame_offset == 0
        assert result.peak_metric > DEFAULT_SYNC_THRESHOLD

    def test_sync_at_offset(self):
        """Sync pattern embedded at a non-zero offset is found correctly."""
        frame = frame_build_partial(0, 69)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        # Prepend 200 random symbols
        offset = 200
        rng = np.random.default_rng(42)
        noise = rng.choice([-1.0, 1.0], size=offset).astype(np.float32)
        stream = np.concatenate([noise, bipolar])
        result, status = sync_detect(stream)
        assert status == SyncDetectorStatus.OK
        assert result.detected is True
        assert result.frame_offset == offset

    def test_noise_only_not_detected(self):
        """Pure noise does not trigger false detection."""
        rng = np.random.default_rng(123)
        noise = rng.standard_normal(FRAME_LENGTH).astype(np.float32)
        result, status = sync_detect(noise)
        assert status == SyncDetectorStatus.NOT_DETECTED
        assert result.detected is False

    def test_input_too_short(self):
        """Input shorter than sync length returns INVALID_INPUT."""
        short = np.ones(SYNC_LENGTH - 1, dtype=np.float32)
        result, status = sync_detect(short)
        assert status == SyncDetectorStatus.INVALID_INPUT

    def test_custom_threshold(self):
        """Custom threshold is respected."""
        frame = frame_build_partial(0, 69)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        # Very high threshold — should not detect
        result, status = sync_detect(bipolar, threshold=1000.0)
        assert result.detected is False

    def test_constants(self):
        """Sync-related constants have expected values."""
        assert SYNC_LENGTH == 68
        assert FRAME_LENGTH == 6000
        assert DEFAULT_SYNC_THRESHOLD > 0.0


class TestGenerateIqEpoch:
    """Tests for generate_iq_epoch (I/Q signal generation)."""

    def test_native_rate_shape(self):
        """Native rate produces (10230, 2) float32 output."""
        iq = generate_iq_epoch(1, 0)
        assert iq.shape == (10230, 2)
        assert iq.dtype == np.float32

    def test_values_bipolar(self):
        """All I/Q values are -1.0 or +1.0 at native rate."""
        iq = generate_iq_epoch(1, 0)
        assert set(iq[:, 0].tolist()).issubset({-1.0, 1.0})
        assert set(iq[:, 1].tolist()).issubset({-1.0, 1.0})

    def test_data_symbol_flips_i(self):
        """data_symbol=-1 inverts the I channel."""
        iq_pos = generate_iq_epoch(1, 0, data_symbol=1)
        iq_neg = generate_iq_epoch(1, 0, data_symbol=-1)
        np.testing.assert_array_equal(iq_pos[:, 0], -iq_neg[:, 0])
        np.testing.assert_array_equal(iq_pos[:, 1], iq_neg[:, 1])

    def test_upsampled_rate(self):
        """Upsampled rate doubles the number of samples."""
        iq = generate_iq_epoch(1, 0, sample_rate=AFS_Q_CHIP_RATE * 2)
        assert iq.shape == (20460, 2)


class TestGenerateIqFrame:
    """Tests for generate_iq_frame (full 12 s frame)."""

    def test_output_shape(self):
        """Full frame at native rate: 6000 epochs x 10230 samples."""
        sb2 = np.zeros(1176, dtype=np.uint8)
        sb3 = np.zeros(846, dtype=np.uint8)
        sb4 = np.zeros(846, dtype=np.uint8)
        frame = frame_build(0, 0, sb2, sb3, sb4)
        iq = generate_iq_frame(1, frame)
        assert iq.shape == (6000 * 10230, 2)
        assert iq.dtype == np.float32

    def test_wrong_length_raises(self):
        """Frame with wrong length raises ValueError."""
        with pytest.raises(ValueError, match="Expected 6000"):
            generate_iq_frame(1, np.zeros(100, dtype=np.uint8))


class TestWriteIqFile:
    """Tests for write_iq_file (binary I/Q export)."""

    def test_header_size(self):
        """Output file has 128-byte header + correct data size."""
        import os

        iq = generate_iq_epoch(1, 0)
        with tempfile.NamedTemporaryFile(suffix=".iq", delete=False) as f:
            path = f.name
        try:
            write_iq_file(path, iq, AFS_Q_CHIP_RATE, 1)
            size = os.path.getsize(path)
            expected = 128 + 10230 * 2 * 4  # header + N * 2 channels * float32
            assert size == expected
        finally:
            os.unlink(path)

    def test_header_magic(self):
        """File starts with LSISIQ magic bytes."""
        iq = generate_iq_epoch(1, 0)
        with tempfile.NamedTemporaryFile(suffix=".iq", delete=False) as f:
            path = f.name
        try:
            write_iq_file(path, iq, AFS_Q_CHIP_RATE, 1)
            with open(path, "rb") as f:
                magic = f.read(8)
            assert magic == b"LSISIQ\x00\x00"
        finally:
            os.unlink(path)

    def test_wrong_shape_raises(self):
        """1D input raises ValueError."""
        with tempfile.NamedTemporaryFile(suffix=".iq", delete=False) as f:
            path = f.name
        try:
            with pytest.raises(ValueError, match="shape"):
                write_iq_file(path, np.zeros(100, dtype=np.float32), 1e6, 1)
        finally:
            os.unlink(path)

    def test_explicit_duration(self):
        """Explicit duration_s parameter is accepted."""
        iq = generate_iq_epoch(1, 0)
        with tempfile.NamedTemporaryFile(suffix=".iq", delete=False) as f:
            path = f.name
        try:
            write_iq_file(path, iq, AFS_Q_CHIP_RATE, 1, duration_s=0.002)
            assert os.path.getsize(path) == 128 + 10230 * 2 * 4
        finally:
            os.unlink(path)


class TestLdpcWrappers:
    """Tests for ldpc_encode/ldpc_decode Python wrappers."""

    def test_encode_sf2(self):
        """LDPC encode SF2 produces correct length."""
        from lunalink.afs import LdpcSubframe, ldpc_encode

        msg = np.zeros(1200, dtype=np.uint8)
        codeword = ldpc_encode(LdpcSubframe.SF2, msg)
        assert codeword.shape == (2400,)

    def test_decode_sf2_round_trip(self):
        """LDPC encode then decode recovers original data."""
        from lunalink.afs import LdpcSubframe, ldpc_decode, ldpc_encode

        msg = np.zeros(1200, dtype=np.uint8)
        codeword = ldpc_encode(LdpcSubframe.SF2, msg)
        soft = np.where(codeword == 0, 10.0, -10.0).astype(np.float32)
        decoded, status = ldpc_decode(LdpcSubframe.SF2, soft, iterations=20)
        np.testing.assert_array_equal(decoded[:1200], msg)


class TestSubframeBuilders:
    """Tests for subframe_build_sb2/sb3/sb4 Python wrappers."""

    def test_sb2_shape(self):
        """SB2 encoder produces correct output length."""
        data = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
        encoded = subframe_build_sb2(data)
        assert encoded.shape == (SB2_LENGTH,)

    def test_sb3_shape(self):
        """SB3 encoder produces correct output length."""
        data = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        encoded = subframe_build_sb3(data)
        assert encoded.shape == (SB3_LENGTH,)

    def test_sb4_shape(self):
        """SB4 encoder produces correct output length."""
        data = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
        encoded = subframe_build_sb4(data)
        assert encoded.shape == (SB4_LENGTH,)


class TestFrameDecode:
    """Tests for frame_decode Python wrapper."""

    def test_decode_clean_frame(self):
        """Encode then decode a frame over a perfect channel."""
        sb2 = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
        sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
        frame = frame_build(0, 42, sb2, sb3, sb4)
        # Convert to bipolar soft symbols: 0→+1.0, 1→-1.0
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        result, sb2_out, sb3_out, sb4_out, status = frame_decode(bipolar)
        assert status == FrameDecodeStatus.OK
        assert int(result.bch.fid) == 0
        assert int(result.bch.toi) == 42


class TestBlockDeinterleaveSoft:
    """Tests for block_deinterleave_soft Python wrapper."""

    def test_round_trip(self):
        """Interleave then deinterleave recovers original data."""
        from lunalink.afs._afs import block_interleave

        data = np.arange(5880, dtype=np.uint8) % 2
        interleaved = block_interleave(data)
        soft = np.where(interleaved == 0, 1.0, -1.0).astype(np.float32)
        deinterleaved = block_deinterleave_soft(soft)
        # Convert back to hard bits for comparison
        hard = (deinterleaved < 0).astype(np.uint8)
        np.testing.assert_array_equal(hard, data)


class TestCommitFrame:
    """Tests for commit_frame Python wrapper."""

    def test_valid_frame_passes(self):
        """A frame with valid CRC passes commit_frame."""
        from lunalink.afs._afs import crc24_calculate

        data = np.zeros(100, dtype=np.uint8)
        crc = crc24_calculate(data)
        buf = np.concatenate([data, crc])
        assert commit_frame(buf) == FrameStatus.OK

    def test_corrupted_frame_fails(self):
        """A frame with flipped bit fails commit_frame."""
        from lunalink.afs._afs import crc24_calculate

        data = np.zeros(100, dtype=np.uint8)
        crc = crc24_calculate(data)
        buf = np.concatenate([data, crc])
        buf[50] ^= 1
        assert commit_frame(buf) == FrameStatus.FAULT_DETECTED


class TestDefaultMatchedAssignment:
    """Tests for default_matched_assignment Python wrapper."""

    def test_prn1_assignment(self):
        """PRN 1 maps to S0, tertiary PRN 1, phase offset 0."""
        primary, secondary_idx, tertiary, phase = default_matched_assignment(1)
        assert primary == 1
        assert secondary_idx == 0  # S0
        assert tertiary == 1
        assert phase == 0

    def test_prn5_cycles(self):
        """PRN 5 cycles back to S0."""
        _, secondary_idx, _, _ = default_matched_assignment(5)
        assert secondary_idx == 0


class TestLegendre:
    """Tests for Legendre cache and checksum wrappers."""

    def test_cache_init(self):
        """Legendre cache initialises successfully."""
        from lunalink.afs import LegendreStatus

        status = legendre_cache_init()
        assert status == LegendreStatus.OK

    def test_10223_checksum(self):
        """Legendre-10223 checksum is nonzero and stable."""
        c1 = legendre_10223_checksum()
        c2 = legendre_10223_checksum()
        assert c1 != 0
        assert c1 == c2

    def test_1499_checksum(self):
        """Legendre-1499 checksum is nonzero and stable."""
        c1 = legendre_1499_checksum()
        c2 = legendre_1499_checksum()
        assert c1 != 0
        assert c1 == c2


# ─── Gateway 6: Message Parsing ──────────────────────────────────────────────


class TestParseSb2:
    """Tests for parse_sb2 Python wrapper."""

    @staticmethod
    def _pack_sb2(wn: int, itow: int) -> np.ndarray:
        """Pack WN (13 bits) + ITOW (9 bits) into SB2 data array."""
        sb2 = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
        for i in range(WN_BITS):
            sb2[i] = (wn >> (WN_BITS - 1 - i)) & 1
        for i in range(ITOW_BITS):
            sb2[WN_BITS + i] = (itow >> (ITOW_BITS - 1 - i)) & 1
        return sb2

    def test_basic_extraction(self):
        """Extract known WN and ITOW values."""
        sb2 = self._pack_sb2(1234, 456)
        result = parse_sb2(sb2)
        assert result.status == MessageParseStatus.OK
        assert result.wn == 1234
        assert result.itow == 456

    def test_boundary_values(self):
        """Extract maximum WN=8191 and ITOW=503."""
        sb2 = self._pack_sb2(WN_MAX, ITOW_MAX)
        result = parse_sb2(sb2)
        assert result.status == MessageParseStatus.OK
        assert result.wn == WN_MAX
        assert result.itow == ITOW_MAX

    def test_zero_values(self):
        """WN=0, ITOW=0 from all-zero data."""
        sb2 = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
        result = parse_sb2(sb2)
        assert result.status == MessageParseStatus.OK
        assert result.wn == 0
        assert result.itow == 0

    def test_wrong_size_rejected(self):
        """Wrong input size returns INVALID_INPUT."""
        too_small = np.zeros(100, dtype=np.uint8)
        result = parse_sb2(too_small)
        assert result.status == MessageParseStatus.INVALID_INPUT

    def test_float_dtype_rejected(self):
        """Non-uint8 dtype is rejected, not silently coerced."""
        sb2_float = np.zeros(SB2_DATA_BITS, dtype=np.float32)
        sb2_float[0] = 1.9  # would truncate to 1 if coerced
        with pytest.raises(TypeError):
            parse_sb2(sb2_float)

    def test_data_offset_and_payload_size(self):
        """data_offset and payload_size cover the full SB2 data region."""
        sb2 = self._pack_sb2(0, 0)
        result = parse_sb2(sb2)
        assert result.data_offset == TOT_HEADER_BITS
        assert result.payload_size == SB2_DATA_BITS - TOT_HEADER_BITS
        # payload_offset + payload_size should not exceed SB2 total
        assert result.data_offset + result.payload_size == SB2_DATA_BITS

    def test_payload_extraction(self):
        """Raw payload bytes are accessible via offset + size."""
        sb2 = self._pack_sb2(100, 42)
        # Put a recognisable pattern in the payload region
        sb2[TOT_HEADER_BITS] = 1
        sb2[TOT_HEADER_BITS + 1] = 0
        sb2[TOT_HEADER_BITS + 2] = 1
        result = parse_sb2(sb2)
        assert result.status == MessageParseStatus.OK
        # Caller slices: sb2[data_offset : data_offset + payload_size]
        payload = sb2[result.data_offset : result.data_offset + result.payload_size]
        assert len(payload) == result.payload_size
        assert payload[0] == 1
        assert payload[1] == 0
        assert payload[2] == 1


class TestParseSb3:
    """Tests for parse_sb3 Python wrapper."""

    @staticmethod
    def _pack_type(type_val: int) -> np.ndarray:
        """Pack a type field into SB3 data."""
        sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        for i in range(SUBFRAME_TYPE_BITS):
            sb3[i] = (type_val >> (SUBFRAME_TYPE_BITS - 1 - i)) & 1
        return sb3

    def test_known_type(self):
        """Type=5 maps to G5 (Orbit Almanac)."""
        sb3 = self._pack_type(5)
        result = parse_sb3(sb3)
        assert result.status == MessageParseStatus.OK
        assert result.type == MessageType.G5
        assert result.type_raw == 5

    def test_unknown_type(self):
        """Unknown type returns OK with UNKNOWN."""
        sb3 = self._pack_type(63)
        result = parse_sb3(sb3)
        assert result.status == MessageParseStatus.OK
        assert result.type == MessageType.UNKNOWN

    def test_wrong_size_rejected(self):
        """Wrong size returns INVALID_INPUT."""
        result = parse_sb3(np.zeros(10, dtype=np.uint8))
        assert result.status == MessageParseStatus.INVALID_INPUT

    def test_payload_offset_and_size(self):
        """Payload covers bits after the type field."""
        sb3 = self._pack_type(5)
        result = parse_sb3(sb3)
        assert result.data_offset == SUBFRAME_TYPE_BITS
        assert result.payload_size == SB3_DATA_BITS - SUBFRAME_TYPE_BITS
        payload = sb3[result.data_offset : result.data_offset + result.payload_size]
        assert len(payload) == result.payload_size


class TestParseSb4:
    """Tests for parse_sb4 Python wrapper."""

    def test_known_type(self):
        """Type=1 maps to G1 (Network Access Info)."""
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
        for i in range(SUBFRAME_TYPE_BITS):
            sb4[i] = (1 >> (SUBFRAME_TYPE_BITS - 1 - i)) & 1
        result = parse_sb4(sb4)
        assert result.status == MessageParseStatus.OK
        assert result.type == MessageType.G1

    def test_wrong_size_rejected(self):
        """Wrong size returns INVALID_INPUT."""
        result = parse_sb4(np.zeros(10, dtype=np.uint8))
        assert result.status == MessageParseStatus.INVALID_INPUT

    def test_payload_offset_and_size(self):
        """Payload covers bits after the type field."""
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
        for i in range(SUBFRAME_TYPE_BITS):
            sb4[i] = (1 >> (SUBFRAME_TYPE_BITS - 1 - i)) & 1
        result = parse_sb4(sb4)
        assert result.data_offset == SUBFRAME_TYPE_BITS
        assert result.payload_size == SB4_DATA_BITS - SUBFRAME_TYPE_BITS


class TestComputeTot:
    """Tests for compute_tot Python wrapper."""

    def test_all_zeros(self):
        """t_F = 0 when all inputs are zero."""
        result = compute_tot(0, 0, 0)
        assert result.status == TotStatus.OK
        assert result.t_f == 0.0

    def test_basic_formula(self):
        """WN=1, ITOW=2, TOI=5 → t_F = 607260."""
        result = compute_tot(1, 2, 5)
        assert result.status == TotStatus.OK
        assert result.t_f == 607260.0

    def test_boundary_values(self):
        """Maximum valid inputs."""
        result = compute_tot(WN_MAX, ITOW_MAX, TOT_TOI_MAX)
        assert result.status == TotStatus.OK
        expected = (
            WN_MAX * SEC_WEEK
            + ITOW_MAX * BLOCK_INTERVAL_DURATION
            + TOT_TOI_MAX * FRAME_DURATION_SEC
        )
        assert result.t_f == float(expected)

    def test_with_delta(self):
        """Delta offset is added."""
        result = compute_tot(0, 0, 0, 1.5)
        assert result.status == TotStatus.OK
        assert result.t_f == 1.5

    def test_invalid_wn(self):
        """WN > 8191 rejected."""
        result = compute_tot(8192, 0, 0)
        assert result.status == TotStatus.INVALID_WN

    def test_invalid_itow(self):
        """ITOW > 503 rejected."""
        result = compute_tot(0, 504, 0)
        assert result.status == TotStatus.INVALID_ITOW

    def test_invalid_toi(self):
        """TOI > 99 rejected."""
        result = compute_tot(0, 0, 100)
        assert result.status == TotStatus.INVALID_TOI

    def test_nan_delta_rejected(self):
        """NaN delta_t_lrt rejected."""
        result = compute_tot(0, 0, 0, float("nan"))
        assert result.status == TotStatus.INVALID_DELTA
        assert result.t_f == 0.0

    def test_inf_delta_rejected(self):
        """Inf delta_t_lrt rejected."""
        result = compute_tot(0, 0, 0, float("inf"))
        assert result.status == TotStatus.INVALID_DELTA

    def test_week_consistency(self):
        """Last frame of week + 12s = first frame of next week."""
        last = compute_tot(0, ITOW_MAX, TOT_TOI_MAX)
        first = compute_tot(1, 0, 0)
        assert last.status == TotStatus.OK
        assert first.status == TotStatus.OK
        assert first.t_f - last.t_f == float(FRAME_DURATION_SEC)

    def test_e2e_round_trip(self):
        """Full pipeline: build → decode → parse → ToT → verify time."""
        wn, itow, toi_val = 100, 42, 17
        sb2 = TestParseSb2._pack_sb2(wn, itow)
        sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)

        frame = frame_build(0, toi_val, sb2, sb3, sb4)
        bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)
        result, sb2_out, sb3_out, sb4_out, status = frame_decode(bipolar)
        assert status == FrameDecodeStatus.OK

        sb2_parsed = parse_sb2(sb2_out)
        assert sb2_parsed.status == MessageParseStatus.OK
        assert sb2_parsed.wn == wn
        assert sb2_parsed.itow == itow

        tot = compute_tot(sb2_parsed.wn, sb2_parsed.itow, toi_val)
        assert tot.status == TotStatus.OK
        expected = (
            wn * SEC_WEEK
            + itow * BLOCK_INTERVAL_DURATION
            + toi_val * FRAME_DURATION_SEC
        )
        assert tot.t_f == float(expected)
