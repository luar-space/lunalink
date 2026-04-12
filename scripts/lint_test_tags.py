#!/usr/bin/env python3
"""Fail if any Catch2 TEST_CASE/SCENARIO in cpp/tests/ lacks a tag string.

The routing contract is: test-cpp runs `~[slow]`, acceptance runs everything.
Untagged tests slip silently into the fast gate, so this lint forces every
test case to declare at least one tag. It does not validate *which* tag.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = ROOT / "cpp" / "tests"

# Matches TEST_CASE("name", "[tag1][tag2]") or SCENARIO(...) / TEMPLATE_TEST_CASE(...)
# Captures the full argument list so we can inspect the tag string.
MACRO_RE = re.compile(
    r"\b(TEST_CASE|SCENARIO|TEMPLATE_TEST_CASE|TEMPLATE_TEST_CASE_SIG)\s*\(",
)
TAG_RE = re.compile(r"\[[^\]\s]+\]")


def split_top_level_args(args: str) -> list[str]:
    """Split a function argument list on top-level commas (ignoring strings)."""
    parts: list[str] = []
    depth = 0
    i = 0
    start = 0
    while i < len(args):
        c = args[i]
        if c == '"':
            i += 1
            while i < len(args) and args[i] != '"':
                if args[i] == "\\":
                    i += 1
                i += 1
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(args[start:i])
            start = i + 1
        i += 1
    parts.append(args[start:])
    return parts


def join_adjacent_strings(arg: str) -> str:
    """Return the concatenation of all C++ string literals in arg."""
    pieces = re.findall(r'"((?:[^"\\]|\\.)*)"', arg)
    return "".join(pieces)


def find_macro_calls(text: str):
    """Yield (macro, line_no, args) for each TEST_CASE-like macro call."""
    for m in MACRO_RE.finditer(text):
        start = m.end()
        depth = 1
        i = start
        while i < len(text) and depth > 0:
            c = text[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c == '"':
                i += 1
                while i < len(text) and text[i] != '"':
                    if text[i] == "\\":
                        i += 1
                    i += 1
            i += 1
        args = text[start : i - 1]
        line_no = text.count("\n", 0, m.start()) + 1
        yield m.group(1), line_no, args


def main() -> int:
    """Entry point: walk test sources and fail on any untagged test case."""
    failures: list[str] = []
    for path in sorted(TESTS_DIR.rglob("*.cpp")):
        text = path.read_text()
        for macro, line, args in find_macro_calls(text):
            top_args = split_top_level_args(args)
            # TEMPLATE_TEST_CASE_SIG has extra sig args after tags, but name
            # and tags are still the first two.
            if len(top_args) < 2:
                failures.append(
                    f"{path.relative_to(ROOT)}:{line}: {macro} has no tag argument"
                )
                continue
            name = join_adjacent_strings(top_args[0])
            tags = join_adjacent_strings(top_args[1])
            if not TAG_RE.search(tags):
                failures.append(
                    f'{path.relative_to(ROOT)}:{line}: {macro}("{name}", ...) '
                    f"has empty tag string"
                )

    if failures:
        print("Untagged Catch2 tests found:", file=sys.stderr)
        for f in failures:
            print(f"  {f}", file=sys.stderr)
        print(
            f"\n{len(failures)} test(s) missing tags. "
            "Every TEST_CASE must have at least one [tag].",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
