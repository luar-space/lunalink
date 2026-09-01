#!/usr/bin/env bash
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
#
# Fresh-export gate.
#
# `task ci-checks` runs against the WORKING TREE, so a file that exists on disk
# but was never `git add`-ed passes every check while being absent from what a
# judge or user actually clones.  That happened: docs/gateway8.rst,
# docs/assurance/coverage_evidence.rst and evidence/coverage_summary.json were
# untracked while the docs cross-referenced them.
#
# This script extracts the committed tree with `git archive` — which sees only
# tracked content, exactly like a clone — and builds and tests it in isolation.
#
# Usage:  scripts/validate_export.sh [git-ref]        (default: HEAD)
set -euo pipefail

REF="${1:-HEAD}"
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

echo "=== Fresh-export validation of ${REF} ==="
echo "    Export dir: ${WORKDIR}"
echo ""

# 1. Working tree must be clean.  A dirty tree means the thing being validated
#    is not the thing that was committed.
if [ -n "$(git status --porcelain)" ]; then
    echo "FAIL: working tree is not clean."
    echo ""
    git status --short
    echo ""
    echo "Commit or stash before validating the export — otherwise this gate"
    echo "validates a tree nobody else can obtain."
    exit 1
fi
echo "Working tree clean: OK"

# 2. Extract only what is committed.
git archive "$REF" | tar -x -C "$WORKDIR"
echo "Exported $(find "$WORKDIR" -type f | wc -l | tr -d ' ') tracked files"

cd "$WORKDIR"

# 3. Every document named in a Sphinx toctree must exist in the export.
#    This is the specific failure mode that motivated the script.
python3 - <<'PY'
import re, sys
from pathlib import Path

missing = []
for rst in Path("docs").rglob("*.rst"):
    text = rst.read_text(encoding="utf-8")
    for block in re.finditer(r"^\.\. toctree::\n((?:[ \t]+.*\n|\n)*)", text, re.M):
        for line in block.group(1).splitlines():
            entry = line.strip()
            if not entry or entry.startswith(":"):
                continue
            entry = entry.split("<")[-1].rstrip(">")
            if entry.startswith(("http", "#")):
                continue
            base = (rst.parent / entry).resolve()
            if not any(
                base.with_suffix(s).exists() for s in (".rst", ".md", ".ipynb")
            ):
                missing.append(f"{rst}: toctree entry '{entry}' has no source file")

if missing:
    print("FAIL: toctree entries missing from the committed tree:")
    for m in missing:
        print(f"  {m}")
    sys.exit(1)
print("Toctree completeness: OK")
PY

# 4. Build and test the export.  Reuse the parent checkout's fetched Catch2
#    source if present so the gate does not require network on every run; a
#    cold machine falls back to FetchContent's clone.
CATCH2_SRC="${REPO_ROOT}/build/cpp-tests-ninja/_deps/catch2-src"
CATCH2_ARG=()
if [ -d "$CATCH2_SRC" ]; then
    CATCH2_ARG=(-DFETCHCONTENT_SOURCE_DIR_CATCH2="$CATCH2_SRC")
    echo "Reusing Catch2 source from parent checkout (offline)"
fi

GENERATOR="Unix Makefiles"
command -v ninja >/dev/null 2>&1 && GENERATOR="Ninja"

echo ""
echo "=== Configuring export ==="
cmake -B build/export -S . -G "$GENERATOR" \
    -DBUILD_PYTHON_BINDINGS=OFF -DBUILD_TESTING=ON \
    -DCMAKE_BUILD_TYPE=Debug "${CATCH2_ARG[@]}" >/dev/null

echo "=== Building export ==="
cmake --build build/export --parallel >/dev/null

echo "=== Testing export ==="
build/export/test_lunalink "~[slow]" --rng-seed 0 --reporter compact | tail -3

echo ""
echo "PASS: ${REF} builds and tests clean from a committed-files-only export."
