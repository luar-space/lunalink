# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Performance and roundtrip tests for the Python AFS module."""

import time

import numpy as np

from lunalink.afs import (
    LdpcStatus,
    LdpcSubframe,
    LdpcWorkspace,
    bch_decode,
    bch_encode,
    ldpc_decode,
    ldpc_encode,
)


def test_ldpc_python_roundtrip():
    """Verify that Python-side LDPC encoding/decoding works correctly."""
    # Generate random 1200-bit message
    msg = np.random.randint(0, 2, 1200, dtype=np.uint8)

    # Encode
    encoded = ldpc_encode(LdpcSubframe.SF2, msg)
    assert len(encoded) == 2400

    # Simulate AWGN (very light noise for roundtrip)
    soft = np.where(encoded == 1, -1.0, 1.0).astype(np.float32)

    # Decode
    ws = LdpcWorkspace()
    decoded, status = ldpc_decode(LdpcSubframe.SF2, soft, iterations=50, workspace=ws)

    assert status == LdpcStatus.OK
    np.testing.assert_array_equal(msg, decoded)


def test_ldpc_python_performance():
    """Measure performance of LDPC decoding from Python."""
    msg = np.random.randint(0, 2, 1200, dtype=np.uint8)
    encoded = ldpc_encode(LdpcSubframe.SF2, msg)
    soft = np.where(encoded == 1, -1.0, 1.0).astype(np.float32)
    ws = LdpcWorkspace()

    start = time.perf_counter()
    iters = 10
    for _ in range(iters):
        ldpc_decode(LdpcSubframe.SF2, soft, iterations=50, workspace=ws)
    end = time.perf_counter()

    avg_ms = ((end - start) / iters) * 1000
    print(f"\n[Python Performance] LDPC Decode (SF2, 50 iter): {avg_ms:.2f} ms")
    # Benchmark: Even with Python overhead, should be < 500ms
    assert avg_ms < 500


def test_bch_python_performance():
    """Measure performance of BCH decoding from Python."""
    codeword = bch_encode(0, 69)

    start = time.perf_counter()
    iters = 1000
    for _ in range(iters):
        bch_decode(codeword)
    end = time.perf_counter()

    avg_us = ((end - start) / iters) * 1_000_000
    print(f"\n[Python Performance] BCH Decode: {avg_us:.2f} us")
    assert avg_us < 100  # Microseconds
