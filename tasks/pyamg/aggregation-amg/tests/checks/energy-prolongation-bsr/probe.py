#!/usr/bin/env python3
"""Probe for energy-prolongation-bsr: energy_prolongation_smoother on a BSR linear-elasticity operator, reaching the same block-sparse incomplete-matrix-multiply kernel (pyamg/amg_core/smoothed_aggregation.h:970) test_incomplete_mat_mult_bsr exercises directly."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import linear_elasticity
from pyamg.strength import symmetric_strength_of_connection
from pyamg.aggregation.aggregate import standard_aggregation
from pyamg.aggregation.tentative import fit_candidates
from pyamg.aggregation.smooth import energy_prolongation_smoother

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    seed = int(cfg["seed"])
    scale = float(cfg["variant_scale"])

    A, B0 = linear_elasticity((20, 20), format="bsr")
    B0 = B0 * scale
    C = symmetric_strength_of_connection(A)
    AggOp, Cpts = standard_aggregation(C)
    T, Bc = fit_candidates(AggOp, B0)
    P = energy_prolongation_smoother(A, T, C, Bc, B0, (False, {}), krylov="cg", degree=1)
    np.save(a.out, np.asarray(P.data, dtype=np.float64).ravel(), allow_pickle=False)

if __name__ == "__main__":
    main()
