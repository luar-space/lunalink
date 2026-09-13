#!/usr/bin/env bash
# Verify that the reference documents vendored in this directory are
# byte-identical to the copies NASA publishes.
#
# Usage:  bash docs/references/verify_references.sh
#
# Exits non-zero if any vendored copy has drifted from its published source,
# which means either the local file was modified or NASA reissued the
# document. See README.md in this directory before changing anything.

set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"

SPEC_NAME="lunanet-signal-in-space-recommended-standard-augmented-forward-signal-vol-a.pdf"
SPEC_URL="https://www.nasa.gov/wp-content/uploads/2025/02/${SPEC_NAME}"
SPEC_SHA256="986e07959f527d24280d87b5477298424ffdf754c4625f08041b5749592e61b6"

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

fail=0

echo "=== Vendored copy ==="
local_sha="$(sha256 "$DIR/$SPEC_NAME")"
if [ "$local_sha" = "$SPEC_SHA256" ]; then
  echo "  ok       $SPEC_NAME matches the recorded checksum"
else
  echo "  MISMATCH $SPEC_NAME"
  echo "    expected $SPEC_SHA256"
  echo "    actual   $local_sha"
  fail=1
fi

if [ "${OFFLINE:-0}" = "1" ]; then
  echo ""
  echo "OFFLINE=1 set; skipping the re-download check."
  exit "$fail"
fi

echo ""
echo "=== Published source ==="
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if curl -fsSL -o "$tmp/$SPEC_NAME" "$SPEC_URL"; then
  remote_sha="$(sha256 "$tmp/$SPEC_NAME")"
  if [ "$remote_sha" = "$SPEC_SHA256" ]; then
    echo "  ok       NASA still publishes the same bytes"
  else
    echo "  REISSUED NASA now publishes different bytes"
    echo "    recorded $SPEC_SHA256"
    echo "    upstream $remote_sha"
    echo "    Do not overwrite the vendored copy silently: the implementation"
    echo "    is traced against the recorded baseline. Open an issue."
    fail=1
  fi
else
  echo "  skipped  could not reach $SPEC_URL"
fi

echo ""
if [ "$fail" -eq 0 ]; then
  echo "All reference documents verified."
else
  echo "Reference verification FAILED."
fi
exit "$fail"
