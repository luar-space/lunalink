#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Create the next semver tag based on the latest v* tag reachable from HEAD."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys


def run_git(*args: str, check: bool = True) -> str:
    """Run a git command and return stripped stdout."""
    result = subprocess.run(
        ["git", *args],
        capture_output=True,
        text=True,
        check=check,
    )
    return result.stdout.strip()


def get_latest_version_tag() -> str | None:
    """Return the latest v* tag reachable from HEAD, or None."""
    try:
        return run_git("describe", "--tags", "--match", "v*", "--abbrev=0")
    except subprocess.CalledProcessError:
        return None


def parse_version(tag: str) -> tuple[int, int, int]:
    """Extract (major, minor, patch) from a tag like 'v1.2.3'."""
    m = re.match(r"^v(\d+)\.(\d+)\.(\d+)$", tag)
    if not m:
        print(f"ERROR: cannot parse version from tag '{tag}'", file=sys.stderr)
        sys.exit(1)
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def main() -> None:
    """Create the next semver tag based on the latest v* tag."""
    parser = argparse.ArgumentParser(description="Create the next semver tag.")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would happen without creating a tag.",
    )
    parser.add_argument(
        "--push",
        action="store_true",
        help="Push the new tag to origin after creation.",
    )
    parser.add_argument(
        "--minor",
        action="store_true",
        help="Bump minor version instead of patch (resets patch to 0).",
    )
    args = parser.parse_args()

    latest = get_latest_version_tag()
    if latest is None:
        print("No v* tags found. Create an initial tag first (e.g. v0.1.0).")
        sys.exit(1)

    major, minor, patch = parse_version(latest)

    if args.minor:
        minor += 1
        patch = 0
    else:
        patch += 1

    new_tag = f"v{major}.{minor}.{patch}"

    # Use HEAD commit's first line as tag message for traceability
    head_subject = run_git("log", "-1", "--format=%s")
    message = f"{new_tag}: {head_subject}"

    print(f"Latest tag : {latest}")
    print(f"Bump       : {'minor' if args.minor else 'patch'}")
    print(f"New tag    : {new_tag}")
    print(f"Message    : {message}")

    if args.dry_run:
        print("\n(dry run — no tag created)")
        return

    run_git("tag", "-a", new_tag, "-m", message)
    print(f"\nCreated annotated tag {new_tag}")

    if args.push:
        run_git("push", "origin", new_tag)
        print(f"Pushed {new_tag} to origin")


if __name__ == "__main__":
    main()
