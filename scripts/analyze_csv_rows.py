"""Analyze row distributions in LDPC CSV index files."""

import csv
from collections import Counter
from pathlib import Path


def analyze_rows(filename):  # noqa: D103
    p = Path("docs/references") / filename
    rows = []
    with open(p) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            rows.append(int(row[0]))  # Assuming Row is Col 0 based on 'ind' analysis

    c = Counter(rows)
    print(f"File: {filename}")
    print(f"Total entries: {len(rows)}")
    print(f"Min Row: {min(rows)}")
    print(f"Max Row: {max(rows)}")
    print(f"Unique Rows: {len(c)}")
    print(f"Most common rows: {c.most_common(5)}")

    # Check if rows are within [0, 719]
    in_range = sum(1 for r in rows if 0 <= r < 720)
    print(f"Entries with Row < 720: {in_range}")


analyze_rows("003a_lunanet_sf2_ldpc_submatrix_a_ind.csv")
analyze_rows("003d_lunanet_sf2_ldpc_submatrix_c_ind.csv")
analyze_rows("003e_lunanet_sf2_ldpc_submatrix_d_ind.csv")
