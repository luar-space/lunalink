# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""LunaLink command-line interface.

Workshop-shape artefact production for the ESA-CCSDS Goonhilly
interoperability workshop.

Subcommands
-----------
generate-codes
    Emit Gold spreading codes for PRN 1-210 in workshop ``codes.txt`` shape
    (single file, 210 lines, 512 hex chars per line — Gold codes only).
encode --format frame
    Emit a raw 6000-byte navigation frame (one byte per symbol, no header).
encode --format iq32
    Emit a raw float32 I/Q signal at the given sample rate (no header,
    AFS-I on the I channel, ``0.0`` on the Q channel).
decode --format iq32
    Despread a raw float32 I/Q signal with the given PRN and run the
    full RX pipeline (sync -> BCH -> deinterleave -> LDPC -> CRC).
    Emits the workshop ``decode.sh`` stdout shape.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

from lunalink.afs import (
    EPOCHS_PER_FRAME,
    GOLD_CHIP_LENGTH,
    ITOW_BITS,
    ITOW_MAX,
    PRN_COUNT,
    SB2_DATA_BITS,
    SB3_DATA_BITS,
    SB4_DATA_BITS,
    WN_BITS,
    WN_MAX,
    frame_build,
    generate_iq_frame,
    parse_sb2,
    prn_code,
    receive_frame,
)


def _gold_chips_to_hex(chips: np.ndarray) -> str:
    """Encode 2046 Gold chips as the 512-char workshop ``codes.txt`` line.

    Workshop convention (Goonhilly interop): 2046 chips MSB-first, last
    2 bits zero-padded into 256 bytes (512 hex). Spec-compliant first
    6 hex of PRN 1 = ``5D6430``.
    """
    padded = np.zeros(GOLD_CHIP_LENGTH + 2, dtype=np.uint8)
    padded[:GOLD_CHIP_LENGTH] = chips
    return np.packbits(padded, bitorder="big").tobytes().hex().upper()


def _cmd_generate_codes(args: argparse.Namespace) -> int:
    """Emit Gold codes for PRN 1-210 to ``args.out`` (workshop ``codes.txt``)."""
    out_path = Path(args.out)
    lines = [_gold_chips_to_hex(prn_code(prn)) for prn in range(1, PRN_COUNT + 1)]
    out_path.write_text("\n".join(lines) + "\n")
    print(
        f"Wrote {len(lines)} Gold codes to {out_path} ({out_path.stat().st_size} bytes)"
    )
    return 0


def _int_to_bits_msb(value: int, width: int) -> np.ndarray:
    """Encode ``value`` into ``width`` bits, MSB-first, as a uint8 array."""
    return np.array(
        [(value >> (width - 1 - i)) & 1 for i in range(width)],
        dtype=np.uint8,
    )


def _build_sb2_data(wn: int, itow: int) -> np.ndarray:
    """Build a 1176-bit SB2 data array with WN, ITOW set; CED/Health/etc zero.

    The workshop canonical payload sets WN at bits [0, WN_BITS) and ITOW at
    bits [WN_BITS, WN_BITS + ITOW_BITS) of SB2 data, MSB-first. All other
    bits (CED, Health, Time Conversions, spare) are zero.
    """
    if not 0 <= wn <= WN_MAX:
        raise ValueError(f"--wn must be in [0, {WN_MAX}], got {wn}")
    if not 0 <= itow <= ITOW_MAX:
        raise ValueError(f"--itow must be in [0, {ITOW_MAX}], got {itow}")
    sb2 = np.zeros(SB2_DATA_BITS, dtype=np.uint8)
    sb2[:WN_BITS] = _int_to_bits_msb(wn, WN_BITS)
    sb2[WN_BITS : WN_BITS + ITOW_BITS] = _int_to_bits_msb(itow, ITOW_BITS)
    return sb2


def _cmd_encode_frame(args: argparse.Namespace) -> int:
    """Emit a 6000-byte raw navigation frame to ``args.out``."""
    sb2 = _build_sb2_data(args.wn, args.itow)
    sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
    sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
    symbols = frame_build(args.fid, args.toi, sb2, sb3, sb4)
    out_path = Path(args.out)
    out_path.write_bytes(symbols.astype(np.uint8).tobytes())
    print(
        f"Wrote {out_path.stat().st_size}-byte frame to {out_path} "
        f"(PRN={args.prn} FID={args.fid} TOI={args.toi} "
        f"WN={args.wn} ITOW={args.itow})"
    )
    return 0


