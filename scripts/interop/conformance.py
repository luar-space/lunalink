# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Check LunaLink against the shared LSIS-AFS test vectors, level by level.

The shared vectors (``luar-space/lsis-afs-test-vectors``) are the reference
other implementations check themselves against.  This script holds LunaLink to
the same check: it fetches the repository at a pinned release tag, has LunaLink
produce each level into a temporary directory, and lets the repository's own
``validate.py`` judge the result with the ``diff-*`` commands any implementer
would run.

  Level 1  spreading codes             validate.py diff
  Level 2  encoded frames              validate.py diff-frames
  Level 2  pre-encode subframe bits    validate.py diff-inputs
  FEC      per-stage component vectors validate.py diff-fec
  Level 3  baseband I/Q signals        validate.py diff-signals
  Level 4  bits recovered from signals validate.py diff-decode
  Level 5  parsed navigation data      validate.py diff-parsed

Levels 3 and 5 start from the shared frames and level 4 from the shared
signals, so each level is judged on its own: a level 2 difference does not
reappear at every later level.

Usage:
    python scripts/interop/conformance.py               # pinned tag, all levels
    python scripts/interop/conformance.py --skip-l3     # skip the slow level
    python scripts/interop/conformance.py --vectors DIR # an existing checkout

Exit status is 0 only if every level passes.
"""

from __future__ import annotations

import argparse
import gzip
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np

from lunalink.afs._transmitter import _apply_sb2_spare_pattern

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "interop"))

from export_test_vectors_l2 import TEST_MESSAGES  # noqa: E402
from iq_receiver import decode_iq_file  # noqa: E402

VECTORS_REPO = "https://github.com/luar-space/lsis-afs-test-vectors"
# The shared release LunaLink is held to.  Moving to a newer release is a
# deliberate change: bump this, and LunaLink has to pass the new vectors.
VECTORS_TAG = "v0.6.2"
VECTORS_DIR = REPO_ROOT / "third_party" / "lsis-afs-test-vectors"


def _git(*args: str, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args], cwd=cwd, check=False, capture_output=True, text=True
    )


def fetch_vectors(target: Path, tag: str) -> None:
    """Clone the shared vectors at ``tag``, or move an existing clone to it."""
    if (target / ".git").exists():
        current = _git("describe", "--tags", "--exact-match", cwd=target)
        if current.returncode == 0 and current.stdout.strip() == tag:
            print(f"Shared vectors at {tag}: {target}")
            return
        print(f"Moving shared vectors to {tag} ...")
        for args in (
            ("fetch", "--depth", "1", "origin", "tag", tag),
            ("checkout", tag),
        ):
            proc = _git(*args, cwd=target)
            if proc.returncode != 0:
                raise RuntimeError(f"git {' '.join(args)} failed:\n{proc.stderr}")
        return
    print(f"Cloning {VECTORS_REPO} at {tag} ...")
    target.parent.mkdir(parents=True, exist_ok=True)
    proc = _git("clone", "--depth", "1", "--branch", tag, VECTORS_REPO, str(target))
    if proc.returncode != 0:
        raise RuntimeError(f"git clone failed:\n{proc.stderr}")


def _run(cmd: list[str]) -> tuple[int, str]:
    proc = subprocess.run(cmd, check=False, capture_output=True, text=True)
    return proc.returncode, (proc.stdout + proc.stderr).strip()


def _export(script: str, out_dir: Path, *extra: str) -> None:
    code, out = _run([sys.executable, str(SCRIPTS / script), str(out_dir), *extra])
    if code != 0:
        raise RuntimeError(f"{script} failed:\n{out}")


def produce_inputs(out_dir: Path) -> None:
    """Write the level 2 subframe bits as ``*_input.bin`` (one bit per byte).

    These are the bits LunaLink encodes: the test patterns with the
    LSIS-AFS-300 spare pattern in SB2, which ``frame_build`` writes before
    encoding.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    for filename, *_, pattern_fn in TEST_MESSAGES:
        sb2, sb3, sb4 = pattern_fn()
        sb2 = _apply_sb2_spare_pattern(sb2)
        bits = np.concatenate([sb2, sb3, sb4]).astype(np.uint8)
        (out_dir / filename.replace(".bin", "_input.bin")).write_bytes(bits.tobytes())


