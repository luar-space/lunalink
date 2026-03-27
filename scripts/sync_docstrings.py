#!/usr/bin/env python3
"""Check that C++ header @param tags and binding docstring Parameters stay in sync.

Parses:
  1. C++ headers in cpp/include/lunalink/ — extracts @param names per function.
  2. cpp/bindings/afs_module.cpp — extracts parameter names from NumPy-style
     docstring ``Parameters`` sections inside R"(...)" strings.

Reports:
  - Binding functions with no docstring at all.
  - @param names in the C++ header that are missing from the binding docstring.
  - Docstring parameter names that don't appear in the C++ header.

Exit code 0 if everything is in sync, 1 if drift is detected.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# ── Parsing helpers ──────────────────────────────────────────────────────────

# Map from Python binding name -> C++ header function name when they differ.
_PY_TO_CPP = {
    "bch_encode": "bch_encode",
    "bch_decode": "bch_decode",
    "bch_decode_soft": "bch_decode_soft",
    "bch_codebook_checksum": "bch_codebook_checksum",
    "crc24_calculate": "crc24_calculate",
    "crc24_verify": "crc24_verify",
    "block_interleave": "block_interleave",
    "block_deinterleave": "block_deinterleave",
    "block_deinterleave_soft": "block_deinterleave_soft",
    "ldpc_encode": "ldpc_encode",
    "ldpc_decode": "ldpc_decode",
    "frame_build": "frame_build",
    "frame_build_partial": "frame_build_partial",
    "commit_frame": "commit_frame",
    "subframe_build_sb2": "subframe_build_sb2",
    "subframe_build_sb3": "subframe_build_sb3",
    "subframe_build_sb4": "subframe_build_sb4",
    "prn_code": "gold_prn_packed",
    "modulate_i": "modulate_bpsk_i",
    "modulate_q": "modulate_bpsk_q",
    "multiplex_iq": "multiplex_iq",
    "sync_detect": "detect",
    "frame_decode": "frame_decode",
    "compute_tot": "compute_tot",
    "compute_sise": "compute_sise",
    "compute_sise_percentile": "compute_sise_percentile",
    "parse_sb2": "parse_sb2",
    "parse_sb3": "parse_sb3",
    "parse_sb4": "parse_sb4",
    "legendre_cache_init": "legendre_cache_init",
}

# Parameters that exist only in the Python binding (C++ uses output spans, etc.)
_BINDING_ONLY_PARAMS: dict[str, set[str]] = {
    "crc24_calculate": {"data"},
    "crc24_verify": {"data_with_crc"},
    "commit_frame": {"frame"},
    "ldpc_decode": {"workspace"},
    "sync_detect": {"threshold"},
}

# Parameters that exist only in C++ (output buffers, workspaces)
_CPP_ONLY_PARAMS: dict[str, set[str]] = {
    "bch_encode": {"out"},
    "crc24_calculate": {"out_crc", "in"},
    "ldpc_encode": {"out", "workspace"},
    "ldpc_decode": {"out", "workspace"},
    "frame_build": {"out", "ldpc_workspace", "frame_workspace"},
    "frame_build_partial": {"out", "workspace"},
    "commit_frame": {"frame_buf", "frame"},
    "subframe_build_sb2": {"out", "workspace"},
    "subframe_build_sb3": {"out", "workspace"},
    "subframe_build_sb4": {"out", "workspace"},
    "block_interleave": {"in", "out"},
    "block_deinterleave": {"in", "out"},
    "block_deinterleave_soft": {"in", "out"},
    "modulate_bpsk_i": {"out"},
    "modulate_bpsk_q": {"out"},
    "multiplex_iq": {"out"},
    "frame_decode": {"result", "workspace", "hal"},
    "detect": {"result"},
    "compute_sise": set(),
    "compute_sise_percentile": set(),
    "compute_tot": set(),
    "parse_sb2": {"result"},
    "parse_sb3": {"result"},
    "parse_sb4": {"result"},
    "gold_prn_packed": {"out"},
}

# Functions where C++ and Python param names intentionally differ
_PARAM_RENAMES: dict[str, dict[str, str]] = {
    "bch_decode": {"in": "codeword"},
    "bch_decode_soft": {"llr": "llr"},
    "prn_code": {"prn_id": "prn_id"},
    "modulate_i": {"prn": "prn", "data": "data"},
    "modulate_q": {"chips": "chips"},
    "multiplex_iq": {"i_samples": "i", "q_samples": "q"},
    "parse_sb2": {"sb2_data": "sb2_data"},
    "parse_sb3": {"sb3_data": "sb3_data"},
    "parse_sb4": {"sb4_data": "sb4_data"},
    "frame_decode": {"rx_symbols": "rx_symbols"},
    "sync_detect": {"rx_symbols": "rx_symbols"},
    "frame_build": {"fid": "fid", "toi": "toi"},
    "compute_sise": {
        "pos_true": "pos_true",
        "pos_broadcast": "pos_broadcast",
        "vel_true": "vel_true",
        "vel_broadcast": "vel_broadcast",
    },
    "compute_sise_percentile": {
        "pos_errors_m": "pos_errors_m",
        "vel_errors_mps": "vel_errors_mps",
    },
}


def extract_cpp_params(headers_dir: Path) -> dict[str, set[str]]:
    """Extract @param names from C++ headers, keyed by function name."""
    result: dict[str, set[str]] = {}
    param_re = re.compile(r"@param\[?\w*\]?\s+(\w+)")

    for hpp in sorted(headers_dir.rglob("*.hpp")):
        text = hpp.read_text()
        # Find @param blocks followed by a function signature
        lines = text.split("\n")
        current_params: set[str] = set()
        for line in lines:
            m = param_re.search(line)
            if m:
                current_params.add(m.group(1))
            # Function declaration line (heuristic: contains '(' and ends with
            # ';', ')' or '{' and has a return type)
            elif current_params and re.search(
                r"\b(\w+)\s*\(", line
            ):
                # Extract function name
                fn_match = re.search(r"\b(\w+)\s*\(", line)
                if fn_match:
                    fn_name = fn_match.group(1)
                    if fn_name not in (
                        "if", "for", "while", "switch", "return", "throw",
                        "static_cast", "const_cast", "reinterpret_cast",
                    ):
                        if fn_name in result:
                            result[fn_name] |= current_params
                        else:
                            result[fn_name] = set(current_params)
                        current_params = set()
            elif not line.strip().startswith("*") and not line.strip().startswith("//"):
                if current_params and line.strip() and not line.strip().startswith("/**"):
                    current_params = set()

    return result


def extract_binding_docstring_params(binding_file: Path) -> dict[str, set[str]]:
    """Extract parameter names from NumPy-style docstrings in binding .def() calls."""
    text = binding_file.read_text()
    result: dict[str, set[str]] = {}

    # Find m.def("name", ..., R"(...)") patterns
    # First, find all m.def("name" occurrences
    def_re = re.compile(r'm\.def\(\s*"(\w+)"')
    # Find R"(...)" raw docstrings
    rawdoc_re = re.compile(r'R"\((.*?)\)"', re.DOTALL)

    # Strategy: for each m.def, find the nearest R"(...)" docstring
    for def_match in def_re.finditer(text):
        fn_name = def_match.group(1)
        # Search forward from this def for a R"(" docstring before the next m.def
        start = def_match.end()
        next_def = def_re.search(text, start)
        end = next_def.start() if next_def else len(text)
        segment = text[start:end]

        doc_match = rawdoc_re.search(segment)
        if not doc_match:
            continue

        docstring = doc_match.group(1)
        # Parse NumPy-style Parameters section
        params: set[str] = set()
        in_params = False
        for line in docstring.split("\n"):
            stripped = line.strip()
            if stripped == "Parameters":
                in_params = True
                continue
            if in_params and stripped.startswith("---"):
                continue
            if in_params:
                # A parameter line looks like "name : type" (not indented further)
                if stripped and not stripped.startswith(" ") and " : " in stripped:
                    param_name = stripped.split(" : ")[0].strip()
                    # Handle "a, b : type" style
                    for p in param_name.split(","):
                        params.add(p.strip())
                elif stripped in ("Returns", "Raises", "Examples", "Notes", "See Also"):
                    in_params = False

        if params:
            result[fn_name] = params

    return result


def extract_binding_undocumented(binding_file: Path) -> list[str]:
    """Find m.def() calls that have no docstring (raw or inline)."""
    text = binding_file.read_text()
    def_re = re.compile(r'm\.def\(\s*"(\w+)"')
    # Match either R"(...)" raw docstrings or trailing inline "..." docstrings
    rawdoc_re = re.compile(r'R"\(')
    # Inline docstrings: the last argument to .def() is a string literal
    inline_doc_re = re.compile(r',\s*"[^"]{10,}"\s*\)')
    undocumented = []

    for def_match in def_re.finditer(text):
        fn_name = def_match.group(1)
        start = def_match.end()
        next_def = def_re.search(text, start)
        end = next_def.start() if next_def else len(text)
        segment = text[start:end]

        if not rawdoc_re.search(segment) and not inline_doc_re.search(segment):
            undocumented.append(fn_name)

    return undocumented


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    headers_dir = repo / "cpp" / "include" / "lunalink"
    binding_file = repo / "cpp" / "bindings" / "afs_module.cpp"

    if not headers_dir.exists():
        print(f"ERROR: {headers_dir} not found", file=sys.stderr)
        return 1
    if not binding_file.exists():
        print(f"ERROR: {binding_file} not found", file=sys.stderr)
        return 1

    cpp_params = extract_cpp_params(headers_dir)
    binding_params = extract_binding_docstring_params(binding_file)
    undocumented = extract_binding_undocumented(binding_file)

    errors: list[str] = []

    # Check for undocumented bindings
    for fn in undocumented:
        # Skip internal/generated names
        if fn.startswith("_"):
            continue
        errors.append(f"  MISSING DOCSTRING: m.def(\"{fn}\") has no R\"(...)\" docstring")

    # Check param sync for documented bindings
    for py_name, binding_ps in sorted(binding_params.items()):
        cpp_name = _PY_TO_CPP.get(py_name)
        if cpp_name is None:
            continue  # Not mapped — skip

        cpp_ps = cpp_params.get(cpp_name, set())
        if not cpp_ps:
            continue  # No C++ params found — likely a simple function

        # Remove known binding-only and C++-only params
        binding_only = _BINDING_ONLY_PARAMS.get(py_name, set())
        cpp_only = _CPP_ONLY_PARAMS.get(cpp_name, set())
        renames = _PARAM_RENAMES.get(py_name, {})

        effective_cpp = cpp_ps - cpp_only
        effective_binding = binding_ps - binding_only

        # Apply renames: map C++ names to Python names
        renamed_cpp = set()
        for p in effective_cpp:
            if p in renames:
                renamed_cpp.add(renames[p])
            else:
                renamed_cpp.add(p)

        missing_in_binding = renamed_cpp - effective_binding
        extra_in_binding = effective_binding - renamed_cpp

        for p in sorted(missing_in_binding):
            errors.append(
                f"  {py_name}: C++ @param '{p}' missing from binding docstring"
            )
        for p in sorted(extra_in_binding):
            errors.append(
                f"  {py_name}: binding docstring param '{p}' not in C++ header"
            )

    if errors:
        print(f"sync-docstrings: {len(errors)} issue(s) found:\n")
        for e in errors:
            print(e)
        return 1

    print(f"sync-docstrings: OK ({len(binding_params)} documented bindings checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
