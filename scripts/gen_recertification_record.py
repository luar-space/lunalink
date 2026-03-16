#!/usr/bin/env python3
r"""
Recertification Record Generator.

Produces a flight-programme-quality RST assurance record from per-gate
PASS/FAIL/CANCELLED/SKIPPED verdicts.  Has zero project-specific knowledge —
it is a pure document generator.  Copy it to any project that uses the same
recertification workflow pattern.

Usage (CI or local)::

    python3 scripts/gen_recertification_record.py \
        --project   "LunaLink" \
        --doc-id    "LUNALINK-RCERT-001" \
        --ref       "abc1234def5678..." \
        --ref-name  "v1.2.0" \
        --milestone "PDR" \
        --reason    "Pre-PDR baseline certification" \
        --authority "Lead Developer" \
        --timestamp "2026-03-13T14:30:00Z" \
        --gate lint=PASS \
        --gate test-cpp=PASS \
        --gate swifi=PASS \
        --gate-label "lint=Lint / Static Analysis (SWE-108, SWE-117)" \
        --gate-label "test-cpp=C++ Unit Tests (SWE-127)" \
        --out docs/assurance/private/recertification_record.rst

Verdict strings accepted: PASS, FAIL, CANCELLED, SKIPPED.
Overall verdict: PASS only when every gate is PASS;
FAIL when any gate is FAIL;
WARN when no FAIL but at least one CANCELLED or SKIPPED.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Generate an RST recertification record from gate verdicts.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--project", required=True, help="Project name")
    p.add_argument(
        "--doc-id", required=True, help="Document ID (e.g. LUNALINK-RCERT-001)"
    )
    p.add_argument("--ref", required=True, help="Full git SHA of the certified ref")
    p.add_argument(
        "--ref-name", required=True, help="Human-readable ref name (branch / tag)"
    )
    p.add_argument(
        "--milestone",
        required=True,
        help="Lifecycle milestone: SRR / PDR / CDR / SAR / Flight / Hotfix / Periodic",
    )
    p.add_argument("--reason", required=True, help="Reason for this certification run")
    p.add_argument("--authority", default="", help="Authorising party (name / role)")
    p.add_argument("--timestamp", required=True, help="ISO-8601 timestamp (UTC)")
    p.add_argument(
        "--gate",
        action="append",
        default=[],
        metavar="KEY=VERDICT",
        help="Gate verdict (repeatable): e.g. --gate lint=PASS",
    )
    p.add_argument(
        "--gate-label",
        action="append",
        default=[],
        metavar="KEY=LABEL",
        dest="gate_label",
        help="Human label for a gate key (repeatable): "
        'e.g. --gate-label "lint=Lint / Static Analysis (SWE-108)"',
    )
    p.add_argument(
        "--out",
        default="docs/assurance/private/recertification_record.rst",
        help="Output RST file path",
    )
    return p


def _parse_kv_list(items: list[str]) -> dict[str, str]:
    """Parse a list of 'KEY=VALUE' strings into an ordered dict."""
    result: dict[str, str] = {}
    for item in items:
        idx = item.find("=")
        if idx < 1:
            print(
                f"WARNING: ignoring malformed key=value argument: {repr(item)}",
                file=sys.stderr,
            )
            continue
        result[item[:idx]] = item[idx + 1 :]
    return result


# ---------------------------------------------------------------------------
# Verdict logic
# ---------------------------------------------------------------------------

VALID_VERDICTS = {"PASS", "FAIL", "CANCELLED", "SKIPPED"}


def _overall_verdict(verdicts: dict[str, str]) -> str:
    vals = list(verdicts.values())
    if any(v == "FAIL" for v in vals):
        return "FAIL"
    if all(v == "PASS" for v in vals):
        return "PASS"
    return "WARN"


# ---------------------------------------------------------------------------
# RST helpers
# ---------------------------------------------------------------------------


def _rst_escape(text: str) -> str:
    """Escape characters that would break RST field lists or inline markup."""
    return (
        text.replace("\\", "\\\\")
        .replace("`", "\\`")
        .replace("*", "\\*")
        .replace("_", "\\_")
    )


def _heading(title: str, char: str = "-") -> str:
    return f"{title}\n{char * len(title)}\n"


def _field_list(**fields: str) -> str:
    """Render an RST field list block."""
    return "\n".join(f":{k}: {v}" for k, v in fields.items()) + "\n"


def _list_table(
    headers: list[str], rows: list[list[str]], widths: list[int] | None = None
) -> str:
    lines = [".. list-table::", "   :header-rows: 1"]
    if widths:
        lines.append(f"   :widths: {' '.join(str(w) for w in widths)}")
    lines.append("")
    # Header
    lines.append(f"   * - {headers[0]}")
    for h in headers[1:]:
        lines.append(f"     - {h}")
    # Rows
    for row in rows:
        lines.append(f"   * - {row[0]}")
        for cell in row[1:]:
            lines.append(f"     - {cell}")
    return "\n".join(lines) + "\n"


def _extract_spec_ref(label: str) -> tuple[str, str]:
    """
    Split a label into name and spec-ref parts.

    Splits "Display Name (SPEC-REF)" into ("Display Name", "SPEC-REF").
    If no parentheses are present, returns (label, "").
    """
    end = label.rfind(")")
    start = label.rfind("(")
    if start > 0 and end > start:
        name = label[:start].strip()
        spec = label[start + 1 : end].strip()
        return name, spec
    return label.strip(), ""


# ---------------------------------------------------------------------------
# Document generation
# ---------------------------------------------------------------------------


def generate(args: argparse.Namespace) -> str:  # noqa: C901
    """Build the full RST document string from parsed CLI arguments."""
    gates: dict[str, str] = _parse_kv_list(args.gate)
    labels: dict[str, str] = _parse_kv_list(args.gate_label)

    # Validate verdicts
    for key, verdict in gates.items():
        if verdict not in VALID_VERDICTS:
            print(
                f"WARNING: unknown verdict '{verdict}' for gate '{key}';"
                " treating as SKIPPED",
                file=sys.stderr,
            )
            gates[key] = "SKIPPED"

    overall = _overall_verdict(gates)
    n_pass = sum(1 for v in gates.values() if v == "PASS")
    n_total = len(gates)
    n_fail = sum(1 for v in gates.values() if v == "FAIL")

    # Resolve display label for each gate key
    def display_label(key: str) -> str:
        return labels.get(key, key)

    sections: list[str] = []

    # ------------------------------------------------------------------
    # Title
    # ------------------------------------------------------------------
    title = "Recertification Record"
    sections.append(f"{title}\n{'=' * len(title)}\n")

    # ------------------------------------------------------------------
    # Document metadata (RST field list)
    # ------------------------------------------------------------------
    sections.append(
        _field_list(
            **{
                "Document ID": _rst_escape(args.doc_id),
                "Project": _rst_escape(args.project),
                "Certified Ref": f"``{args.ref_name}`` @ ``{args.ref}``",
                "Lifecycle Milestone": _rst_escape(args.milestone),
                "Certification Timestamp": _rst_escape(args.timestamp),
                "Reason": _rst_escape(args.reason),
                "Authorised by": _rst_escape(args.authority)
                if args.authority
                else "*(not specified)*",
                "Overall Verdict": f"**{overall}**",
            }
        )
    )
    sections.append("")

    # ------------------------------------------------------------------
    # Executive Summary
    # ------------------------------------------------------------------
    sections.append(_heading("Executive Summary", "-"))
    summary_line = (
        f"{n_pass} of {n_total} gates passed.  Overall verdict: **{overall}**."
    )
    sections.append(summary_line + "\n")

    if n_fail > 0:
        failed_items = []
        for k, v in gates.items():
            if v != "FAIL":
                continue
            name, _ = _extract_spec_ref(display_label(k))
            entry = f"* **{name}**" if name != k else f"* **{k}**"
            failed_items.append(entry)
        sections.append("**Failed gates:**\n")
        sections.append("\n".join(failed_items) + "\n")

    sections.append("")

    # ------------------------------------------------------------------
    # Gate Verdicts table
    # ------------------------------------------------------------------
    sections.append(_heading("Gate Verdicts", "-"))

    table_rows = []
    for key, verdict in gates.items():
        label_full = display_label(key)
        name, spec_ref = _extract_spec_ref(label_full)
        table_rows.append([name, spec_ref, verdict])

    sections.append(
        _list_table(
            headers=["Gate", "Specification Reference", "Verdict"],
            rows=table_rows,
            widths=[42, 38, 20],
        )
    )
    sections.append("")

    # ------------------------------------------------------------------
    # Acceptance Criteria
    # ------------------------------------------------------------------
    sections.append(_heading("Acceptance Criteria", "-"))
    sections.append(
        "This record constitutes a passing certification when **all** gates report "
        "``PASS``.\n"
        "A ``FAIL`` verdict on any gate means the software does not satisfy the\n"
        "requirements for the stated milestone and must not be used as evidence\n"
        "in a milestone review.\n"
        "\n"
        "``CANCELLED`` or ``SKIPPED`` verdicts indicate a gate was not exercised in\n"
        "this run.  The record is still issued so that the auditor can see precisely\n"
        "which gates were and were not evaluated.  A ``WARN`` overall verdict"
        " requires\n"
        "a documented justification for each non-``PASS`` gate before the record is\n"
        "submitted to a formal review.\n"
    )
    sections.append("")

    # ------------------------------------------------------------------
    # Open Items
    # ------------------------------------------------------------------
    sections.append(_heading("Open Items", "-"))
    open_items = [
        (key, verdict, display_label(key))
        for key, verdict in gates.items()
        if verdict in {"CANCELLED", "SKIPPED", "WARN"}
    ]
    if open_items:
        for key, verdict, lbl in open_items:
            name, _ = _extract_spec_ref(lbl)
            sections.append(f"* **{name}** (``{key}``): {verdict}\n")
    else:
        sections.append("None.\n")
    sections.append("")

    # ------------------------------------------------------------------
    # Signature Block
    # ------------------------------------------------------------------
    sections.append(_heading("Signature Block", "-"))
    authority_name = args.authority if args.authority else "_______"
    sections.append(
        _list_table(
            headers=["Role", "Name", "Signature", "Date"],
            rows=[
                ["Lead Developer", authority_name, "_______", "_______"],
                ["IV&V Engineer *(if applicable)*", "_______", "_______", "_______"],
                [
                    "Programme Manager *(if applicable)*",
                    "_______",
                    "_______",
                    "_______",
                ],
            ],
            widths=[28, 24, 24, 24],
        )
    )
    sections.append("")

    # ------------------------------------------------------------------
    # Document Control
    # ------------------------------------------------------------------
    sections.append(_heading("Document Control", "-"))
    sections.append(
        ".. list-table::\n"
        "   :header-rows: 0\n"
        "   :widths: 30 70\n"
        "\n"
        "   * - Generated by\n"
        "     - ``scripts/gen_recertification_record.py``\n"
        "   * - CI workflow\n"
        "     - ``.github/workflows/recertify.yml``\n"
        "   * - Local task\n"
        "     - ``task recertify``\n"
        "   * - Reuse\n"
        "     - This script has no project-specific knowledge.\n"
        "       Copy ``gen_recertification_record.py`` and\n"
        "       ``recertify_local.sh`` to any project using the same\n"
        "       gate pattern.\n"
    )

    return "\n".join(sections)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    """Parse CLI arguments, generate the RST record, and write it to disk."""
    parser = _build_parser()
    args = parser.parse_args()

    doc = generate(args)

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(doc, encoding="utf-8")

    overall = _overall_verdict(_parse_kv_list(args.gate))
    print(f"Recertification record written: {out_path}  [overall: {overall}]")


if __name__ == "__main__":
    main()
