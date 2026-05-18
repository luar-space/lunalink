"""Generate Level 3 interoperability test vectors (baseband I/Q signals).

Reads ``frame_*.bin`` files from the public ``lsis-afs-test-vectors``
repository's ``frames/`` directory, modulates each frame to baseband
I/Q at the L3-baseline 10.23 MHz sample rate, and writes one
``signal_<case>_12s.iq.gz`` per output case in the interoperability.pdf
§"Signal Export Format" shape (128-byte ``LSISIQ\\0\\0`` header +
interleaved float32 I/Q samples, gzipped with ``mtime=0`` for
byte-stable output).

Ten signals are produced, mirroring the public repo's L3 set:

  - ``signal_message_{1..5}_12s.iq.gz`` — messages 1-5 modulated at PRN 1
  - ``signal_boundary_at_prn12_12s.iq.gz`` — TC4 boundary at PRN 12
  - ``signal_boundary_max_fields_at_prn12_12s.iq.gz`` — TC4 SB2 maxima at PRN 12
  - ``signal_prn{2,3,12}_baseline_12s.iq.gz`` — all-zeros baseline at PRN 2/3/12

The boundary cases use PRN 12 (the maximum interim test code per
interoperability.pdf Test Case 2 / Table 11) rather than the LSIS-spec
maximum PRN 210 used in L2: only PRNs 1-12 have spreading codes in the
V1.0 interim Annex 3, and ``generate_iq_frame`` requires a code-bearing
PRN.  The baseline trio re-modulates the message-1 frame symbols at
three additional PRNs to validate the L1↔L3 trust chain (codes
generate correctly + modulator wires them to the right PRN).

Output is byte-stable across rebuilds: ``compresslevel=9`` (matches the
v0.3.0 publication's deflate stream) and ``mtime=0`` (deterministic
gzip header).  The v0.3.0 publication itself used wall-clock ``mtime``,
so the wrapper bytes of the published ``.iq.gz`` files will NOT match
this script's output — but the *uncompressed* LSISIQ-headered I/Q
payload reproduces bit-exactly, which is what L3 interop testing is
about.  Consumers should compare the gunzipped content, not the gzip
wrapper.

Usage:
    python scripts/export_test_vectors_l3.py <output_dir> [--input-dir <frames_dir>]

Defaults to reading from ``../lsis-afs-test-vectors/frames/`` (sibling
checkout) and writing to ``<output_dir>`` which is typically the
``signals/`` subdirectory of the same checkout.

Companion to ``scripts/export_test_vectors_l2.py`` (frame producer).
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import struct
import sys
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from lunalink.afs.signal import FRAME_LENGTH, generate_iq_frame

# Common interop-PDF L3 baseline (page 2): "Sample rate: 10.23 MHz".
L3_SAMPLE_RATE_HZ = 10_230_000.0

# (output_filename, source_frame_filename, prn_id)
TEST_CASES: list[tuple[str, str, int]] = [
    ("signal_message_1_12s.iq.gz", "frame_message_1.bin", 1),
    ("signal_message_2_12s.iq.gz", "frame_message_2.bin", 1),
    ("signal_message_3_12s.iq.gz", "frame_message_3.bin", 1),
    ("signal_message_4_12s.iq.gz", "frame_message_4.bin", 1),
    ("signal_message_5_12s.iq.gz", "frame_message_5.bin", 1),
    ("signal_boundary_at_prn12_12s.iq.gz", "frame_boundary.bin", 12),
    (
        "signal_boundary_max_fields_at_prn12_12s.iq.gz",
        "frame_boundary_max_fields.bin",
        12,
    ),
    ("signal_prn2_baseline_12s.iq.gz", "frame_message_1.bin", 2),
    ("signal_prn3_baseline_12s.iq.gz", "frame_message_1.bin", 3),
    ("signal_prn12_baseline_12s.iq.gz", "frame_message_1.bin", 12),
]


def _load_frame_syms(path: Path) -> NDArray[np.uint8]:
    """Read a 6064-byte LSISAFS frame.bin and return its 6000-symbol payload."""
    data = path.read_bytes()
    if len(data) != 6064:
        raise ValueError(f"{path.name}: expected 6064 bytes, got {len(data)}")
    if data[:8] != b"LSISAFS\x00":
        raise ValueError(f"{path.name}: missing LSISAFS magic")
    syms = np.frombuffer(data[64:], dtype=np.uint8).copy()
    if syms.shape[0] != FRAME_LENGTH:
        raise ValueError(
            f"{path.name}: payload {syms.shape[0]} != {FRAME_LENGTH} symbols"
        )
    if syms.max(initial=0) > 1:
        raise ValueError(f"{path.name}: payload has non-binary symbols")
    return syms


def _build_lsisiq_payload(iq: NDArray[np.float32], prn: int) -> bytes:
    """Build 128-byte LSISIQ header + interleaved float32 I/Q bytes.

    Header layout per interoperability.pdf §"Signal Export Format" and
    ``write_iq_file`` (lunalink.afs.signal): magic(8) + version(4) +
    sample_rate(8) + duration(8) + prn(4) + format(16) + reserved(80).
    """
    n_samples = iq.shape[0]
    duration_s = n_samples / L3_SAMPLE_RATE_HZ
    buf = io.BytesIO()
    buf.write(b"LSISIQ\x00\x00")
    buf.write(struct.pack("<I", 1))  # version
    buf.write(struct.pack("<d", L3_SAMPLE_RATE_HZ))
    buf.write(struct.pack("<d", duration_s))
    buf.write(struct.pack("<I", prn))
    buf.write(b"float32" + b"\x00" * 9)  # 16-byte zero-padded format
    buf.write(b"\x00" * (128 - buf.tell()))
    return buf.getvalue() + iq.astype(np.float32).ravel().tobytes()


def _gzip_write(path: Path, content: bytes) -> None:
    """Write ``content`` as gzip with compresslevel=9, mtime=0, FNAME embedded.

    ``compresslevel=9`` matches the v0.3.0 publication's deflate stream
    (XFL=2 in the gzip header).  ``mtime=0`` makes the wrapper
    deterministic across rebuilds.  The FNAME slot embeds the ``.iq``
    basename via ``filename=`` so the gzip header is fully pinned.
    """
    iq_basename = path.name.removesuffix(".gz")
    with open(path, "wb") as f:
        with gzip.GzipFile(
            filename=iq_basename, fileobj=f, mode="wb", compresslevel=9, mtime=0
        ) as gz:
            gz.write(content)


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        help="Target directory (typically the signals/ subdir of lsis-afs-test-vectors)",
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

    for out_name, frame_filename, prn in TEST_CASES:
        in_path = args.input_dir / frame_filename
        if not in_path.is_file():
            print(f"  SKIP  {out_name:<48}  input not found: {frame_filename}", file=sys.stderr)
            continue

        syms = _load_frame_syms(in_path)
        iq = generate_iq_frame(prn, syms, sample_rate=L3_SAMPLE_RATE_HZ)
        payload = _build_lsisiq_payload(iq, prn)

        out_path = out_dir / out_name
        _gzip_write(out_path, payload)

        size = out_path.stat().st_size
        sha = _sha256(out_path)[:16]
        print(
            f"  {out_name:<48}  PRN={prn:>2}  ({size} bytes, sha256={sha}…)",
            file=sys.stderr,
        )

    print(f"OK: {len(TEST_CASES)} signals in {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
