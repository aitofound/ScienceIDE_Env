#!/usr/bin/env python3
"""Probe: one-point (single strongest connection) interpolation on recirc_flow.

TestAIR.test_one_point_interpolation checks the exact pattern on a 5-point
1-D case, where the strongest connection is trivially the identity stencil
neighbor. recirc_flow's irregular, nonsymmetric connectivity makes the
selected C-point vary row to row, exercising the same argmax/indexing logic
on non-trivial structure.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.strength import classical_strength_of_connection
from pyamg.classical import split
from pyamg.classical.interpolate import one_point_interpolation


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
    A = load_example("recirc_flow")["A"].tocsr()
    A = perturb(A, int(cfg["matrix_perturb_ulps"]))
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S).astype("intc")
    P = one_point_interpolation(A, A, splitting)
    np.save(a.out, P.toarray().ravel(), allow_pickle=False)


if __name__ == "__main__":
    main()
