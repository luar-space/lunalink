#!/usr/bin/env python3
"""Generate a build manifest capturing binary hashes, compiler info, and flags.

Usage:
    python3 scripts/build_manifest.py --build-dir build/cpp-tests-ninja

Output: docs/assurance/private/evidence/build_manifest.json
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys


def sha256_file(path: str) -> str:
    """Compute SHA-256 hash of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def find_artifacts(build_dir: str) -> list[dict]:
    """Find binary artifacts in the build directory."""
    targets = [
        "test_lunalink",
        "lunalink_swifi_tests",
        "liblunalink_core.a",
    ]
    # Also look for shared libraries
    so_extensions = (".so", ".dylib", ".dll")

    artifacts: list[dict] = []

    for root, _, files in os.walk(build_dir):
        # Skip _deps directory (fetched dependencies)
        if "_deps" in root:
            continue
        for name in files:
            full_path = os.path.join(root, name)
            matched = name in targets or any(
                name.endswith(ext) for ext in so_extensions
            )
            if matched and os.path.isfile(full_path):
                stat = os.stat(full_path)
                artifacts.append(
                    {
                        "name": name,
                        "sha256": sha256_file(full_path),
                        "size_bytes": stat.st_size,
                    }
                )

    # De-duplicate by name, keeping the first found
    seen: set[str] = set()
    unique: list[dict] = []
    for a in artifacts:
        if a["name"] not in seen:
            seen.add(a["name"])
            unique.append(a)

    return sorted(unique, key=lambda a: a["name"])


def parse_compiler_from_cmake_cache(build_dir: str) -> dict:  # noqa: C901
    """Extract compiler info from CMake build artifacts."""
    compiler_info: dict = {"id": "unknown", "version": "unknown", "path": "unknown"}

    # Try CMakeCache.txt first
    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    if os.path.isfile(cache_path):
        with open(cache_path) as f:
            content = f.read()

        m = re.search(r"CMAKE_CXX_COMPILER:FILEPATH=(.+)", content)
        if m:
            compiler_info["path"] = m.group(1).strip()

        m = re.search(r"CMAKE_CXX_COMPILER_ID:INTERNAL=(.+)", content)
        if m:
            compiler_info["id"] = m.group(1).strip()

        m = re.search(r"CMAKE_CXX_COMPILER_VERSION:INTERNAL=(.+)", content)
        if m:
            compiler_info["version"] = m.group(1).strip()

    # Also check CMakeCXXCompiler.cmake (more reliable for ID and version)
    for root, _, files in os.walk(os.path.join(build_dir, "CMakeFiles")):
        for name in files:
            if name == "CMakeCXXCompiler.cmake":
                cmake_compiler_path = os.path.join(root, name)
                try:
                    with open(cmake_compiler_path) as f:
                        cmake_content = f.read()
                    m = re.search(
                        r'set\(CMAKE_CXX_COMPILER_ID\s+"([^"]+)"\)', cmake_content
                    )
                    if m and compiler_info["id"] == "unknown":
                        compiler_info["id"] = m.group(1)
                    m = re.search(
                        r'set\(CMAKE_CXX_COMPILER_VERSION\s+"([^"]+)"\)',
                        cmake_content,
                    )
                    if m and compiler_info["version"] == "unknown":
                        compiler_info["version"] = m.group(1)
                except OSError:
                    pass
                break
        else:
            continue
        break

    return compiler_info


