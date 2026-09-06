#!/usr/bin/env python3
"""Probe: injection interpolation on the nonsymmetric advection_2d operator.

TestAIR.test_injection_interpolation checks the exact pattern on a 5-point
1-D case. This probe builds the same operator (a single unit weight per
C-point row) on advection_2d's larger, RS-split, nonsymmetric graph.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import advection_2d
from pyamg.strength import classical_strength_of_connection
from pyamg.classical import split
from pyamg.classical.interpolate import injection_interpolation


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
    ap.add_argument("--size", required=True, type=int)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    A, _ = advection_2d((a.size, a.size))
    A = perturb(A.tocsr(), int(cfg["matrix_perturb_ulps"]))
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S).astype("intc")
    P = injection_interpolation(A, splitting)
    np.save(a.out, P.toarray().ravel(), allow_pickle=False)


if __name__ == "__main__":
    main()
