#!/usr/bin/env python3
# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate the PICS proforma from the LSIS V1.0 shall-statement inventory.

A PICS (Protocol Implementation Conformance Statement) proforma states, per
protocol feature, whether the implementation supports it.  LunaLink already
dispositions every normative identifier in ``docs/signal/shall_inventory.rst``;
this script turns that inventory into the conformance view a reviewer expects,
rather than maintaining the same facts twice.

Status and support are derived, not asserted:

* Every LSIS V1.0 "shall" is **M** (mandatory) by definition, except the two
  the spec marks as LNSP implementation flexibility, which are **O**.
* Support follows the disposition: *Implemented* and *TBC in Spec* are
  supported (the latter against a documented assumption); *RF / Out of Scope*
  and *TBW in Spec* are Not Applicable, for reasons the spec or the
  competition scope imposes rather than choices LunaLink made.

The script fails if its row count or disposition histogram disagrees with the
inventory's own summary table, so the proforma cannot silently drift from it.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INVENTORY = REPO_ROOT / "docs" / "signal" / "shall_inventory.rst"

# Identifiers come in three shapes: LSIS-nnn, the frame-qualified
# LSIS-FIDn-nnn used across the navigation-message tables, and Appendix X.
ROW_ID = re.compile(r"(?:LSIS-(?:FID\d+-)?\d+|Appendix [A-Z])$")

# disposition -> (status, support, rationale shown in the Notes column)
DISPOSITION_MAP = {
    "Implemented": ("M", "Yes", ""),
    "TBC in Spec": ("M", "Yes", "spec value To Be Confirmed; documented assumption"),
    "RF / Out of Scope": (
        "M",
        "N/A",
        "requires RF hardware; excluded by competition scope",
    ),
    "TBW in Spec": ("M", "N/A", "spec content To Be Written; nothing to conform to"),
    "FLEX": (
        "O",
        "Yes",
        "LNSP implementation flexibility; choice documented in the SIS ICD",
    ),
}


def parse_inventory(text: str) -> list[dict]:
    """Extract (section, id, clause, feature, disposition, evidence) rows."""
    lines = text.splitlines()
    section: str | None = None
    rows: list[dict] = []
    cells: list[str] | None = None

    def flush() -> None:
        nonlocal cells
        if cells and ROW_ID.fullmatch(cells[0]) and len(cells) >= 4:
            rows.append(
                {
                    "section": section or "",
                    "item": cells[0],
                    "clause": cells[1],
                    "feature": cells[2],
                    "disposition": cells[3],
                    "evidence": cells[4] if len(cells) > 4 else "",
                }
            )
        cells = None

    for i, line in enumerate(lines):
        nxt = lines[i + 1] if i + 1 < len(lines) else ""
        if line.strip() and re.fullmatch(r"-{3,}", nxt.strip()):
            flush()
            section = line.strip()
            continue
        start = re.match(r"^   \* - (.*)$", line)
        if start:
            flush()
            cells = [start.group(1).strip()]
            continue
        cell = re.match(r"^     - (.*)$", line)
        if cell is not None and cells is not None:
            cells.append(cell.group(1).strip())
            continue
        # Continuation of the current cell: indented past the bullet column.
        if cells and line.startswith("       ") and line.strip():
            cells[-1] = f"{cells[-1]} {line.strip()}"
            continue
        if cells and not line.strip():
            continue
        flush()
    flush()
    return rows


def parse_summary(text: str) -> dict[str, int]:
    """Read the inventory's own summary table, for cross-checking."""
    counts: dict[str, int] = {}
    block = text.split("Summary\n-------", 1)
    if len(block) < 2:
        return counts
    for label, value in re.findall(r"   \* - ([^\n]+)\n     - (\d+)\n", block[1]):
        key = label.strip().strip("*").strip()
        key = key.replace(" (implemented with assumption)", "")
        counts[key] = int(value)
    return counts


def check(rows: list[dict], summary: dict[str, int]) -> list[str]:
    """Return the disagreements between the parsed rows and the summary."""
    problems: list[str] = []
    seen = Counter(r["disposition"] for r in rows)
    for disposition, expected in summary.items():
        if disposition == "Total":
            if len(rows) != expected:
                problems.append(f"total: inventory says {expected}, parsed {len(rows)}")
            continue
        if seen.get(disposition, 0) != expected:
            problems.append(
                f"{disposition}: inventory says {expected}, "
                f"parsed {seen.get(disposition, 0)}"
            )
    unknown = sorted({r["disposition"] for r in rows} - set(DISPOSITION_MAP))
    problems += [f"unmapped disposition: {d}" for d in unknown]
    return problems


def support_count(rows: list[dict], support: str) -> int:
    """Count rows whose derived support value matches."""
    return sum(1 for r in rows if DISPOSITION_MAP[r["disposition"]][1] == support)


