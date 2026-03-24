#!/usr/bin/env python3
"""Generate a 12-second AFS I/Q signal file in 3 lines."""

from lunalink.afs import (
    AFS_Q_CHIP_RATE,
    make_test_payload,
    transmit_frame,
    write_iq_file,
)

# 1. Create test payload (all-zeros navigation data)
sb2, sb3, sb4 = make_test_payload("zeros")

# 2. Build frame and generate I/Q samples
frame, iq = transmit_frame(
    prn_id=1,
    fid=0,
    toi=42,
    sb2_data=sb2,
    sb3_data=sb3,
    sb4_data=sb4,
)

# 3. Write to file with LSISIQ header
write_iq_file("signal.iq", iq, AFS_Q_CHIP_RATE, prn_id=1)

print(f"Wrote {iq.shape[0]:,} I/Q samples ({frame.symbols.shape[0]} symbols)")
print(f"  PRN={frame.prn_id}  FID={frame.fid}  TOI={frame.toi}")
print(f"  File: signal.iq ({iq.nbytes / 1e6:.1f} MB)")
