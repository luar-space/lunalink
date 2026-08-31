#!/usr/bin/env python3
"""Apply retrospective version and gateway tags to historical commits.

One-time script. Safe to re-run — skips tags that already exist.
"""

from __future__ import annotations

import argparse
import os
import subprocess

# (tag, commit_short_sha, annotation message)
VERSION_TAGS: list[tuple[str, str, str]] = [
    # v0.1.0 already exists (a397ed5)
    (
        "v0.1.1",
        "622f0be",
        "G1+G4: spreading code generators and baseband signal generation",
    ),
    (
        "v0.1.2",
        "e648a97",
        "G2: forward error correction — BCH, LDPC, interleaver, CRC-24",
    ),
    (
        "v0.1.3",
        "923a7e7",
        "G3: message framing — subframe builders and frame assembler",
    ),
    (
        "v0.1.4",
        "2f4bb66",
        "G5: frame sync & decoding — correlator, sync detector, RX pipeline",
    ),
    (
        "v0.1.5",
        "12a5993",
        "Certification V1-V3: OSS export, traceability, evidence artifacts",
    ),
    (
        "v0.1.6",
        "e0e9821",
        "Certification V4: zero-warning docs, compliance matrix, cross-ref audit",
    ),
    ("v0.1.7", "1d5c04e", "Restructure: C++ sources reorganised by functional domain"),
    ("v0.1.8", "3b0c55a", "MISRA C++:2023 compliance + battle-hardened test suites"),
]

GATEWAY_TAGS: list[tuple[str, str, str]] = [
    (
        "gateway/g0",
        "fc2c6f9",
        "Gateway 0 complete: standards compliance & architecture",
    ),
    ("gateway/g1", "622f0be", "Gateway 1 complete: spreading code generation"),
    ("gateway/g4", "622f0be", "Gateway 4 complete: baseband signal generation"),
    ("gateway/g2", "e648a97", "Gateway 2 complete: forward error correction"),
    ("gateway/g3", "923a7e7", "Gateway 3 complete: message framing"),
    ("gateway/g5", "2f4bb66", "Gateway 5 complete: frame sync & decoding"),
]


def tag_exists(tag: str) -> bool:
    """Return True if the tag already exists."""
    result = subprocess.run(
        ["git", "rev-parse", tag],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0


def get_commit_date(commit: str) -> str:
    """Return the author date of a commit in ISO format."""
    result = subprocess.run(
        ["git", "log", "-1", "--format=%aI", commit],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def create_tag(tag: str, commit: str, message: str, *, dry_run: bool) -> None:
    """Create an annotated tag backdated to the commit's author date."""
    if tag_exists(tag):
        print(f"  SKIP  {tag:20s}  (already exists)")
        return

    if dry_run:
        print(f'  WOULD {tag:20s}  -> {commit}  "{message}"')
        return

    date = get_commit_date(commit)
    env = {**os.environ, "GIT_COMMITTER_DATE": date}
    subprocess.run(
        ["git", "tag", "-a", tag, commit, "-m", message],
        env=env,
        check=True,
    )
    print(f'  TAG   {tag:20s}  -> {commit}  "{message}"')


def main() -> None:
    """Apply retrospective version and gateway tags."""
    parser = argparse.ArgumentParser(
        description="Apply retrospective version and gateway tags.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would happen without creating tags.",
    )
    args = parser.parse_args()

    if args.dry_run:
        print("=== DRY RUN ===\n")

    print("Version tags:")
    for tag, commit, msg in VERSION_TAGS:
        create_tag(tag, commit, msg, dry_run=args.dry_run)

    print("\nGateway annotation tags:")
    for tag, commit, msg in GATEWAY_TAGS:
        create_tag(tag, commit, msg, dry_run=args.dry_run)

    print()
    if args.dry_run:
        print("(dry run — no tags created)")
    else:
        print("Done. Review with:")
        fmt = "%(refname:short) %(creatordate:short) %(subject)"
        print(f"  git tag -l --sort=creatordate --format='{fmt}'")
        print("\nPush with:")
        print("  git push origin --tags")


if __name__ == "__main__":
    main()
