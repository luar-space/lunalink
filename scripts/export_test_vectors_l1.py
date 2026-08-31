# Copyright 2025-2026 Marco Fiscato
# SPDX-License-Identifier: Apache-2.0
"""Generate Level 1 interoperability test vectors for all 210 PRNs.

Writes one ``codes_prn{NNN}.hex`` file per PRN into the target directory,
in the standardized format defined in :mod:`lunalink.afs.vectors`.

Intended to target the public ``lsis-afs-test-vectors`` repository so that
regenerating the distribution is a one-shot command.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import UTC, datetime
from pathlib import Path

from lunalink.afs import PRN_COUNT
from lunalink.afs.vectors import export_code_vectors


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    """Emit the Level 1 spreading-code vectors for the public repo."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "output_dir",
        type=Path,
        help="Target directory (typically the codes/ subdir of lsis-afs-test-vectors)",
    )
    ap.add_argument(
        "--manifest",
        type=Path,
        default=None,
        help="Also write a manifest.json next to the output dir with "
        "SHA256 of every file",
    )
    args = ap.parse_args(argv)

    out_dir: Path = args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    entries: dict[str, str] = {}
    for prn in range(1, PRN_COUNT + 1):
        path_str = export_code_vectors(prn, out_dir)
        path = Path(path_str)
        entries[path.name] = _sha256(path)
        if prn % 20 == 0 or prn == PRN_COUNT:
            print(f"  generated {prn}/{PRN_COUNT}", file=sys.stderr)

    if args.manifest is not None:
        manifest = {
            "version": "1.0",
            "level": 1,
            "generated": datetime.now(UTC).isoformat(),
            "implementation": "LunaLink",
            "files": dict(sorted(entries.items())),
        }
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"  wrote manifest: {args.manifest}", file=sys.stderr)

    print(f"OK: {len(entries)} vectors in {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
