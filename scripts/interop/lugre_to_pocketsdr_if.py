#!/usr/bin/env python3
"""G-06.2 — LuGRE IQS → PocketSDR INT8X2 raw IF file converter.

PocketSDR's `pocket_acq` reads digital IF data as a flat stream of samples;
for complex sampling (`-fmt INT8X2`) it expects interleaved int8 I/Q pairs.
LuGRE's IQS format packs 4-bit two's-complement I/Q into a single byte with
a binary header plus SDRX XML metadata. This script bridges the two by
reusing `lunalink.receiver.lugre.load_lugre_iq` and scaling from the 4-bit
[-8, 7] range up to the full int8 [-127, 127] range.

Usage:
    python scripts/interop/lugre_to_pocketsdr_if.py <iqs.bin> <out.if>
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT / "src") not in sys.path:
    sys.path.insert(0, str(REPO_ROOT / "src"))


def convert(iqs_path: Path, out_path: Path) -> tuple[float, float]:
    """Convert a LuGRE IQS bin to a PocketSDR INT8X2 IF file.

    Returns
    -------
    (sample_rate_hz, center_freq_hz)
        Metadata the caller needs to pass to `pocket_acq` via -f / -fi.
    """
    from lunalink.receiver.lugre import load_lugre_iq

    meta, samples = load_lugre_iq(iqs_path)

    # PocketSDR's internal sample type is `sdr_cpx8_t` — a single byte holding
    # 4-bit signed I in the low nibble and 4-bit signed Q in the high nibble
    # (see `pocket_sdr.h` `SDR_CPX8` macro). When `sdr_read_data` reads an
    # INT8X2 file it immediately re-packs each sample pair through that macro,
    # which truncates any I or Q outside [-8, 7]. LuGRE's 4-bit samples already
    # sit in [-8, 7] natively — no scaling applied. An earlier draft of this
    # script multiplied by 16 for "headroom"; that made every repacked sample
    # collapse to zero (low nibble of an even-scaled value is 0) and
    # pocket_acq returned C/N0 = 0 for every PRN. Leave as-is.
    i = np.clip(np.round(samples.real), -8, 7).astype(np.int8)
    q = np.clip(np.round(samples.imag), -8, 7).astype(np.int8)

    interleaved = np.empty(2 * len(i), dtype=np.int8)
    interleaved[0::2] = i
    interleaved[1::2] = q
    interleaved.tofile(out_path)

    return meta.sample_rate_hz, meta.center_freq_hz


def main() -> None:
    """CLI entry point — parse args and run the IQS→INT8X2 conversion."""
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("iqs_bin", type=Path)
    p.add_argument("out_if", type=Path)
    args = p.parse_args()

    fs, fc = convert(args.iqs_bin, args.out_if)
    print(f"OK: wrote {args.out_if} (fs={fs / 1e6:.3f} MHz, fc={fc / 1e6:.2f} MHz)")


if __name__ == "__main__":
    main()
