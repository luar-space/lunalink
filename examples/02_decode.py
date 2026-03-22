#!/usr/bin/env python3
"""Decode an AFS frame from soft-decision symbols."""

import numpy as np

from lunalink.afs import SyncNotFoundError, receive_frame, frame_build, make_test_payload

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
print(f"SB2: LDPC={'OK' if result.sb2.ldpc_converged else 'FAIL'}  CRC={'OK' if result.sb2.crc_ok else 'FAIL'}")
print(f"SB3: LDPC={'OK' if result.sb3.ldpc_converged else 'FAIL'}  CRC={'OK' if result.sb3.crc_ok else 'FAIL'}")
print(f"SB4: LDPC={'OK' if result.sb4.ldpc_converged else 'FAIL'}  CRC={'OK' if result.sb4.crc_ok else 'FAIL'}")

if result.time:
    print(f"Time of Transmission: {result.time.t_f:.1f}s (WN={result.time.wn} ITOW={result.time.itow})")

print(f"All OK: {result.all_ok}")
