#!/usr/bin/env python3
"""Probe for energy-prolongation: energy_prolongation_smoother called directly (pyamg/aggregation/smooth.py:875) -- the exact function test_range/test_postfilter/test_prefilter exercise, not a full solve -- on the shipped unit_square mesh; test_incomplete_mat_mult_bsr is its own check (energy-prolongation-bsr)."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
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

    d = load_example("unit_square")
    A = d["A"].tocsr()
    B = d["B"] * scale
    C = symmetric_strength_of_connection(A)
    AggOp, Cpts = standard_aggregation(C)
    T, Bc = fit_candidates(AggOp, B)
    P = energy_prolongation_smoother(A, T, C, Bc, B, (False, {}),
                                     krylov="cgnr", weighting="diagonal", degree=2)
    np.save(a.out, np.asarray(P.data, dtype=np.float64).ravel(), allow_pickle=False)

if __name__ == "__main__":
    main()
