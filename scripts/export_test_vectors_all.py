"""Regenerate every level of the public ``lsis-afs-test-vectors`` set.

One-shot orchestrator: drives ``export_test_vectors_l1.py`` (codes),
``export_test_vectors_l2.py`` (frames), ``export_test_vectors_fec.py``
(BCH/CRC/LDPC/interleaver component vectors), ``export_test_vectors_l3.py``
(I/Q signals), and ``export_test_vectors_l5.py`` (parsed JSONs) in
sequence over a target ``lsis-afs-test-vectors`` checkout.

Used for:
- Bumping the spec or finding a producer-side bug — one command rebuilds
  every level.
- CI: regenerate-and-diff against the committed public-repo state to
  detect silent producer drift.
- Local development on the lunalink reference implementation: run from
  the lunalink repo to refresh the sibling checkout.

Levels are produced in dependency order:
  L1 codes → L2 frames → FEC components → L3 signals (reads L2 frames)
  → L5 parsed JSONs
  (reads L2 frames + runs LunaLink RX path).

L4 (decoded reference data) is consumer-side: produced by
PocketSDR-AFS against the L3 signals and lives entirely in the public
repo's ``references/pocketsdr-afs/`` directory.  No lunalink producer.

Usage:
    python scripts/export_test_vectors_all.py <public_repo_path>

    # Or, with a sibling checkout at ../lsis-afs-test-vectors/:
    python scripts/export_test_vectors_all.py

The target directory must already exist as a clone of
``luar-space/lsis-afs-test-vectors``.  Subdirectories ``codes/``,
``frames/``, ``signals/``, ``parsed/`` are created as needed.

Heads-up: L3 generation modulates 10× ~1 GB I/Q streams at compresslevel=9
and takes 20-30 minutes on a fast laptop.  Pass ``--skip-l3`` to skip it
when iterating on the other levels.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = REPO_ROOT / "scripts"

# (level_label, script_filename, target_subdir, needs_input_dir)
# needs_input_dir = True for levels that read freshly-generated frames
# (L3 modulates them, L5 runs the LunaLink RX path on them).
LEVELS: list[tuple[str, str, str, bool]] = [
    ("L1 codes", "export_test_vectors_l1.py", "codes", False),
    ("L2 frames", "export_test_vectors_l2.py", "frames", False),
    ("FEC components", "export_test_vectors_fec.py", "fec", False),
    ("L3 signals", "export_test_vectors_l3.py", "signals", True),
    ("L5 parsed", "export_test_vectors_l5.py", "parsed", True),
]


def _run_level(
    label: str, script: str, target_dir: Path, frames_dir: Path | None
) -> int:
    print(
        f"\n── {label} → {target_dir} ─────────────────────────────────",
        file=sys.stderr,
    )
    cmd = [sys.executable, str(SCRIPTS / script), str(target_dir)]
    if frames_dir is not None:
        cmd.extend(["--input-dir", str(frames_dir)])
    proc = subprocess.run(cmd, check=False)
    return proc.returncode


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "public_repo",
        type=Path,
        nargs="?",
        default=None,
        help="Path to a checkout of luar-space/lsis-afs-test-vectors. "
        "Defaults to ../lsis-afs-test-vectors/ relative to this lunalink repo.",
    )
    ap.add_argument(
        "--skip-l3",
        action="store_true",
        help="Skip L3 (I/Q signals); takes 20-30 minutes at compresslevel=9.",
    )
    ap.add_argument(
        "--only",
        choices=["L1", "L2", "FEC", "L3", "L5"],
        action="append",
        help="Run only the named level(s). Repeatable, e.g. --only L2 --only L5.",
    )
    args = ap.parse_args(argv)

    public_repo: Path = args.public_repo or REPO_ROOT.parent / "lsis-afs-test-vectors"
    if not public_repo.is_dir():
        print(f"error: public repo not found at {public_repo}", file=sys.stderr)
        return 2

    selected_levels = LEVELS
    if args.only:
        wanted = set(args.only)  # {"L1", "L2", ...}
        selected_levels = [
            (label, script, sub, needs_in)
            for label, script, sub, needs_in in LEVELS
            if label.split()[0] in wanted
        ]
    if args.skip_l3:
        selected_levels = [t for t in selected_levels if t[0] != "L3 signals"]

    frames_dir = public_repo / "frames"
    failures: list[str] = []
    for label, script, subdir, needs_input_dir in selected_levels:
        target = public_repo / subdir
        target.mkdir(parents=True, exist_ok=True)
        rc = _run_level(label, script, target, frames_dir if needs_input_dir else None)
        if rc != 0:
            failures.append(f"{label} (rc={rc})")

    print(
        "\n── summary ─────────────────────────────────────────────",
        file=sys.stderr,
    )
    if failures:
        print(f"FAIL: {len(failures)} level(s): {', '.join(failures)}", file=sys.stderr)
        return 1
    print(
        f"OK: regenerated {len(selected_levels)}/{len(LEVELS)} levels into {public_repo}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
