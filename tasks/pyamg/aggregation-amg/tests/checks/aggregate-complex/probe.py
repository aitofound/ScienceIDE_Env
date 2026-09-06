#!/usr/bin/env python3
"""Probe for aggregate-complex: standard_aggregation and naive_aggregation on the shipped real knot mesh with a small imaginary perturbation added (the same construction TestComplexAggregate's own setUp uses on Poisson)."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.strength import symmetric_strength_of_connection
from pyamg.aggregation.aggregate import standard_aggregation, naive_aggregation
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

    A = load_example("knot")["A"].tocsr().astype(np.complex128)
    np.random.seed(seed)
    A.data = A.data + 0.001j * np.random.rand(A.nnz)
    A.data[0] *= scale
    S = symmetric_strength_of_connection(A)
    std_ids, std_n = canon_ids(standard_aggregation(S)[0])
    naive_ids, naive_n = canon_ids(naive_aggregation(S)[0])
    values = np.concatenate((std_ids, [std_n], naive_ids, [naive_n]))
    np.save(a.out, values, allow_pickle=False)

if __name__ == "__main__":
    main()
