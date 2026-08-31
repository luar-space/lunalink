#!/usr/bin/env bash
# Capture a minimal hardware/build fingerprint for benchmark provenance.
# Writes a JSON document to the path given as $1.
set -euo pipefail

out_path="${1:?usage: write_hw_fingerprint.sh <out.json>}"

# Anchor to the repo root so the ``git rev-parse`` and any relative
# ``out_path`` resolve regardless of the caller's working directory.
# Falls back to the script's own dir if not inside a git worktree (e.g.
# packaged release tarball).
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." 2>/dev/null && git rev-parse --show-toplevel 2>/dev/null || echo "$script_dir/..")"
cd "$repo_root"

platform="$(uname -s)"
arch="$(uname -m)"

chip="unknown"
cores_p="0"
cores_e="0"
macos_version=""
xcode_version=""

if [[ "$platform" == "Darwin" ]]; then
    chip="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo unknown)"
    cores_p="$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null || echo 0)"
    cores_e="$(sysctl -n hw.perflevel1.physicalcpu 2>/dev/null || echo 0)"
    macos_version="$(sw_vers -productVersion 2>/dev/null || true)"
    # ``xcodebuild`` requires Xcode (not just CLT); fall back to the
    # CLT compiler banner so the field is never silently empty on a
    # CLT-only host.  The label still reads ``xcode_version`` for
    # backwards-compat with prior baselines.
    xcode_version="$(xcodebuild -version 2>/dev/null | head -1 || true)"
    if [[ -z "$xcode_version" ]]; then
        xcode_version="$(clang --version 2>/dev/null | head -1 || true)"
    fi
fi

# Fail loudly if ``git`` is unavailable or the cwd is outside a worktree.
# A "commit_sha":"unknown" baseline is unactionable for provenance and
# the script is only meant to run from ``task bench-campaign`` inside the
# repo — so a missing SHA always indicates a configuration error worth
# surfacing.
commit_sha="$(git rev-parse HEAD 2>/dev/null)" || {
    echo "ERROR: write_hw_fingerprint.sh must run inside a git worktree" >&2
    echo "       (cwd=$(pwd) is not a git repository)." >&2
    exit 1
}
captured_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

# --- Build provenance --------------------------------------------------------
# Compiler identity + version for the C++ toolchain that produced the
# bench binaries.  ``CXX`` env var wins (matches what scikit-build-core /
# CMake actually invoked); fall back to ``c++`` on PATH.
cxx_bin="${CXX:-c++}"
cxx_path="$(command -v "$cxx_bin" 2>/dev/null || echo "$cxx_bin")"
cxx_version_full=""
if command -v "$cxx_bin" >/dev/null 2>&1; then
    cxx_version_full="$("$cxx_bin" --version 2>/dev/null | head -1 || true)"
fi

# CMake flags actually used for the bench build, sourced from the
# environment.  ``bench-install`` in Taskfile.yml exports
# ``CMAKE_ARGS`` with ``-DLUNALINK_BENCH_COUNT_FFTS=ON`` and
# ``-DLUNALINK_DEV_RX_FFT_CACHE=ON``; capture verbatim so the baseline
# JSON answers "which flags produced this evidence?" without ambiguity.
cmake_args="${CMAKE_ARGS:-}"
cmake_build_type="${CMAKE_BUILD_TYPE:-}"

cmake_version=""
if command -v cmake >/dev/null 2>&1; then
    cmake_version="$(cmake --version 2>/dev/null | head -1 || true)"
fi

# JSON-safe escaping for fields that may contain double quotes or
# backslashes (e.g. CXX path on weird mounts).
json_escape() {
    python3 -c 'import json,sys; sys.stdout.write(json.dumps(sys.argv[1]))' "$1"
}

cxx_path_json="$(json_escape "$cxx_path")"
cxx_version_json="$(json_escape "$cxx_version_full")"
cmake_args_json="$(json_escape "$cmake_args")"
cmake_build_type_json="$(json_escape "$cmake_build_type")"
cmake_version_json="$(json_escape "$cmake_version")"
# External-tool strings that may contain quotes or backslashes — escape
# them so a future chip name or Xcode banner cannot break the JSON.
platform_json="$(json_escape "$platform")"
arch_json="$(json_escape "$arch")"
chip_json="$(json_escape "$chip")"
macos_version_json="$(json_escape "$macos_version")"
xcode_version_json="$(json_escape "$xcode_version")"
commit_sha_json="$(json_escape "$commit_sha")"
captured_at_json="$(json_escape "$captured_at")"

cat > "$out_path" <<EOF
{
  "platform": $platform_json,
  "arch": $arch_json,
  "chip": $chip_json,
  "cores_p": $cores_p,
  "cores_e": $cores_e,
  "macos_version": $macos_version_json,
  "xcode_version": $xcode_version_json,
  "cxx_path": $cxx_path_json,
  "cxx_version": $cxx_version_json,
  "cmake_version": $cmake_version_json,
  "cmake_args": $cmake_args_json,
  "cmake_build_type": $cmake_build_type_json,
  "commit_sha": $commit_sha_json,
  "captured_at_utc": $captured_at_json
}
EOF

echo "Wrote $out_path"
