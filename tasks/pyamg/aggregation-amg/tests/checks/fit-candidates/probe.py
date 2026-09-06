#!/usr/bin/env python3
"""Probe for fit-candidates: fit_candidates (pyamg/aggregation/tentative.py:9) called directly on the shipped bar mesh's own 6-mode near-null-space candidates, a fixed aggregate map from standard_aggregation."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.strength import symmetric_strength_of_connection
from pyamg.aggregation.aggregate import standard_aggregation
from pyamg.aggregation.tentative import fit_candidates

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    seed = int(cfg["seed"])
    scale = float(cfg["variant_scale"])

    d = load_example("bar")
    A = d["A"].tocsr()
    B0 = d["B"].copy()
    B0[0, 0] *= scale
    S = symmetric_strength_of_connection(A)
    AggOp, Cpts = standard_aggregation(S)
    Q, R = fit_candidates(AggOp, B0)
    values = np.concatenate((np.asarray(Q.data, dtype=np.float64).ravel(),
                             np.asarray(R, dtype=np.float64).ravel()))
    np.save(a.out, values, allow_pickle=False)

if __name__ == "__main__":
    main()
