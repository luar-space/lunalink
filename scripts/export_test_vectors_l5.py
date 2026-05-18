"""Generate Level 5 interoperability test vectors (parsed JSON).

Reads ``frame_*.bin`` files from the public ``lsis-afs-test-vectors``
repository's ``frames/`` directory, runs the LunaLink RX path on each
(sync detect → BCH SB1 → deinterleave → LDPC SB2/SB3/SB4 → CRC → parse),
and writes one ``parsed_<frame_id>.json`` per input in the
interoperability.pdf §"Parsed Data Export Format" shape.

LSIS V1.0 honesty: only fields V1.0 actually pins (FID/TOI per Tables
13/14; WN/ITOW per Table 22; ToT per §2.5.5; CRC-24Q per §2.4.3.1.3)
are emitted as concrete values.  CED, Health, Time Conversions, SF3
and SF4 data layouts are V1.0-TBW; those are surfaced as raw bit-slices
with explicit ``_lsis_ref`` markers so consumers can byte-equal-compare
without inventing positions V1.0 doesn't pin.

Seven frames are produced, mirroring the public repo's set:

  - ``parsed_frame_message_1..5.json`` — Test Messages 1-5 (TC1)
  - ``parsed_frame_boundary.json`` — TC4 (FID=3, TOI=99 maxima)
  - ``parsed_frame_boundary_max_fields.json`` — TC4 SB2 maxima (WN=8191, ITOW=503)

Usage:
    python scripts/export_test_vectors_l5.py <output_dir> [--input-dir <frames_dir>]

Defaults to reading from ``../lsis-afs-test-vectors/frames/`` (sibling
checkout) and writing to ``--output-dir`` which is typically the
``parsed/`` subdirectory of the same checkout.

Companion to ``scripts/export_test_vectors_l2.py`` (frame producer).
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

import numpy as np

from lunalink.afs.vectors import export_parsed_json_interop_v1
from lunalink.afs._receiver import receive_frame

# Mirrors TEST_MESSAGES in scripts/export_test_vectors_l2.py.
# (frame_filename, frame_id, description)
TEST_MESSAGES: list[tuple[str, str, str]] = [
    ("frame_message_1.bin", "frame_message_1", "Test Message 1 — all-zeros baseline"),
    ("frame_message_2.bin", "frame_message_2", "Test Message 2 — all-ones inverse baseline"),
    ("frame_message_3.bin", "frame_message_3", "Test Message 3 — alternating pattern (0xAA…, MSB-first)"),
    ("frame_message_4.bin", "frame_message_4", "Test Message 4 — bytewise marker pattern"),
    ("frame_message_5.bin", "frame_message_5", "Test Message 5 — xorshift32 PRNG (seed 0xAF52)"),
    ("frame_boundary.bin", "frame_boundary", "Test Case 4 — FID=3, TOI=99, PRN=210 maxima"),
    ("frame_boundary_max_fields.bin", "frame_boundary_max_fields", "Test Case 4 — WN=8191, ITOW=503 SB2 maxima"),
]


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _load_frame_bin(path: Path) -> tuple[np.ndarray, int]:
    """Read a 6064-byte LSISAFS frame.bin, return (6000-symbol uint8, prn)."""
    data = path.read_bytes()
    if len(data) != 6064:
        raise ValueError(f"{path.name}: expected 6064 bytes, got {len(data)}")
    if data[:8] != b"LSISAFS\x00":
        raise ValueError(f"{path.name}: missing LSISAFS magic")
    (version, frame_length, prn) = struct.unpack_from("<III", data, 8)
    if version != 1:
        raise ValueError(f"{path.name}: header version {version} != 1")
    if frame_length != 6000:
        raise ValueError(f"{path.name}: header frame_length {frame_length} != 6000")
    syms = np.frombuffer(data[64:], dtype=np.uint8)
    if syms.shape[0] != 6000:
        raise ValueError(f"{path.name}: payload {syms.shape[0]} != 6000 symbols")
    if syms.max(initial=0) > 1:
        raise ValueError(f"{path.name}: payload has non-binary symbols")
    return syms, prn


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        help="Target directory (typically the parsed/ subdir of lsis-afs-test-vectors)",
    )
    ap.add_argument(
        "--input-dir",
        type=Path,
        default=None,
        help="Directory containing the input frame_*.bin files. "
        "Defaults to ../lsis-afs-test-vectors/frames/ relative to the lunalink repo root.",
    )
    args = ap.parse_args(argv)

    if args.input_dir is None:
        repo_root = Path(__file__).resolve().parents[1]
        args.input_dir = repo_root.parent / "lsis-afs-test-vectors" / "frames"

    if not args.input_dir.is_dir():
        print(
            f"error: input directory not found: {args.input_dir}",
            file=sys.stderr,
        )
        return 2

    out_dir: Path = args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    failures = 0
    for filename, frame_id, description in TEST_MESSAGES:
        in_path = args.input_dir / filename
        if not in_path.is_file():
            print(f"  SKIP  {filename:<32}  not found", file=sys.stderr)
            failures += 1
            continue

        syms, prn = _load_frame_bin(in_path)
        sha = _sha256(in_path)

        # Convert hard-decision binary symbols (0/1) to bipolar soft symbols
        # (+1.0 = bit 0, -1.0 = bit 1) per receive_frame's contract.
        bipolar = np.where(syms == 0, 1.0, -1.0).astype(np.float32)

        # Reference-implementation test vectors must never ship with a
        # failed CRC: any CRC error here means an encode/decode/CRC
        # regression that should break the build, not be silently
        # downgraded to a warning.  raise_on_error=True surfaces a
        # decode-stage error as a hard exception; the post-decode
        # all_ok check guards against any remaining CRC mismatch.
        result = receive_frame(bipolar, raise_on_error=True)
        if not result.all_ok:
            sb_status = (
                f"sb2_crc={result.sb2.crc_ok} sb3_crc={result.sb3.crc_ok} "
                f"sb4_crc={result.sb4.crc_ok}"
            )
            raise RuntimeError(
                f"{filename}: CRC failure during decode ({sb_status}); "
                "reference test vectors must round-trip cleanly. "
                "Investigate encode/decode/CRC for regression."
            )

        out_path = out_dir / f"parsed_{frame_id}.json"
        export_parsed_json_interop_v1(
            result,
            out_path,
            frame_id=frame_id,
            input_frame_sha256=sha,
            description=description,
        )

        size = out_path.stat().st_size
        out_sha = _sha256(out_path)[:16]
        print(
            f"  {out_path.name:<40}  PRN={prn:>3}  FID={result.fid}  "
            f"TOI={result.toi:>3}  ({size} bytes, sha256={out_sha}…)",
            file=sys.stderr,
        )

    print(
        f"OK: {len(TEST_MESSAGES) - failures}/{len(TEST_MESSAGES)} parsed JSONs "
        f"in {out_dir}",
        file=sys.stderr,
    )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
