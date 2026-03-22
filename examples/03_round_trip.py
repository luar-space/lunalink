#!/usr/bin/env python3
"""End-to-end: transmit → perfect channel → decode → verify."""

import numpy as np

from lunalink.afs import receive_frame, make_test_payload, transmit_frame

# Transmit
sb2, sb3, sb4 = make_test_payload("random", seed=42)
frame, iq = transmit_frame(
    prn_id=1, fid=2, toi=77,
    sb2_data=sb2, sb3_data=sb3, sb4_data=sb4,
)
print(f"TX: PRN={frame.prn_id} FID={frame.fid} TOI={frame.toi}")
print(f"    {iq.shape[0]:,} I/Q samples")

# Perfect channel (bipolar from frame symbols, no noise)
bipolar = np.where(frame.symbols == 0, 1.0, -1.0).astype(np.float32)

# Decode
result = receive_frame(bipolar)

# Verify
assert result.all_ok, "Decode failed!"
assert result.fid == frame.fid
assert result.toi == frame.toi
np.testing.assert_array_equal(result.sb2.data, sb2)
np.testing.assert_array_equal(result.sb3.data, sb3)
np.testing.assert_array_equal(result.sb4.data, sb4)

print(f"RX: FID={result.fid} TOI={result.toi}")
if result.time:
    print(f"    ToT={result.time.t_f:.1f}s")
print("Round-trip PASSED — all subframe data matches bit-exact.")
