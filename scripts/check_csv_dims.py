"""Check max dimensions of LDPC CSV index files."""

import csv
from pathlib import Path


def check_csv(filename):  # noqa: D103
    p = Path("docs/references") / filename
    if not p.exists():
        print(f"Skip {filename}")
        return

    max_c0 = 0
    max_c1 = 0
    with open(p) as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            v0, v1 = map(int, row)
            max_c0 = max(max_c0, v0)
            max_c1 = max(max_c1, v1)

    print(f"File: {filename}")
    print(f"  Max Col 0: {max_c0}")
    print(f"  Max Col 1: {max_c1}")


check_csv("003a_lunanet_sf2_ldpc_submatrix_a_ind.csv")
check_csv("003d_lunanet_sf2_ldpc_submatrix_c_ind.csv")
check_csv("003e_lunanet_sf2_ldpc_submatrix_d_ind.csv")
