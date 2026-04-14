#!/usr/bin/env python3
"""G-15 sanity gate — verify interop tests are not silently skipped in CI.

Every `tests/test_interop_*.py` file is guarded by
`pytest.mark.skipif(not BIN.is_file(), ...)`. If CI doesn't build the
third-party binaries before pytest runs, those tests silently provide
zero coverage — which is the single most important finding from the
2026-04-12 test audit (see docs/shaping/test-audit.md G-15).

This script runs after the LANS-AFS-SIM build step and asserts:

1. Each expected binary exists and is executable.
2. Each interop test file collects at least MIN_TESTS_PER_FILE tests
   when collected with pytest (no module-level skip applied).

Exit 0 on success; exit 1 with a diagnostic on any failure.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LANS_DIR = REPO_ROOT / "third_party" / "LANS-AFS-SIM"

POCKETSDR_DIR = REPO_ROOT / "third_party" / "PocketSDR"

REQUIRED_BINARIES = [
    LANS_DIR / "afs_sim",
    LANS_DIR / "dump_lans_codes",
    LANS_DIR / "dump_lans_frame",
    POCKETSDR_DIR / "dump_pocketsdr_codes",
    POCKETSDR_DIR / "pocket_acq",  # G-06.2 correlator cross-decode
]

# (test file, minimum collected count). Thresholds picked well below the
# current count to tolerate normal churn but catch wholesale erosion.
INTEROP_TEST_FLOORS: list[tuple[str, int]] = [
    ("tests/test_interop_codes.py", 400),  # 420 parameterised currently
    ("tests/test_interop_frames.py", 4),  # 5 cases currently
    ("tests/test_interop_decode.py", 4),  # 4 live + 1 skipped (G-06.1)
    ("tests/test_interop_pocketsdr_codes.py", 20),  # 25 parameterised currently
    (
        "tests/test_interop_acquire_pocketsdr.py",
        4,
    ),  # 3 parametrised + sign gate (G-06.2)
]


def check_binaries() -> list[str]:
    """Return a list of missing or non-executable interop binary diagnostics."""
    errors: list[str] = []
    for binary in REQUIRED_BINARIES:
        if not binary.is_file():
            errors.append(f"missing binary: {binary.relative_to(REPO_ROOT)}")
        elif not binary.stat().st_mode & 0o111:
            errors.append(f"not executable: {binary.relative_to(REPO_ROOT)}")
    return errors


def count_collected(test_path: str) -> int:
    """Return the number of tests pytest collects from `test_path`.

    A module-level `skipif` that fires prevents collection entirely, so a
    zero count catches the silent-skip regression this gate exists to
    prevent.
    """
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            test_path,
            "--collect-only",
            "-q",
            "--no-header",
            "--no-cov",
            "-p",
            "no:cacheprovider",
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    # We intentionally ignore returncode: pyproject config may enforce
    # post-collection gates (e.g. coverage thresholds) that fail the run
    # even when collection succeeds. The collection summary line is
    # authoritative.
    for line in result.stdout.splitlines():
        stripped = line.strip()
        # Matches both "5 tests collected in 0.12s" and "5 tests collected".
        if "tests collected" in stripped or "test collected" in stripped:
            first = stripped.split()[0]
            if first.isdigit():
                return int(first)
    # Nothing matched — dump output so CI logs show what happened.
    print(f"Could not parse collection count for {test_path}:", file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    print(result.stderr, file=sys.stderr)
    return -1


def main() -> int:
    """Run the G-15 gate; return 0 on pass, 1 on fail."""
    errors = check_binaries()
    if errors:
        print("G-15 FAIL: third-party interop binaries missing.", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        print(
            "Run: uv run python scripts/interop/build_lans_afs_sim.py",
            file=sys.stderr,
        )
        return 1

    print("Interop binaries present:")
    for binary in REQUIRED_BINARIES:
        print(f"  OK: {binary.relative_to(REPO_ROOT)}")

    print("\nCollecting interop tests to verify no silent skips...")
    any_fail = False
    for test_path, floor in INTEROP_TEST_FLOORS:
        count = count_collected(test_path)
        if count < 0:
            any_fail = True
            continue
        status = "OK" if count >= floor else "FAIL"
        print(f"  {status}: {test_path} — {count} collected (floor {floor})")
        if count < floor:
            any_fail = True

    if any_fail:
        print(
            "\nG-15 FAIL: one or more interop test files collect fewer tests than "
            "the floor. This usually means a module-level skipif fired — check that "
            "third-party binaries are built and DUMP_BIN paths are correct.",
            file=sys.stderr,
        )
        return 1

    print("\nG-15 OK: interop tests are live and will run.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
