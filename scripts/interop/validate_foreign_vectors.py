#!/usr/bin/env python3
"""I6 — Validate test vectors from any LSIS-AFS implementation.

Checks format, dimensions, and optionally decodes foreign test vectors
against LunaLink's signal chain. Designed for cross-team interoperability
at the ESA-CCSDS competition.

Usage:
    python scripts/interop/validate_foreign_vectors.py <vector_dir>

Expects vectors in any combination of:
    codes/       — spreading code files (.hex, .bin)
    frames/      — encoded frame files (.bin, 6000 symbols)
    signals/     — I/Q signal files (.bin/.iq/.iq.gz, int16 or float32)
    parsed/      — parsed message metadata (.json/.json.gz)
    decode/      — decode-direction vectors (.json/.json.gz)

Files may be gzip-compressed (.gz suffix).  Compression is transparent.

Returns exit code 0 if all checks pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import gzip
import json
import sys
from pathlib import Path

import numpy as np

GOLD_CODE_LENGTH = 2046
WEIL_PRIMARY_LENGTH = 10230
FRAME_LENGTH = 6000

# File extensions recognised per category (with and without .gz)
CODE_EXTS = {".bin", ".hex"}
FRAME_EXTS = {".bin"}
IQ_EXTS = {".bin", ".iq"}
JSON_EXTS = {".json"}


def _read_bytes(path: Path) -> bytes:
    """Read file contents, transparently decompressing .gz."""
    if path.suffix == ".gz":
        with gzip.open(path, "rb") as f:
            return f.read()
    return path.read_bytes()


def _stem_and_ext(path: Path) -> tuple[str, str]:
    """Return (stem, inner_extension) handling double extensions like .iq.gz."""
    if path.suffix == ".gz":
        inner = Path(path.stem)  # e.g. "foo.iq" or "foo.json"
        return inner.stem, inner.suffix
    return path.stem, path.suffix


def _check_code_file(path: Path) -> list[str]:
    """Validate a spreading code file (.bin raw uint8 or .hex sectioned ASCII)."""
    errors = []
    _, ext = _stem_and_ext(path)
    raw = _read_bytes(path)

    if ext == ".hex":
        # Sectioned ASCII format: [SECTION]\nlength: N\nhex: ABCDEF...\n
        # Count total chips across all sections
        total_chips = 0
        for line in raw.decode("ascii", errors="replace").splitlines():
            line = line.strip()
            if line.startswith("length:"):
                total_chips += int(line.split(":")[1].strip())
        if total_chips > 0:
            print(f"  {path.name}: hex code file, {total_chips} total chips, OK")
        else:
            errors.append(f"{path.name}: no 'length:' fields found in hex file")
        return errors

    # Raw binary: uint8 chips
    data = np.frombuffer(raw, dtype=np.uint8)

    if len(data) == GOLD_CODE_LENGTH:
        code_type = "Gold"
    elif len(data) == WEIL_PRIMARY_LENGTH:
        code_type = "Weil primary"
    else:
        errors.append(
            f"{path.name}: unexpected length {len(data)} "
            f"(expected {GOLD_CODE_LENGTH} or {WEIL_PRIMARY_LENGTH})"
        )
        return errors

    non_binary = np.sum((data != 0) & (data != 1))
    if non_binary > 0:
        errors.append(
            f"{path.name}: {non_binary} non-binary chip values (expected only 0 and 1)"
        )
    else:
        print(f"  {path.name}: {code_type} code, {len(data)} chips, OK")
    return errors


LSISAFS_HEADER_SIZE = 64  # "LSISAFS\x00" + metadata


def _check_frame_file(path: Path) -> list[str]:
    """Validate an encoded frame file (6000 uint8 symbols, optional 64-byte header)."""
    errors = []
    raw = _read_bytes(path)
    data = np.frombuffer(raw, dtype=np.uint8)

    # Strip LSISAFS interop header if present
    if len(data) == FRAME_LENGTH + LSISAFS_HEADER_SIZE and raw[:7] == b"LSISAFS":
        data = data[LSISAFS_HEADER_SIZE:]

    if len(data) != FRAME_LENGTH:
        errors.append(f"{path.name}: length {len(data)}, expected {FRAME_LENGTH}")
        return errors

    non_binary = np.sum((data != 0) & (data != 1))
    if non_binary > 0:
        errors.append(f"{path.name}: {non_binary} non-binary symbol values")
        return errors

    # Check sync pattern (first 68 symbols)
    sync_bytes = bytes([0xCC, 0x63, 0xF7, 0x45, 0x36, 0xF4, 0x9E, 0x04, 0xA0])
    expected_sync = np.unpackbits(np.frombuffer(sync_bytes, dtype=np.uint8))[:68]
    sync_match = np.array_equal(data[:68], expected_sync)

    if not sync_match:
        errors.append(f"{path.name}: sync pattern mismatch in first 68 symbols")
    else:
        print(f"  {path.name}: frame, {len(data)} symbols, sync OK")

    # Optionally decode
    try:
        from lunalink.afs import receive_frame

        bipolar = np.where(data == 0, 1.0, -1.0).astype(np.float32)
        result = receive_frame(bipolar, raise_on_error=False)
        parts = []
        if result.sb2.crc_ok:
            parts.append("SB2-CRC-OK")
        if result.sb3.crc_ok:
            parts.append("SB3-CRC-OK")
        if result.sb4.crc_ok:
            parts.append("SB4-CRC-OK")
        print(
            f"    decode: FID={result.fid} TOI={result.toi} "
            f"{' '.join(parts) or 'decode-failed'}"
        )
    except Exception:
        pass

    return errors


def _check_iq_file(path: Path) -> list[str]:
    """Validate an I/Q signal file (int16 or float32 interleaved)."""
    errors = []
    raw_bytes = _read_bytes(path)
    size = len(raw_bytes)

    if size == 0:
        errors.append(f"{path.name}: empty file")
        return errors

    if size % 4 != 0:
        errors.append(f"{path.name}: size {size} not aligned to 4-byte I/Q samples")
        return errors

    n_samples_i16 = size // 4
    n_samples_f32 = size // 8 if size % 8 == 0 else 0

    raw = np.frombuffer(raw_bytes[: min(4096, size)], dtype=np.int16)
    max_val = int(np.max(np.abs(raw)))

    if max_val > 0 and max_val < 32768:
        fmt = "int16"
        n_samples = n_samples_i16
    elif n_samples_f32 > 0:
        fmt = "float32"
        n_samples = n_samples_f32
    else:
        fmt = "int16"
        n_samples = n_samples_i16

    print(f"  {path.name}: I/Q {fmt}, {n_samples} samples, {size / 1024 / 1024:.1f} MB")
    return errors


def _check_json_file(path: Path) -> list[str]:
    """Validate a JSON vector file (parsed or decode-direction)."""
    errors = []
    try:
        data = json.loads(_read_bytes(path))
        keys = list(data.keys()) if isinstance(data, dict) else []
        print(f"  {path.name}: JSON, {len(keys)} top-level keys")
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        errors.append(f"{path.name}: invalid JSON — {e}")
    return errors


def _match_ext(path: Path, ext_set: set[str]) -> bool:
    """Check if a file's inner extension (ignoring .gz) is in ext_set."""
    _, ext = _stem_and_ext(path)
    return ext in ext_set