def _cmd_encode_iq32(args: argparse.Namespace) -> int:
    """Emit a raw float32 I/Q signal to ``args.out``.

    Output shape matches the workshop's interop convention: float32
    little-endian, interleaved ``[I0, Q0, I1, Q1, ...]``, no file header.
    The AFS-I data channel goes on I; the Q channel is forced to ``0.0``
    (AFS-I-only baseline per the workshop clarification).

    With ``--input-frame``, the 6000 symbols are read verbatim from the
    given file instead of being regenerated — useful for spreading an
    externally-encoded frame (e.g. the workshop reference) through the
    LunaLink signal generator at the chip layer.
    """
    if args.input_frame is not None:
        symbols = np.frombuffer(
            Path(args.input_frame).read_bytes(), dtype=np.uint8
        ).copy()
        if symbols.size != 6000:
            raise ValueError(
                f"--input-frame must be exactly 6000 bytes, got {symbols.size}"
            )
    else:
        sb2 = _build_sb2_data(args.wn, args.itow)
        sb3 = np.zeros(SB3_DATA_BITS, dtype=np.uint8)
        sb4 = np.zeros(SB4_DATA_BITS, dtype=np.uint8)
        symbols = frame_build(args.fid, args.toi, sb2, sb3, sb4)
    iq = generate_iq_frame(args.prn, symbols, float(args.rate))
    iq[:, 1] = 0.0
    out_path = Path(args.out)
    out_path.write_bytes(iq.astype("<f4").tobytes())
    print(
        f"Wrote {out_path.stat().st_size}-byte I/Q signal to {out_path} "
        f"(PRN={args.prn} FID={args.fid} TOI={args.toi} "
        f"WN={args.wn} ITOW={args.itow} rate={args.rate})"
    )
    return 0


def _dispatch_encode(args: argparse.Namespace) -> int:
    """Route the ``encode`` subcommand to the format-specific handler."""
    if args.format == "frame":
        return _cmd_encode_frame(args)
    return _cmd_encode_iq32(args)


def _cmd_decode_iq32(args: argparse.Namespace) -> int:
    """Despread an iq32 file and emit the workshop ``decode.sh`` stdout shape.

    Despread is the chip-rate inverse of ``modulate_i``: for each of the
    6000 symbol epochs, multiply the I samples by the bipolar PRN code
    and average. The resulting soft symbols are fed to ``receive_frame``,
    which runs sync detection, BCH, deinterleave, LDPC, and CRC-24.

    Stdout shape (matches the contract in decode.sh)::

        FID=<int>
        TOI=<int>
        WN=<int>
        ITOW=<int>
        CRC_SB2=PASS|FAIL
        CRC_SB3=PASS|FAIL
        CRC_SB4=PASS|FAIL
    """
    if args.rate != 1_023_000:
        raise ValueError(
            f"--rate {args.rate} not supported by decode (only 1023000 Hz)."
        )

    raw = np.frombuffer(Path(args.input).read_bytes(), dtype="<f4")
    expected = 2 * EPOCHS_PER_FRAME * GOLD_CHIP_LENGTH
    if raw.size != expected:
        raise ValueError(f"Input must be {expected * 4} bytes (got {raw.size * 4}).")
    i_samples = raw.reshape(-1, 2)[:, 0]

    chip_pol = 1.0 - 2.0 * prn_code(args.prn).astype(np.float32)
    soft = (
        (i_samples.reshape(EPOCHS_PER_FRAME, GOLD_CHIP_LENGTH) * chip_pol)
        .mean(axis=1)
        .astype(np.float32)
    )

    result = receive_frame(soft, raise_on_error=False)

    if result.time is not None:
        wn, itow = result.time.wn, result.time.itow
    elif result.sb2.crc_ok:
        sb2_parsed = parse_sb2(result.sb2.data)
        wn, itow = sb2_parsed.wn, sb2_parsed.itow
    else:
        wn, itow = -1, -1

    print(f"FID={int(result.fid)}")
    print(f"TOI={int(result.toi)}")
    print(f"WN={wn}")
    print(f"ITOW={itow}")
    print(f"CRC_SB2={'PASS' if result.sb2.crc_ok else 'FAIL'}")
    print(f"CRC_SB3={'PASS' if result.sb3.crc_ok else 'FAIL'}")
    print(f"CRC_SB4={'PASS' if result.sb4.crc_ok else 'FAIL'}")

    all_crcs_pass = result.sb2.crc_ok and result.sb3.crc_ok and result.sb4.crc_ok
    return 0 if all_crcs_pass else 1


