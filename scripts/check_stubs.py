#!/usr/bin/env python3
"""Verify that _afs.pyi type stubs and Python re-exports match the C++ module.

Two checks:
1. Every public symbol in the compiled _afs module has a stub in _afs.pyi.
2. Every public _afs symbol is re-exported via lunalink.afs (signal.py or __init__.py).

Run as part of CI to catch drift after adding new bindings.

Usage:
    python scripts/check_stubs.py
"""

from __future__ import annotations

import ast
import sys
from pathlib import Path

STUB_PATH = Path("src/lunalink/afs/_afs.pyi")

# Symbols that are internal or inherited and don't need stubs/re-exports.
IGNORE = {"__doc__", "__loader__", "__name__", "__package__", "__spec__", "__file__"}


def get_stub_symbols(path: Path) -> set[str]:
    """Parse the .pyi file and extract all top-level public names."""
    tree = ast.parse(path.read_text())
    names: set[str] = set()
    for node in ast.iter_child_nodes(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            names.add(node.name)
        elif isinstance(node, ast.ClassDef):
            names.add(node.name)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names.add(node.target.id)
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    names.add(target.id)
    return names


def get_runtime_symbols() -> set[str]:
    """Import the compiled module and list its public attributes.

    Excludes enum member names exported to module scope via .export_values()
    — these are covered by the enum class definition in the stub.
    """
    import enum

    import lunalink.afs._afs as mod

    # Collect all enum member names so we can exclude them.
    enum_member_names: set[str] = set()
    for name in dir(mod):
        obj = getattr(mod, name)
        if isinstance(obj, type) and issubclass(obj, enum.Enum):
            enum_member_names.update(m.name for m in obj)

    return (
        {name for name in dir(mod) if not name.startswith("_")}
        - IGNORE
        - enum_member_names
    )


def get_reexported_symbols() -> set[str]:
    """Get the set of _afs symbols accessible via lunalink.afs.signal."""
    import lunalink.afs.signal as sig

    return {name for name in dir(sig) if not name.startswith("_")}


def main() -> int:  # noqa: D103
    if not STUB_PATH.exists():
        print(f"ERROR: stub file not found: {STUB_PATH}")
        return 1

    stub_names = get_stub_symbols(STUB_PATH)
    runtime_names = get_runtime_symbols()
    reexported = get_reexported_symbols()

    ok = True

    # Check 1: stubs cover runtime
    missing_from_stub = runtime_names - stub_names
    extra_in_stub = stub_names - runtime_names

    if missing_from_stub:
        print(f"FAIL: {len(missing_from_stub)} runtime symbols missing from _afs.pyi:")
        for name in sorted(missing_from_stub):
            print(f"  + {name}")
        ok = False

    if extra_in_stub:
        print(f"WARN: {len(extra_in_stub)} stub symbols not in runtime module:")
        for name in sorted(extra_in_stub):
            print(f"  - {name}")

    # Check 2: signal.py re-exports everything from _afs
    missing_reexport = runtime_names - reexported
    if missing_reexport:
        print(
            f"FAIL: {len(missing_reexport)} _afs symbols not re-exported via signal.py:"
        )
        for name in sorted(missing_reexport):
            print(f"  + {name}")
        ok = False

    if ok:
        print(
            f"OK: {len(runtime_names)} runtime symbols — "
            f"stubs: {len(stub_names)}, re-exports: {len(reexported & runtime_names)}, "
            f"all matched."
        )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