def render(rows: list[dict]) -> str:
    """Render the proforma as reStructuredText."""
    supported = support_count(rows, "Yes")
    not_applicable = len(rows) - supported
    optional = sum(1 for r in rows if DISPOSITION_MAP[r["disposition"]][0] == "O")

    out: list[str] = []
    add = out.append
    add("..")
    add("   Generated by scripts/pics_proforma.py from shall_inventory.rst.")
    add("   Do not edit by hand: edit the inventory and regenerate.")
    add("")
    add("PICS Proforma")
    add("=============")
    add("")
    add(":Document ID: LUNALINK-PICS-001")
    add(":Spec Reference: LSIS V1.0, 29 January 2025")
    add(":Generated by: ``scripts/pics_proforma.py``")
    add("")
    add("Protocol Implementation Conformance Statement for the LunaLink LSIS-AFS")
    add("reference implementation: which features of the specification this")
    add("implementation supports.  It is generated from")
    add(":doc:`shall_inventory`, so the two cannot disagree — the generator fails")
    add("if its counts differ from the inventory's summary.")
    add("")
    add("Notation")
    add("--------")
    add("")
    add(".. list-table::")
    add("   :widths: 12 88")
    add("   :header-rows: 1")
    add("")
    add("   * - Symbol")
    add("     - Meaning")
    add("   * - **M**")
    add("     - Mandatory.  Every LSIS V1.0 *shall* is mandatory by definition.")
    add("   * - **O**")
    add("     - Optional.  The specification marks the item as LNSP")
    add("       implementation flexibility.")
    add("   * - **Yes**")
    add("     - Supported: implemented, with the evidence named in the row.")
    add("   * - **N/A**")
    add("     - Not applicable.  Either the specification leaves the content To")
    add("       Be Written, so there is nothing to conform to, or the item needs")
    add("       RF hardware, which the competition scope excludes.  Neither is a")
    add("       choice made by this implementation.")
    add("")
    add("Summary")
    add("-------")
    add("")
    add(".. list-table::")
    add("   :widths: 60 20")
    add("   :header-rows: 1")
    add("")
    add("   * - Category")
    add("     - Count")
    add("   * - Items in the proforma")
    add(f"     - {len(rows)}")
    add("   * - Supported (**Yes**)")
    add(f"     - {supported}")
    add("   * - Not applicable (**N/A**)")
    add(f"     - {not_applicable}")
    add("   * - Of which optional (**O**) in the specification")
    add(f"     - {optional}")
    add("")
    add("No item within the competition's scope is unsupported: every **N/A** is")
    add("either RF hardware or specification content marked To Be Written.")
    add("")

    for section in dict.fromkeys(r["section"] for r in rows):
        section_rows = [r for r in rows if r["section"] == section]
        title = section.replace("Section ", "").strip() or "Other"
        add("----")
        add("")
        add(title)
        add("-" * max(len(title), 3))
        add("")
        add(".. list-table::")
        add("   :widths: 12 8 34 8 10 28")
        add("   :header-rows: 1")
        add("")
        add("   * - Item")
        add("     - Clause")
        add("     - Feature")
        add("     - Status")
        add("     - Support")
        add("     - Notes / evidence")
        for row in section_rows:
            status, support, rationale = DISPOSITION_MAP[row["disposition"]]
            note = rationale
            if support == "Yes":
                note = (
                    f"{rationale}; {row['evidence']}" if rationale else row["evidence"]
                )
            elif row["evidence"]:
                note = f"{rationale} ({row['evidence']})"
            add(f"   * - {row['item']}")
            add(f"     - {row['clause']}")
            add(f"     - {row['feature']}")
            add(f"     - {status}")
            add(f"     - {support}")
            add(f"     - {note}")
        add("")

    add(".. |le| unicode:: U+2264")
    add(".. |rarr| unicode:: U+2192")
    add(".. |times| unicode:: U+00D7")
    add("")
    return "\n".join(out)


def main() -> int:
    """Generate the proforma, or report why the inventory disagrees with it."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-rst",
        type=Path,
        default=REPO_ROOT / "docs" / "signal" / "pics_proforma.rst",
    )
    parser.add_argument(
        "--output-json",
        type=Path,
        default=REPO_ROOT / "docs" / "assurance" / "evidence" / "pics_proforma.json",
    )
    args = parser.parse_args()

    text = INVENTORY.read_text(encoding="utf-8")
    rows = parse_inventory(text)
    problems = check(rows, parse_summary(text))
    if problems:
        print("pics-proforma: inventory disagreement", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    args.output_rst.write_text(render(rows), encoding="utf-8")
    payload = {
        "title": "LSIS-AFS PICS Proforma",
        "document_id": "LUNALINK-PICS-001",
        "generated_by": "scripts/pics_proforma.py",
        "source": "docs/signal/shall_inventory.rst",
        "items": [
            {
                **row,
                "status": DISPOSITION_MAP[row["disposition"]][0],
                "support": DISPOSITION_MAP[row["disposition"]][1],
            }
            for row in rows
        ],
        "summary": {
            "items": len(rows),
            "supported": sum(
                1 for r in rows if DISPOSITION_MAP[r["disposition"]][1] == "Yes"
            ),
            "not_applicable": sum(
                1 for r in rows if DISPOSITION_MAP[r["disposition"]][1] == "N/A"
            ),
            "optional": sum(
                1 for r in rows if DISPOSITION_MAP[r["disposition"]][0] == "O"
            ),
        },
    }
    args.output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        f"pics-proforma: {len(rows)} items -> {args.output_rst.relative_to(REPO_ROOT)}"
    )
    print(f"                        -> {args.output_json.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
