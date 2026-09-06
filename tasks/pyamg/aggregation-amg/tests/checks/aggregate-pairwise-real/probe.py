#!/usr/bin/env python3
"""Probe for aggregate-pairwise-real: pairwise_aggregation called directly (pyamg/aggregation/aggregate.py:181), a distinct matching-based kernel from standard/naive aggregation, on the shipped 3-D unit_cube mesh."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.aggregation.aggregate import pairwise_aggregation
def canon_ids(AggOp):
    """Order-independent identity for an aggregate map: for every dof, the
    minimum dof index in its aggregate (the aggregate's canonical
    representative). Invariant to how the algorithm numbers or orders
    aggregates (columns); NOT invariant to which dofs actually group
    together, which is the physics under test."""
    AggOp = AggOp.tocsc()
    n = AggOp.shape[0]
    ncols = AggOp.shape[1]
    rep = np.full(ncols, -1.0, dtype=np.float64)
    for j in range(ncols):
        col = AggOp.indices[AggOp.indptr[j]:AggOp.indptr[j + 1]]
        rep[j] = float(col.min()) if col.size else -1.0
    Acsr = AggOp.tocsr()
    dof_agg = np.full(n, -1, dtype=np.int64)
    for i in range(n):
        row = Acsr.indices[Acsr.indptr[i]:Acsr.indptr[i + 1]]
        if row.size:
            dof_agg[i] = row[0]
    return np.array([rep[a] if a >= 0 else -1.0 for a in dof_agg], dtype=np.float64), float(ncols)



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    seed = int(cfg["seed"])
    scale = float(cfg["variant_scale"])

    A = load_example("unit_cube")["A"].tocsr().copy()
    A.data = A.data.copy()
    A.data[0] *= scale
    P, Cpts = pairwise_aggregation(A, matchings=1, theta=0.25, norm="min")
    ids, n = canon_ids(P)
    values = np.concatenate((ids, [n]))
    np.save(a.out, values, allow_pickle=False)

if __name__ == "__main__":
    main()