def produce_decode(signals_dir: Path, out_dir: Path) -> None:
    """Decode every shared level 3 signal and write ``decoded_fec_*.bin``.

    The PRN of each signal is taken from its ``LSISIQ`` header, so the list of
    signals is the shared repository's, not one LunaLink keeps.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    for path in sorted(signals_dir.glob("signal_*.iq.gz")):
        prn = _header_prn(path)
        result = decode_iq_file(path, prn)
        bits = np.concatenate(
            [result.sb2.data, result.sb3.data, result.sb4.data]
        ).astype(np.uint8)
        stem = path.name.removesuffix(".gz").removesuffix(".iq")
        (out_dir / f"decoded_fec_{stem}.bin").write_bytes(bits.tobytes())


def _header_prn(path: Path) -> int:
    """Read the PRN field of a gzip-compressed ``LSISIQ`` header."""
    with gzip.open(path, "rb") as f:
        header = f.read(32)
    return struct.unpack_from("<I", header, 28)[0]


def main(argv: list[str] | None = None) -> int:
    """Produce each level with LunaLink and judge it with the shared validator."""
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument(
        "--vectors",
        type=Path,
        default=None,
        help="Use this checkout of the shared vectors instead of fetching "
        f"{VECTORS_TAG} into third_party/.",
    )
    ap.add_argument(
        "--skip-l3",
        action="store_true",
        help="Skip level 3, which modulates ten 12 s signals.",
    )
    ap.add_argument(
        "--keep",
        type=Path,
        default=None,
        help="Write LunaLink's output here and keep it, instead of a temp dir.",
    )
    args = ap.parse_args(argv)

    vectors: Path = args.vectors or VECTORS_DIR
    if args.vectors is None:
        fetch_vectors(vectors, VECTORS_TAG)
    validate = vectors / "validate.py"

    def check(*cmd: str) -> tuple[int, str]:
        return _run([sys.executable, "-I", str(validate), *cmd])

    code, out = check("verify-manifest")
    if code != 0:
        print(f"FAIL  the shared vectors do not match their manifest:\n{out}")
        return 1

    with tempfile.TemporaryDirectory(prefix="lunalink-conformance-") as tmp_name:
        work = args.keep or Path(tmp_name)
        work.mkdir(parents=True, exist_ok=True)
        shared_frames = str(vectors / "frames")

        levels: list[tuple[str, object, tuple[str, ...]]] = [
            (
                "L1   codes",
                lambda: _export("export_test_vectors_l1.py", work / "codes"),
                ("diff", str(work / "codes")),
            ),
            (
                "L2   frames",
                lambda: _export("export_test_vectors_l2.py", work / "frames"),
                ("diff-frames", str(work / "frames")),
            ),
            (
                "L2   inputs",
                lambda: produce_inputs(work / "inputs"),
                ("diff-inputs", str(work / "inputs")),
            ),
            # In a directory of its own: with frames/ and inputs/ beside it,
            # the exporter would stitch-check against them and repeat any
            # level 2 failure here.
            (
                "FEC  components",
                lambda: _export(
                    "export_test_vectors_fec.py", work / "fec-only" / "fec"
                ),
                ("diff-fec", str(work / "fec-only" / "fec")),
            ),
            (
                "L3   signals",
                lambda: _export(
                    "export_test_vectors_l3.py",
                    work / "signals",
                    "--input-dir",
                    shared_frames,
                    "--compresslevel",
                    "1",
                ),
                ("diff-signals", str(work / "signals")),
            ),
            (
                "L4   decode",
                lambda: produce_decode(vectors / "signals", work / "decode"),
                ("diff-decode", str(work / "decode")),
            ),
            (
                "L5   parsed",
                lambda: _export(
                    "export_test_vectors_l5.py",
                    work / "parsed",
                    "--input-dir",
                    shared_frames,
                ),
                ("diff-parsed", str(work / "parsed")),
            ),
        ]

        print(f"LunaLink against lsis-afs-test-vectors {_describe(vectors)}\n")
        failed = 0
        for label, produce, cmd in levels:
            if args.skip_l3 and label.startswith("L3"):
                print(f"SKIP  {label}")
                continue
            start = time.monotonic()
            try:
                produce()  # type: ignore[operator]
            except RuntimeError as exc:
                failed += 1
                print(f"FAIL  {label}  (LunaLink could not produce it)\n{exc}")
                continue
            code, out = check(*cmd)
            took = time.monotonic() - start
            last = out.splitlines()[-1] if out else ""
            if code == 0:
                print(f"PASS  {label:<16} {took:6.1f}s  {last}")
            else:
                failed += 1
                print(f"FAIL  {label:<16} {took:6.1f}s\n{out}")

    print()
    if failed:
        print(f"{failed} level(s) failed.")
        return 1
    print("PASS: LunaLink matches the shared test vectors at every level checked.")
    return 0


def _describe(vectors: Path) -> str:
    proc = _git("describe", "--tags", "--always", cwd=vectors)
    return proc.stdout.strip() if proc.returncode == 0 else str(vectors)


if __name__ == "__main__":
    raise SystemExit(main())
