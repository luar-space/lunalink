#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Decode an AFS frame from soft-decision symbols."""

import numpy as np

from lunalink.afs import (
    SyncNotFoundError,
    frame_build,
    make_test_payload,
    receive_frame,
)

# Build a frame to decode (in practice, these symbols come from a receiver)
sb2, sb3, sb4 = make_test_payload("zeros")
frame = frame_build(0, 42, sb2, sb3, sb4)

# Convert to bipolar soft symbols (perfect channel)
bipolar = np.where(frame == 0, 1.0, -1.0).astype(np.float32)

# Decode — one call, structured result
try:
    result = receive_frame(bipolar)
except SyncNotFoundError:
    print("No sync pattern found!")
    raise SystemExit(1)

print(f"FID={result.fid}  TOI={result.toi}")
for name, sb in [("SB2", result.sb2), ("SB3", result.sb3), ("SB4", result.sb4)]:
    ldpc = "OK" if sb.ldpc_converged else "FAIL"
    crc = "OK" if sb.crc_ok else "FAIL"
    print(f"{name}: LDPC={ldpc}  CRC={crc}")

if result.time:
    t = result.time
    print(f"Time of Transmission: {t.t_f:.1f}s (WN={t.wn} ITOW={t.itow})")

print(f"All OK: {result.all_ok}")