def _dispatch_decode(args: argparse.Namespace) -> int:
    """Route the ``decode`` subcommand to the format-specific handler."""
    return _cmd_decode_iq32(args)


def _build_parser() -> argparse.ArgumentParser:
    """Build the top-level argument parser."""
    parser = argparse.ArgumentParser(
        prog="lunalink",
        description=(
            "LunaLink CLI — workshop-shape artefact production for the "
            "ESA-CCSDS LSIS-AFS interoperability workshop."
        ),
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_codes = sub.add_parser(
        "generate-codes",
        help="Emit Gold spreading codes (codes.txt, 210 x 512 hex).",
    )
    p_codes.add_argument(
        "--out",
        default="codes.txt",
        help="Output file path (default: codes.txt).",
    )
    p_codes.set_defaults(func=_cmd_generate_codes)

    p_encode = sub.add_parser(
        "encode",
        help="Encode a navigation artefact (frame or I/Q signal).",
    )
    p_encode.add_argument(
        "--format",
        required=True,
        choices=("frame", "iq32"),
        help="Output artefact shape.",
    )
    p_encode.add_argument(
        "--prn",
        type=int,
        required=True,
        help="PRN ID (1-210).",
    )
    p_encode.add_argument(
        "--fid",
        type=int,
        default=0,
        help="Frame ID (0-3, default 0).",
    )
    p_encode.add_argument(
        "--toi",
        type=int,
        default=0,
        help="Time of Interval (0-99, default 0).",
    )
    p_encode.add_argument(
        "--wn",
        type=int,
        default=0,
        help="Week Number (0-8191, default 0).",
    )
    p_encode.add_argument(
        "--itow",
        type=int,
        default=0,
        help="Interval Time of Week (0-503, default 0).",
    )
    p_encode.add_argument(
        "--rate",
        type=int,
        default=1_023_000,
        help=("Sample rate in Hz (iq32 only, default 1023000 = 1 sample/chip)."),
    )
    p_encode.add_argument(
        "--out",
        required=True,
        help="Output file path.",
    )
    p_encode.add_argument(
        "--input-frame",
        default=None,
        help=(
            "iq32 only: spread an existing 6000-byte frame.bin instead of "
            "encoding from --fid/--toi/--wn/--itow."
        ),
    )
    p_encode.set_defaults(func=_dispatch_encode)

    p_decode = sub.add_parser(
        "decode",
        help="Decode an I/Q signal back to FID/TOI/WN/ITOW + CRC pass flags.",
    )
    p_decode.add_argument(
        "--format",
        required=True,
        choices=("iq32",),
        help="Input artefact shape.",
    )
    p_decode.add_argument(
        "--prn",
        type=int,
        required=True,
        help="PRN ID used to despread (1-210).",
    )
    p_decode.add_argument(
        "--rate",
        type=int,
        default=1_023_000,
        help="Sample rate in Hz (default 1023000 = 1 sample/chip).",
    )
    p_decode.add_argument(
        "--input",
        required=True,
        help="Input file path (raw float32 I/Q, interleaved).",
    )
    p_decode.set_defaults(func=_dispatch_decode)

    return parser


def main(argv: list[str] | None = None) -> int:
    """Run the LunaLink CLI.

    Parameters
    ----------
    argv : list[str] or None
        Argument vector excluding the program name. ``None`` reads
        ``sys.argv[1:]`` (the standard argparse default).

    Returns
    -------
    int
        Process exit code (0 on success).
    """
    parser = _build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
