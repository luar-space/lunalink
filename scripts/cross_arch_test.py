#!/usr/bin/env python3
"""Cross-architecture build & test orchestrator.

Uses Docker buildx + QEMU emulation to build and test the C++ core on
multiple architectures without cross-compilers or toolchain files.

Usage:
    python3 scripts/cross_arch_test.py              # full matrix
    python3 scripts/cross_arch_test.py --arch arm64  # single arch
    python3 scripts/cross_arch_test.py --dry-run     # preview commands
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time

PLATFORMS: dict[str, dict[str, str]] = {
    "arm64": {
        "docker_platform": "linux/arm64",
        "endian": "LE",
        "alignment": "Strict",
        "purpose": "ARM validation, Kria KR260 proxy",
    },
    "s390x": {
        "docker_platform": "linux/s390x",
        "endian": "BE",
        "alignment": "Strict",
        "purpose": "Endianness + alignment validation",
    },
}

BUILDER_NAME = "lunalink-cross-arch"


def check_prerequisites() -> None:  # noqa: D103
    if not shutil.which("docker"):
        print("ERROR: docker not found in PATH", file=sys.stderr)
        sys.exit(1)

    result = subprocess.run(
        ["docker", "buildx", "version"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("ERROR: docker buildx not available", file=sys.stderr)
        sys.exit(1)


def ensure_builder() -> None:
    """Create or reuse a buildx builder with QEMU support."""
    result = subprocess.run(
        ["docker", "buildx", "inspect", BUILDER_NAME],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"Creating buildx builder '{BUILDER_NAME}' with QEMU...")
        subprocess.run(
            [
                "docker",
                "buildx",
                "create",
                "--name",
                BUILDER_NAME,
                "--driver",
                "docker-container",
                "--bootstrap",
            ],
            check=True,
        )
    else:
        print(f"Using existing builder '{BUILDER_NAME}'")


def run_build(platform_key: str, dry_run: bool) -> tuple[str, bool, float]:
    """Build and test for a single platform. Returns (arch, passed, duration_s)."""
    info = PLATFORMS[platform_key]
    docker_platform = info["docker_platform"]

    cmd = [
        "docker",
        "buildx",
        "build",
        "--builder",
        BUILDER_NAME,
        "--platform",
        docker_platform,
        "--progress=plain",
        "--no-cache",
        "-f",
        "Dockerfile.cross-arch",
        ".",
    ]

    print(f"\n{'=' * 60}")
    print(f"  {platform_key} ({docker_platform})")
    print(f"  Endian: {info['endian']}  Alignment: {info['alignment']}")
    print(f"  {info['purpose']}")
    print(f"{'=' * 60}")

    if dry_run:
        print(f"  [DRY RUN] {' '.join(cmd)}")
        return platform_key, True, 0.0

    t0 = time.monotonic()
    result = subprocess.run(cmd)
    duration = time.monotonic() - t0

    return platform_key, result.returncode == 0, duration


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Cross-architecture build & test for LunaLink C++ core"
    )
    parser.add_argument(
        "--arch",
        choices=list(PLATFORMS.keys()),
        help="Run a single architecture (default: full matrix)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without executing",
    )
    args = parser.parse_args()

    targets = [args.arch] if args.arch else list(PLATFORMS.keys())

    if not args.dry_run:
        check_prerequisites()
        ensure_builder()

    results: list[tuple[str, bool, float]] = []
    for target in targets:
        results.append(run_build(target, args.dry_run))

    # Summary table
    print(f"\n{'=' * 60}")
    print("  Cross-Architecture Test Summary")
    print(f"{'=' * 60}")
    print(f"  {'Arch':<10} {'Platform':<16} {'Endian':<8} {'Result':<10} {'Time'}")
    print(f"  {'-' * 10} {'-' * 16} {'-' * 8} {'-' * 10} {'-' * 8}")
    all_passed = True
    for arch, passed, duration in results:
        info = PLATFORMS[arch]
        status = "PASS" if passed else "FAIL"
        time_str = f"{duration:.0f}s" if duration > 0 else "—"
        print(
            f"  {arch:<10} {info['docker_platform']:<16}"
            f" {info['endian']:<8} {status:<10} {time_str}"
        )
        if not passed:
            all_passed = False
    print()

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