def _scan_dir(
    label: str,
    directory: Path,
    ext_set: set[str],
    checker: object,
    all_errors: list[str],
) -> None:
    """Scan a directory for files matching ext_set and run checker."""
    if not directory.exists():
        return
    files = sorted(
        f for f in directory.iterdir() if f.is_file() and _match_ext(f, ext_set)
    )
    if not files:
        return
    print(f"\n{label} ({directory}):")
    for f in files:
        all_errors.extend(checker(f))  # type: ignore[operator]


def validate(vector_dir: Path) -> int:
    """Validate all vectors in a directory tree. Returns error count."""
    all_errors: list[str] = []

    _scan_dir("Codes", vector_dir / "codes", CODE_EXTS, _check_code_file, all_errors)
    _scan_dir(
        "FEC vectors", vector_dir / "fec", JSON_EXTS, _check_json_file, all_errors
    )
    _scan_dir(
        "Frames", vector_dir / "frames", FRAME_EXTS, _check_frame_file, all_errors
    )
    _scan_dir("Signals", vector_dir / "signals", IQ_EXTS, _check_iq_file, all_errors)
    _scan_dir("Parsed", vector_dir / "parsed", JSON_EXTS, _check_json_file, all_errors)
    _scan_dir(
        "Decode vectors", vector_dir / "decode", JSON_EXTS, _check_json_file, all_errors
    )

    # Flat directory fallback
    subdirs = {"codes", "frames", "signals", "parsed", "decode"}
    if not any((vector_dir / d).exists() for d in subdirs):
        print(f"\nScanning {vector_dir} (flat layout):")
        for f in sorted(vector_dir.iterdir()):
            if not f.is_file():
                continue
            _, ext = _stem_and_ext(f)
            raw = _read_bytes(f)
            size = len(raw)
            if ext in CODE_EXTS:
                all_errors.extend(_check_code_file(f))
            elif size in (FRAME_LENGTH, FRAME_LENGTH + LSISAFS_HEADER_SIZE):
                all_errors.extend(_check_frame_file(f))
            elif ext in JSON_EXTS:
                all_errors.extend(_check_json_file(f))
            elif size > FRAME_LENGTH and ext in IQ_EXTS:
                all_errors.extend(_check_iq_file(f))

    if all_errors:
        print(f"\n{len(all_errors)} error(s):")
        for e in all_errors:
            print(f"  ERROR: {e}")
        return 1

    print("\nAll checks passed.")
    return 0


def main() -> None:
    """Parse arguments and validate foreign test vectors."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "vector_dir", type=Path, help="Directory containing test vectors"
    )
    args = parser.parse_args()

    if not args.vector_dir.is_dir():
        print(f"ERROR: {args.vector_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    sys.exit(validate(args.vector_dir))


if __name__ == "__main__":
    main()
