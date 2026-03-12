"""Check if CSV row indices are sequential."""

import csv
from pathlib import Path


def check_sequential(filename):  # noqa: D103
    p = Path("docs/references") / filename
    if not p.exists():
        return

    last_v0 = -1
    is_seq = True
    with open(p) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            v0 = int(row[0])
            if v0 != last_v0 + 1:
                is_seq = False
                # break
            last_v0 = v0

    print(f"File: {filename} Col 0 Sequential? {is_seq}. Max: {last_v0}")


check_sequential("003d_lunanet_sf2_ldpc_submatrix_c_ind.csv")