def get_compiler_version_from_binary(compiler_path: str) -> str | None:
    """Get compiler version by running it."""
    try:
        result = subprocess.run(
            [compiler_path, "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0:
            return result.stdout.strip().split("\n")[0]
    except (subprocess.SubprocessError, FileNotFoundError, OSError):
        pass
    return None


def parse_flags_from_cmake_cache(build_dir: str) -> dict:  # noqa: C901
    """Extract compile flags from CMakeCache.txt."""
    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    flags: dict = {"core": "", "hardening": ""}

    if not os.path.isfile(cache_path):
        return flags

    with open(cache_path) as f:
        content = f.read()

    # Get CMAKE_CXX_FLAGS
    m = re.search(r"CMAKE_CXX_FLAGS:STRING=(.+)", content)
    if m:
        flags["core"] = m.group(1).strip()

    # Also check compile_commands.json for more complete flags
    compile_commands = os.path.join(build_dir, "compile_commands.json")
    if os.path.isfile(compile_commands):
        try:
            with open(compile_commands) as f:
                commands = json.load(f)
            # Find a lunalink_core source file command
            for cmd in commands:
                src = cmd.get("file", "")
                if (
                    any(
                        d in src
                        for d in (
                            "cpp/codes/",
                            "cpp/encoding/",
                            "cpp/messages/",
                            "cpp/signal/",
                            "cpp/platform/",
                        )
                    )
                    and "test" not in src
                ):
                    command_str = cmd.get("command", "")
                    # Extract safety-related flags
                    core_flags: list[str] = []
                    hardening_flags: list[str] = []
                    for flag in command_str.split():
                        if flag in (
                            "-fno-exceptions",
                            "-fno-rtti",
                            "-fno-fast-math",
                            "-ffp-contract=off",
                        ):
                            core_flags.append(flag)
                        elif flag.startswith(
                            ("-fstack-protector", "-fno-common", "-D_FORTIFY_SOURCE")
                        ):
                            hardening_flags.append(flag)
                        elif flag == "-U_FORTIFY_SOURCE":
                            hardening_flags.append(flag)
                    if core_flags:
                        flags["core"] = " ".join(core_flags)
                    if hardening_flags:
                        flags["hardening"] = " ".join(hardening_flags)
                    break
        except (json.JSONDecodeError, OSError):
            pass

    return flags


def get_git_sha() -> str:
    """Get the current HEAD SHA."""
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "HEAD"], stderr=subprocess.DEVNULL
            )
            .decode()
            .strip()
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def main() -> None:  # noqa: D103
    parser = argparse.ArgumentParser(
        description="Generate build manifest evidence artifact"
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Build directory (default: build)",
    )
    parser.add_argument(
        "--output",
        default="docs/assurance/private/evidence/build_manifest.json",
        help="Output JSON path",
    )
    args = parser.parse_args()

    if not os.path.isdir(args.build_dir):
        print(f"ERROR: Build directory not found: {args.build_dir}", file=sys.stderr)
        sys.exit(1)

    # Gather compiler info
    compiler = parse_compiler_from_cmake_cache(args.build_dir)

    # Try to get version from binary if not in cache
    if compiler["version"] == "unknown" and compiler["path"] != "unknown":
        version_str = get_compiler_version_from_binary(compiler["path"])
        if version_str:
            compiler["version"] = version_str

    # Gather build type from CMakeCache
    build_type = "unknown"
    cache_path = os.path.join(args.build_dir, "CMakeCache.txt")
    if os.path.isfile(cache_path):
        with open(cache_path) as f:
            for line in f:
                m = re.match(r"CMAKE_BUILD_TYPE:STRING=(.+)", line)
                if m:
                    build_type = m.group(1).strip()
                    break

    # Gather flags
    flags = parse_flags_from_cmake_cache(args.build_dir)

    # Find and hash artifacts
    artifacts = find_artifacts(args.build_dir)

    manifest = {
        "generated": datetime.datetime.now(datetime.UTC).isoformat(),
        "git_sha": get_git_sha(),
        "build_type": build_type,
        "compiler": compiler,
        "flags": flags,
        "artifacts": artifacts,
    }

    # Write output
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print(f"Build manifest: {args.output}")
    print(f"  Compiler: {compiler['id']} {compiler['version']}")
    print(f"  Artifacts: {len(artifacts)}")
    for a in artifacts:
        print(f"    {a['name']}: {a['sha256'][:16]}... ({a['size_bytes']} bytes)")


if __name__ == "__main__":
    main()
