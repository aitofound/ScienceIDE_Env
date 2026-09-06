#!/usr/bin/env python3
"""Probe: classical (non-modified) interpolation weights on the shipped airfoil matrix.

bar is skipped, matching TestRugeStubenFunctions.test_classical_interpolation's own
"classical does not work" skip for that gallery matrix.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.strength import classical_strength_of_connection
from pyamg.classical import split
from pyamg.classical.interpolate import classical_interpolation


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    v = A2.data[0]
    A2.data[0] = v + ulps * np.spacing(v)
    return A2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    A = load_example("airfoil")["A"].tocsr()
    A = perturb(A, int(cfg["matrix_perturb_ulps"]))
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S, second_pass=True)
    P = classical_interpolation(A, S, splitting, modified=False)
    np.save(a.out, P.toarray().ravel(), allow_pickle=False)


if __name__ == "__main__":
    main()
