#!/usr/bin/env python3
"""
check_build_flags.py — SWE-109 compile-flag gate.

Reads compile_commands.json and verifies that every translation unit under
the core source directories carries required hardening
flags, is free of forbidden compiler flags, and is free of forbidden
preprocessor defines (dev/bench-only macros that must never reach a flight
build — see ``docs/assurance/build_policy.rst``).

Usage:
    python scripts/check_build_flags.py [path/to/compile_commands.json]

Exits with code 0 if all units pass, 1 if any violation is found.
"""

import json
import pathlib
import re
import sys

REQUIRED_FLAGS = [
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-fast-math",
    "-fstack-protector-strong",
    "-Werror",
]

FORBIDDEN_FLAGS = [
    "-ffast-math",
    "-funsafe-math-optimizations",
    "-fstrict-aliasing",
    "-funroll-loops",
    "-fomit-frame-pointer",
    "-fdelete-null-pointer-checks",
]

# Preprocessor defines that gate dev/bench-only code paths. They add heap
# allocation, atomic counters, or other behaviour that must never be present
# in a flight binary. Catalogue is normative — see
# ``docs/assurance/build_policy.rst``. Currently empty: no dev/bench-only
# macro exists in this tree, but the gate stays wired so any future one
# must be registered here to be caught.
FORBIDDEN_DEFINES: list[str] = []


# Match -DNAME or -DNAME=VALUE as a standalone token.
_DEFINE_RE = re.compile(r"^-D([A-Za-z_][A-Za-z0-9_]*)(?:=.*)?$")


def _extract_defines(flags: list[str]) -> set[str]:
    """Return the set of preprocessor macro names defined on the command line."""
    out: set[str] = set()
    for tok in flags:
        m = _DEFINE_RE.match(tok)
        if m:
            out.add(m.group(1))
    return out


def check_unit(entry: dict) -> list[str]:
    """Return a list of violation strings for one compile_commands entry."""
    command = entry.get("command", "") or " ".join(entry.get("arguments", []))
    flags = command.split()

    violations: list[str] = []

    for flag in REQUIRED_FLAGS:
        if flag not in flags:
            violations.append(f"  MISSING required flag: {flag}")

    for flag in FORBIDDEN_FLAGS:
        if flag in flags:
            violations.append(f"  FOUND forbidden flag: {flag}")

    defines = _extract_defines(flags)
    for name in FORBIDDEN_DEFINES:
        if name in defines:
            violations.append(f"  FOUND forbidden define: -D{name}")

    return violations


CORE_SOURCE_DIRS = (
    "cpp/codes/",
    "cpp/encoding/",
    "cpp/messages/",
    "cpp/signal/",
    "cpp/platform/",
)


def main() -> int:
    """Run flag checks on all core library translation units."""
    db_path_arg = sys.argv[1] if len(sys.argv) > 1 else "build/compile_commands.json"
    db_path = pathlib.Path(db_path_arg)

    if not db_path.exists():
        print(f"ERROR: compile_commands.json not found at {db_path}")
        print(
            "Run 'task build' or cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON first."
        )
        return 1

    with db_path.open(encoding="utf-8") as fh:
        commands = json.load(fh)

    # Check translation units under any core source directory
    signal_units = [
        entry
        for entry in commands
        if any(d in entry.get("file", "").replace("\\", "/") for d in CORE_SOURCE_DIRS)
    ]

    if not signal_units:
        print("WARNING: No core library units found in compile_commands.json.")
        print("         Ensure the build was configured with the core library.")
        return 1

    all_pass = True
    for entry in signal_units:
        file_name = pathlib.Path(entry.get("file", "unknown")).name
        violations = check_unit(entry)
        if violations:
            all_pass = False
            print(f"FAIL: {file_name}")
            for v in violations:
                print(v)
        else:
            print(f"PASS: {file_name}")

    if all_pass:
        print(f"\nAll {len(signal_units)} core unit(s) passed flag checks.")
        return 0

    print(f"\nFlag check FAILED ({len(signal_units)} unit(s) checked).")

    # Common operator confusion: this gate is for the *flight* compile
    # database; it is intentionally hostile to dev/bench-flag-on builds.
    # If every flight unit reports a forbidden define, the user almost
    # certainly fed the wrong database (e.g.
    # ``build/bench-flags/compile_commands.json``).  Spell that out
    # rather than leaving the operator to compare paths by eye.
    if any(
        "forbidden define" in line
        for entry in signal_units
        for line in check_unit(entry)
    ):
        print(
            "\nHint: this gate is for the *flight* compile database.\n"
            "      If you meant to inspect the bench build, the flags\n"
            "      were enabled deliberately and the gate is doing its\n"
            "      job — point it at build/cpp-tests-ninja/ or\n"
            "      build/compile_commands.json instead of\n"
            "      build/bench-flags/."
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
