"""Verify requirement-to-test traceability for LSIS-AFS and SW-SAF tags.

Scans C++ source and header files for [LSIS-AFS-*] and [SW-SAF-*] requirement
tags, then checks that every tagged requirement has at least one matching test.

Outputs:
  - Human-readable summary to stdout
  - Machine-readable JSON to docs/assurance/private/evidence/traceability.json

Exit codes:
  - 0: all requirements covered (or --strict not set)
  - 1: gaps found and --strict is set

Uses only Python stdlib.  No third-party dependencies.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from datetime import UTC, datetime

# Regex for requirement tags: [LSIS-AFS-101], [SW-SAF-01], etc.
TAG_RE = re.compile(r"\[(?:LSIS-AFS-\d+|SW-SAF-\d+)\]")


def find_files(root, dirs, extensions):
    """Walk directories under root and yield files matching extensions."""
    for d in dirs:
        dirpath = os.path.join(root, d)
        if not os.path.isdir(dirpath):
            continue
        for dirpath_walk, _, filenames in os.walk(dirpath):
            for fn in filenames:
                if any(fn.endswith(ext) for ext in extensions):
                    yield os.path.join(dirpath_walk, fn)


def extract_tags(filepath):
    """Return set of requirement tags found in a file."""
    tags = set()
    with open(filepath, encoding="utf-8", errors="replace") as f:
        for line in f:
            for match in TAG_RE.findall(line):
                # Strip brackets: [LSIS-AFS-101] -> LSIS-AFS-101
                tags.add(match[1:-1])
    return tags


def relative_path(filepath, root):
    """Return path relative to repo root."""
    return os.path.relpath(filepath, root)


def git_sha(root):
    """Return current HEAD SHA, or 'unknown' if not in a git repo."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=root,
            capture_output=True,
            text=True,
            timeout=5,
        )
        return result.stdout.strip() if result.returncode == 0 else "unknown"
    except (subprocess.SubprocessError, FileNotFoundError):
        return "unknown"


def main():  # noqa: C901
    """Scan codebase for requirement tags and verify traceability."""
    parser = argparse.ArgumentParser(
        description="Verify requirement traceability (LSIS-AFS / SW-SAF tags)."
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Repository root directory (default: parent of scripts/).",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit 1 if any requirement tag lacks a matching test.",
    )
    args = parser.parse_args()

    # Determine repo root
    if args.repo_root:
        root = os.path.abspath(args.repo_root)
    else:
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    # --- Scan code files (headers + sources + bindings) ---
    code_dirs = [
        "cpp/include",
        "cpp/codes",
        "cpp/encoding",
        "cpp/messages",
        "cpp/signal",
        "cpp/platform",
        "cpp/bindings",
    ]
    code_extensions = [".hpp", ".cpp"]
    # tag -> set of relative file paths
    code_tags = defaultdict(set)

    for fpath in find_files(root, code_dirs, code_extensions):
        tags = extract_tags(fpath)
        rel = relative_path(fpath, root)
        for tag in tags:
            code_tags[tag].add(rel)

    # --- Scan test files ---
    test_dirs = ["cpp/tests"]
    test_extensions = [".cpp"]
    # tag -> set of relative file paths
    test_tags = defaultdict(set)

    for fpath in find_files(root, test_dirs, test_extensions):
        tags = extract_tags(fpath)
        rel = relative_path(fpath, root)
        for tag in tags:
            test_tags[tag].add(rel)

    # --- Build requirements map ---
    all_code_tags = sorted(code_tags.keys())
    all_test_tags = sorted(test_tags.keys())

    requirements = {}
    for tag in all_code_tags:
        covered = tag in test_tags
        requirements[tag] = {
            "code_files": sorted(code_tags[tag]),
            "test_files": sorted(test_tags.get(tag, set())),
            "covered": covered,
        }

    # Orphaned test tags (in tests but not in code)
    orphaned = sorted(set(all_test_tags) - set(all_code_tags))

    total = len(all_code_tags)
    covered_count = sum(1 for r in requirements.values() if r["covered"])
    uncovered_count = total - covered_count
    coverage_pct = round(100.0 * covered_count / total, 1) if total > 0 else 0.0

    summary = {
        "total_requirements": total,
        "covered": covered_count,
        "uncovered": uncovered_count,
        "coverage_pct": coverage_pct,
    }

    # --- Build JSON output ---
    output = {
        "generated": datetime.now(UTC).isoformat(),
        "git_sha": git_sha(root),
        "requirements": requirements,
        "orphaned_test_tags": orphaned,
        "summary": summary,
    }

    # Write JSON
    json_dir = os.path.join(root, "docs", "assurance", "private", "evidence")
    os.makedirs(json_dir, exist_ok=True)
    json_path = os.path.join(json_dir, "traceability.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)
        f.write("\n")

    # --- Human-readable output ---
    print("=" * 70)
    print("  Requirement Traceability Report")
    print("=" * 70)
    print()
    print(f"  Repository root : {root}")
    print(f"  Git SHA          : {output['git_sha'][:12]}")
    print(f"  Generated        : {output['generated']}")
    print()

    # Covered requirements
    print("  COVERED REQUIREMENTS")
    print("  " + "-" * 66)
    for tag in all_code_tags:
        if requirements[tag]["covered"]:
            code = ", ".join(requirements[tag]["code_files"])
            tests = ", ".join(requirements[tag]["test_files"])
            print(f"  [OK]  {tag}")
            print(f"         code : {code}")
            print(f"         tests: {tests}")

    print()

    # Uncovered requirements
    if uncovered_count > 0:
        print("  UNCOVERED REQUIREMENTS (gaps)")
        print("  " + "-" * 66)
        for tag in all_code_tags:
            if not requirements[tag]["covered"]:
                code = ", ".join(requirements[tag]["code_files"])
                print(f"  [GAP] {tag}")
                print(f"         code : {code}")
        print()

    # Orphaned test tags
    if orphaned:
        print("  ORPHANED TEST TAGS (in tests but not in code)")
        print("  " + "-" * 66)
        for tag in orphaned:
            tests = ", ".join(test_tags[tag])
            print(f"  [???] {tag}")
            print(f"         tests: {tests}")
        print()

    # Summary
    print("  SUMMARY")
    print("  " + "-" * 66)
    print(f"  Total requirements : {total}")
    print(f"  Covered            : {covered_count}")
    print(f"  Uncovered          : {uncovered_count}")
    print(f"  Coverage           : {coverage_pct}%")
    print(f"  Orphaned test tags : {len(orphaned)}")
    print()
    print(f"  JSON written to: {json_path}")
    print("=" * 70)

    # Exit code
    if args.strict and uncovered_count > 0:
        print(f"\n  FAIL: {uncovered_count} requirement(s) lack test coverage.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
