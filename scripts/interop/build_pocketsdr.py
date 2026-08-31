#!/usr/bin/env python3
"""G-04 — Clone upstream PocketSDR and build the `dump_pocketsdr_codes` harness.

PocketSDR's full libsdr build chain is heavy (libfec + libldpc + librtk +
libfftw3 + libusb). For pure code-generator interop we only need
`sdr_gen_code()` from `sdr_code.c`, which transitively uses a handful of
helpers from `sdr_cmn.c` and the memory-code tables in `sdr_code_gal.c`.

We extract those three files and compile them against our stub headers in
`scripts/interop/pocketsdr_stubs/` (fftw3.h, rtklib.h), plus a small
`pocketsdr_missing.c` that provides sdr_malloc / sdr_free / sdr_cpx_malloc /
sdr_cpx_free (which normally live in sdr_cmn.c / sdr_func.c). The resulting
binary calls `sdr_gen_code("L1CA", 5, &N)` and writes one uint8 {0,1} file
per PRN, bit-exact-comparable with LunaLink's Python generator outputs.

See docs/shaping/test-audit.md G-04.

Usage:
    python scripts/interop/build_pocketsdr.py [--clean]

Installs into: third_party/PocketSDR/
Binary:        third_party/PocketSDR/dump_pocketsdr_codes
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY = REPO_ROOT / "third_party"
POCKETSDR_DIR = THIRD_PARTY / "PocketSDR"
POCKETSDR_REPO = "https://github.com/tomojitakasu/PocketSDR.git"
STUBS_DIR = REPO_ROOT / "scripts" / "interop" / "pocketsdr_stubs"
STUBS_ACQ_DIR = REPO_ROOT / "scripts" / "interop" / "pocketsdr_stubs_acq"
HARNESS_SRC = REPO_ROOT / "scripts" / "interop" / "dump_pocketsdr_codes.c"
HARNESS_BIN = POCKETSDR_DIR / "dump_pocketsdr_codes"
ACQ_BIN = POCKETSDR_DIR / "pocket_acq"

# Upstream source files we need. sdr_code.c holds sdr_gen_code() and all its
# helpers (sdr_sig_freq, sdr_sig_boc, sdr_sat_id, sdr_res_code, sdr_code_cyc,
# sdr_sec_code, sdr_xor_bits, sdr_code_len, sdr_gen_code_fft). sdr_code_gal.c
# holds the Galileo E1/E6 memory-code hex tables referenced by sdr_code.c.
POCKETSDR_SOURCES = [
    "src/sdr_code.c",
    "src/sdr_code_gal.c",
]


def clone() -> None:
    """Clone upstream PocketSDR if not already present."""
    if POCKETSDR_DIR.exists():
        print(f"PocketSDR already cloned at {POCKETSDR_DIR}")
        return
    print("Cloning upstream PocketSDR...")
    THIRD_PARTY.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["git", "clone", "--depth", "1", POCKETSDR_REPO, str(POCKETSDR_DIR)],
        check=True,
    )


def patch_pocket_sdr_header() -> None:
    """Remove upstream includes our stubs don't cover.

    pocket_sdr.h normally pulls windows.h, CyAPI.h, libusb-1.0/libusb.h and
    pthread.h. Only pthread is available on a clean Linux box without extra
    packages; the others are guarded by WIN32 or only referenced from USB
    code we never compile. We patch them to empty `#if 0` blocks so the
    header parses cleanly against our stubs.
    """
    header = POCKETSDR_DIR / "src" / "pocket_sdr.h"
    text = header.read_text()

    unwanted = [
        "#include <windows.h>",
        "#include <CyAPI.h>",
        "#include <libusb-1.0/libusb.h>",
    ]
    changed = False
    for needle in unwanted:
        if needle in text:
            text = text.replace(needle, f"// G-04 stub: {needle}")
            changed = True

    if changed:
        header.write_text(text)
        print(f"Patched {header.name} (removed unused platform includes)")


def compile_harness() -> None:
    """Compile sdr_code.c + sdr_code_gal.c + missing.c + harness → binary."""
    src_dir = POCKETSDR_DIR / "src"
    sources = (
        [src_dir / s.split("/", 1)[1] for s in POCKETSDR_SOURCES]
        + [STUBS_DIR / "pocketsdr_missing.c"]
        + [HARNESS_SRC]
    )

    cmd = [
        "cc",
        "-O2",
        "-std=c11",
        # Our stubs must come BEFORE the upstream src/ include dir so that
        # `#include <fftw3.h>` resolves to our stub, not the system one.
        "-I",
        str(STUBS_DIR),
        "-I",
        str(src_dir),
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-sign-compare",
        "-Wno-missing-field-initializers",
        "-Wno-incompatible-pointer-types",
        "-Wno-deprecated-non-prototype",
        "-pthread",
        *[str(s) for s in sources],
        "-lm",
        "-lpthread",
        "-o",
        str(HARNESS_BIN),
    ]

    print("Compiling dump_pocketsdr_codes harness...")
    print("  " + " ".join(cmd))
    subprocess.run(cmd, check=True)
    print(f"  Built: {HARNESS_BIN}")


def fftw_flags() -> tuple[list[str], list[str]]:
    """Return (cflags, libs) for linking against system fftw3f.

    Uses pkg-config if available; falls back to common include/lib paths on
    macOS (Homebrew) and Linux (apt-get install libfftw3-dev).
    """
    try:
        cflags = (
            subprocess.check_output(
                ["pkg-config", "--cflags", "fftw3f"], stderr=subprocess.DEVNULL
            )
            .decode()
            .split()
        )
        libs = (
            subprocess.check_output(
                ["pkg-config", "--libs", "fftw3f"], stderr=subprocess.DEVNULL
            )
            .decode()
            .split()
        )
        return cflags, libs
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass

    # Homebrew fallback (Apple Silicon + Intel).
    for prefix in ("/opt/homebrew", "/usr/local"):
        inc = Path(prefix) / "include" / "fftw3.h"
        if inc.is_file():
            return (
                [f"-I{prefix}/include"],
                [f"-L{prefix}/lib", "-lfftw3f"],
            )
    # Linux default search path.
    return ([], ["-lfftw3f"])


def compile_acq_harness() -> None:
    """Compile upstream pocket_acq against real fftw3f.

    Second harness for G-06.2: feeds raw IF files to PocketSDR's acquisition
    and writes (PRN, code delay, Doppler, C/N0) lines to stdout. Used as an
    independent oracle for LunaLink's V2 correlator on real LuGRE IQ.

    Unlike dump_pocketsdr_codes, we link the real sdr_func.c and sdr_cmn.c
    (which provide sdr_malloc/sdr_free/sdr_cpx_malloc/sdr_cpx_free/
    sdr_xor_bits, among others). The G-04 stub file cannot be used here
    because it would produce duplicate symbol definitions.
    """
    src_dir = POCKETSDR_DIR / "src"
    app_src = POCKETSDR_DIR / "app" / "pocket_acq" / "pocket_acq.c"
    sources = [
        src_dir / "sdr_code.c",
        src_dir / "sdr_code_gal.c",
        src_dir / "sdr_func.c",
        src_dir / "sdr_cmn.c",
        app_src,
    ]

    cflags, libs = fftw_flags()

    # macOS needs -DMACOS for the fpos_t path in sdr_read_data().
    platform_defs = ["-DMACOS"] if sys.platform == "darwin" else []

    cmd = [
        "cc",
        "-O2",
        "-std=c11",
        # Real fftw3 first, then the acq-specific stubs dir (rtklib stub),
        # then upstream src for "pocket_sdr.h".
        *cflags,
        "-I",
        str(STUBS_ACQ_DIR),
        "-I",
        str(src_dir),
        *platform_defs,
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-sign-compare",
        "-Wno-missing-field-initializers",
        "-Wno-incompatible-pointer-types",
        "-Wno-deprecated-non-prototype",
        "-pthread",
        *[str(s) for s in sources],
        *libs,
        "-lm",
        "-lpthread",
        "-o",
        str(ACQ_BIN),
    ]

    print("Compiling pocket_acq harness...")
    print("  " + " ".join(cmd))
    subprocess.run(cmd, check=True)
    print(f"  Built: {ACQ_BIN}")


def verify() -> None:
    """Smoke test — generate GPS L1 C/A PRN 1 and assert a 1023-byte output."""
    import tempfile

    with tempfile.TemporaryDirectory(prefix="pocketsdr_verify_") as tmp:
        subprocess.run(
            [str(HARNESS_BIN), tmp, "L1CA", "1", "1"],
            check=True,
            capture_output=True,
        )
        out_file = Path(tmp) / "l1ca_prn_001.bin"
        if not out_file.is_file():
            print(f"ERROR: verify failed — {out_file} not produced", file=sys.stderr)
            sys.exit(1)
        data = out_file.read_bytes()
        if len(data) != 1023:
            print(
                f"ERROR: verify failed — L1CA PRN 1 should be 1023 chips,"
                f" got {len(data)}",
                file=sys.stderr,
            )
            sys.exit(1)
        # Must be {0, 1} only.
        if any(b not in (0, 1) for b in data):
            bad = [b for b in data if b not in (0, 1)]
            print(
                f"ERROR: verify failed — non-binary chips: {bad[:10]}", file=sys.stderr
            )
            sys.exit(1)
    print("  OK: dump_pocketsdr_codes L1CA PRN 1 → 1023 binary chips")


def main() -> None:
    """Parse arguments and run clone → patch → compile → verify pipeline."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--clean", action="store_true", help="Remove and re-clone before building"
    )
    args = parser.parse_args()

    if args.clean and POCKETSDR_DIR.exists():
        print(f"Removing {POCKETSDR_DIR}...")
        shutil.rmtree(POCKETSDR_DIR)

    clone()
    patch_pocket_sdr_header()
    compile_harness()
    verify()
    compile_acq_harness()
    verify_acq()
    print("\nG-04 + G-06.2 build OK.")


def verify_acq() -> None:
    """Smoke test — run pocket_acq -v and assert non-zero version output."""
    result = subprocess.run(
        [str(ACQ_BIN), "-v"],
        check=False,
        capture_output=True,
        text=True,
    )
    # pocket_acq -v exits via print_ver() with exit(0) and prints "pocket_acq ver.X.Y".
    if result.returncode != 0 or "pocket_acq" not in result.stdout:
        print(
            f"ERROR: pocket_acq -v failed: rc={result.returncode}\n"
            f"  stdout: {result.stdout!r}\n  stderr: {result.stderr!r}",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"  OK: {result.stdout.strip()}")


if __name__ == "__main__":
    main()
