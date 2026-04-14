#!/usr/bin/env python3
"""I1 — Clone and build LANS-AFS-SIM (Ebinuma's independent LSIS-AFS implementation).

https://github.com/osqzss/LANS-AFS-SIM

Usage:
    python scripts/interop/build_lans_afs_sim.py [--clean]

Installs into: third_party/LANS-AFS-SIM/
Binary:        third_party/LANS-AFS-SIM/afs_sim
"""

from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY = REPO_ROOT / "third_party"
LANS_DIR = THIRD_PARTY / "LANS-AFS-SIM"
LANS_REPO = "https://github.com/osqzss/LANS-AFS-SIM.git"


def clone() -> None:
    """Clone the LANS-AFS-SIM repository if not already present."""
    if LANS_DIR.exists():
        print(f"LANS-AFS-SIM already cloned at {LANS_DIR}")
        return
    print("Cloning LANS-AFS-SIM...")
    THIRD_PARTY.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["git", "clone", "--depth", "1", LANS_REPO, str(LANS_DIR)],
        check=True,
    )


def find_gcc_on_macos() -> str | None:
    """Find Homebrew GCC (Apple clang lacks -fopenmp)."""
    try:
        prefix = subprocess.run(
            ["brew", "--prefix", "gcc"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    bin_dir = Path(prefix) / "bin"
    for ver in range(15, 12, -1):
        candidate = bin_dir / f"gcc-{ver}"
        if candidate.is_file():
            return str(candidate)
    return None


def patch_openmp(makefile: Path) -> None:
    """Remove -fopenmp from makefile when no OpenMP-capable compiler is available."""
    text = makefile.read_text()
    if "-fopenmp" in text:
        print("Patching makefile: removing -fopenmp")
        makefile.write_text(text.replace("-fopenmp", ""))


def build(cc_override: list[str]) -> None:
    """Build LANS-AFS-SIM and interop harnesses."""
    print("Building LANS-AFS-SIM...")
    makefile = LANS_DIR / "makefile"

    if platform.system() == "Darwin" and not cc_override:
        print("Warning: Homebrew GCC not found; dropping -fopenmp")
        patch_openmp(makefile)
    elif cc_override:
        print(f"Using: {cc_override[0]}")

    subprocess.run(
        ["make", "clean", *cc_override], cwd=LANS_DIR, capture_output=True
    )  # ignore errors
    subprocess.run(["make", *cc_override], cwd=LANS_DIR, check=True)


def _get_cc(cc_override: list[str]) -> str:
    for arg in cc_override:
        if arg.startswith("CC="):
            return arg.split("=", 1)[1]
    return "gcc"


def _common_flags(cc: str) -> list[str]:
    flags = [
        "-O2",
        "-I.",
        "-I./ldpc",
        "-I./rtklib",
        "-I./pocketsdr",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
    ]
    if cc != "gcc" or platform.system() != "Darwin":
        flags.append("-fopenmp")
    return flags


def _build_harness(cc: str, flags: list[str], src: Path, name: str) -> None:
    """Build a C harness linked against LANS-AFS-SIM object files."""
    harness_bin = LANS_DIR / name

    # Compile afs_sim.c → afs_sim_lib.o with main renamed (once)
    lib_obj = LANS_DIR / "afs_sim_lib.o"
    if not lib_obj.exists():
        subprocess.run(
            [
                cc,
                *flags,
                "-Dmain=afs_sim_main",
                "-c",
                "afs_sim.c",
                "-o",
                "afs_sim_lib.o",
            ],
            cwd=LANS_DIR,
            check=True,
        )

    support_objs = [
        "afs_nav.o",
        "afs_rand.o",
        "ldpc/alloc.o",
        "ldpc/mod2sparse.o",
        "rtklib/rtkcmn.o",
        "pocketsdr/pocketsdr.o",
    ]
    subprocess.run(
        [
            cc,
            *flags,
            str(src),
            "afs_sim_lib.o",
            *support_objs,
            "-lm",
            "-o",
            str(harness_bin),
        ],
        cwd=LANS_DIR,
        check=True,
    )
    print(f"  Built: {harness_bin}")


HARNESSES = [
    ("dump_lans_codes.c", "dump_lans_codes"),
    ("dump_lans_frame.c", "dump_lans_frame"),
    # G-16: standalone LDPC encoder dumper. One-shot tool — run manually
    # to regenerate cpp/tests/encoding/ldpc_lans_reference.hpp. Not invoked
    # by any pytest, but bundled with the existing harness build so the
    # binary is available for hand-run regeneration after every rebuild.
    ("dump_lans_ldpc.c", "dump_lans_ldpc"),
]


def build_harnesses(cc_override: list[str]) -> None:
    """Build all interop harnesses."""
    cc = _get_cc(cc_override)
    flags = _common_flags(cc)
    interop_dir = REPO_ROOT / "scripts" / "interop"

    print("\nBuilding interop harnesses...")
    for src_name, bin_name in HARNESSES:
        _build_harness(cc, flags, interop_dir / src_name, bin_name)


def verify() -> None:
    """Verify that all expected binaries were built successfully."""
    binaries = ["afs_sim"] + [h[1] for h in HARNESSES]
    for name in binaries:
        binary = LANS_DIR / name
        if binary.is_file() and binary.stat().st_mode & 0o111:
            print(f"  OK: {binary}")
        else:
            print(f"ERROR: {name} not found after build", file=sys.stderr)
            sys.exit(1)


def main() -> None:
    """Parse arguments and run clone, build, verify pipeline."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--clean", action="store_true", help="Remove and re-clone before building"
    )
    args = parser.parse_args()

    if args.clean and LANS_DIR.exists():
        print(f"Removing {LANS_DIR}...")
        shutil.rmtree(LANS_DIR)

    clone()
    cc_override = _get_cc_override()
    build(cc_override)
    build_harnesses(cc_override)
    verify()


def _get_cc_override() -> list[str]:
    """Determine CC override for macOS, shared between build steps."""
    if platform.system() == "Darwin":
        gcc = find_gcc_on_macos()
        if gcc:
            return [f"CC={gcc}"]
    return []


if __name__ == "__main__":
    main()
