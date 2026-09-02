#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Clone and build PocketSDR-AFS, the third-party receiver used for L4/L5.

PocketSDR-AFS (Sobukawa & Ebinuma) is a software-defined LANS AFS receiver:
https://github.com/osqzss/PocketSDR-AFS.  It is the *receiving* counterpart to
LANS-AFS-SIM and the tool that closes the reverse direction of the
interoperability plan — an independent implementation decoding a LunaLink
signal, rather than LunaLink decoding theirs.

Nothing is vendored: the sources are cloned at build time, exactly as
``build_lans_afs_sim.py`` does for LANS-AFS-SIM.

One upstream patch is applied, and it is deliberately the smallest possible.
``src/sdr_pvt_afs.c`` declares a file-static ``satpos()`` whose name collides
with RTKLIB's public ``satpos()`` from ``rtklib.h``, which the translation unit
includes transitively.  Clang rejects this ("static declaration of 'satpos'
follows non-static declaration"), so the build fails on macOS.  The patch
renames the static function and its single call site to ``afs_satpos``.  It
changes no arithmetic, no signal processing and no decision logic; without it
the receiver cannot be built here at all.

Prerequisites:
    macOS:  brew install fftw libusb
    Ubuntu: sudo apt install libfftw3-dev libusb-1.0-0-dev

Usage:
    python scripts/interop/build_pocketsdr_afs.py [--clean]

Installs into: third_party/PocketSDR-AFS/
Binary:        third_party/PocketSDR-AFS/app/pocket_trk/pocket_trk
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY = REPO_ROOT / "third_party"
PSDR_DIR = THIRD_PARTY / "PocketSDR-AFS"
PSDR_REPO = "https://github.com/osqzss/PocketSDR-AFS.git"
BINARY = PSDR_DIR / "app" / "pocket_trk" / "pocket_trk"

# The one upstream source we touch, and why (see the module docstring).
PATCH_FILE = PSDR_DIR / "src" / "sdr_pvt_afs.c"


def run(cmd: list[str] | str, cwd: Path, shell: bool = False) -> None:
    """Run a build step, failing loudly with its output."""
    proc = subprocess.run(cmd, cwd=cwd, shell=shell, capture_output=True, text=True)
    if proc.returncode != 0:
        printable = cmd if isinstance(cmd, str) else " ".join(cmd)
        print(f"FAILED ({proc.returncode}): {printable}", file=sys.stderr)
        print(proc.stdout[-4000:], file=sys.stderr)
        print(proc.stderr[-4000:], file=sys.stderr)
        raise SystemExit(1)


def clone() -> None:
    """Clone PocketSDR-AFS and its external library sources."""
    if (PSDR_DIR / ".git").exists():
        print(f"PocketSDR-AFS already cloned at {PSDR_DIR}")
        return
    THIRD_PARTY.mkdir(parents=True, exist_ok=True)
    print(f"Cloning {PSDR_REPO} ...")
    run(["git", "clone", "--depth", "1", PSDR_REPO, str(PSDR_DIR)], cwd=THIRD_PARTY)


def clone_libs() -> None:
    """Clone libfec and LDPC-codes, which the receiver's makefiles expect."""
    lib = PSDR_DIR / "lib"
    if (lib / "libfec").exists() and (lib / "LDPC-codes").exists():
        print("External library sources already present")
        return
    print("Cloning external library sources (libfec, LDPC-codes) ...")
    run(["bash", "clone_lib.sh"], cwd=lib)


def patch_satpos() -> None:
    """Rename the file-static satpos() that collides with RTKLIB's."""
    text = PATCH_FILE.read_text(encoding="utf-8", errors="surrogateescape")
    if "afs_satpos" in text:
        print("satpos collision already patched")
        return
    patched, count = re.subn(r"\bsatpos\b", "afs_satpos", text)
    if count == 0:
        print("WARNING: satpos not found — upstream may have fixed the collision")
        return
    PATCH_FILE.write_text(patched, encoding="utf-8", errors="surrogateescape")
    rel = PATCH_FILE.relative_to(REPO_ROOT)
    print(f"Patched {rel}: satpos -> afs_satpos ({count} sites)")


def build() -> None:
    """Build the receiver libraries, then pocket_trk itself."""
    print("Building libraries (librtk, libldpc, libfec, libsdr) ...")
    run(["make"], cwd=PSDR_DIR / "lib" / "build")
    run(["make", "install"], cwd=PSDR_DIR / "lib" / "build")
    print("Building pocket_trk ...")
    run(["make"], cwd=PSDR_DIR / "app" / "pocket_trk")


def main() -> int:
    """Clone, patch and build the receiver; report where the binary landed."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="remove the clone first")
    args = parser.parse_args()

    if args.clean and PSDR_DIR.exists():
        print(f"Removing {PSDR_DIR}")
        shutil.rmtree(PSDR_DIR)

    if BINARY.is_file():
        print(f"pocket_trk already built: {BINARY.relative_to(REPO_ROOT)}")
        return 0

    clone()
    clone_libs()
    patch_satpos()
    build()

    if not BINARY.is_file():
        print("Build reported success but the binary is missing", file=sys.stderr)
        return 1
    print(f"\nBuilt: {BINARY.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
