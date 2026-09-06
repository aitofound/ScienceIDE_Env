#!/usr/bin/env python3
"""Probe for energy-prolongation-bsr: energy_prolongation_smoother on a BSR linear-elasticity operator, reaching the same block-sparse incomplete-matrix-multiply kernel (pyamg/amg_core/smoothed_aggregation.h:970) test_incomplete_mat_mult_bsr exercises directly."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from scipy import sparse
from pyamg.gallery import linear_elasticity
from pyamg.strength import symmetric_strength_of_connection
from pyamg.aggregation.aggregate import standard_aggregation
from pyamg.aggregation.tentative import fit_candidates
from pyamg.aggregation.smooth import energy_prolongation_smoother

def canonical_aggregate_order(AggOp):
    """Aggregate numbering is storage, not physics.

    `standard_aggregation` numbers its aggregates by the order in which it
    scans rows (pyamg/aggregation/aggregate.py:12); a correct implementation
    that groups the same degrees of freedom may number them differently, and
    the leaf's aggregate-* checks already grade membership through an
    order-independent identity for that reason. Everything built on top of the
    aggregate map inherits the numbering: the columns of the tentative and
    smoothed prolongators and the row blocks of the coarse candidate matrix R.

    This returns the permutation that puts the aggregates in a canonical
    order -- each aggregate keyed by the smallest fine index it holds, an
    identity the output itself carries. Measured on the pinned build: after a
    random renumbering of the same aggregates, reordering this way brings the
    graded prolongator entries back to within 3.6e-16 (unit_square) and
    4.2e-15 (BSR elasticity) of the originals, while grading them in the raw
    numbering leaves them 0.53 and 0.22 apart.
    """
    C = AggOp.tocsc()
    key = np.array([C.indices[C.indptr[j]:C.indptr[j + 1]].min() if C.indptr[j + 1] > C.indptr[j] else 2 ** 31
                    for j in range(C.shape[1])], dtype=np.int64)
    return np.argsort(key, kind="stable")


def canonical_columns(M, order, block):
    """M with its aggregate column blocks put in `order`, indices re-sorted so
    that the CSR data array is read in a canonical, numbering-free sequence."""
    cols = (np.asarray(order)[:, None] * block + np.arange(block)).ravel()
    out = sparse.csr_matrix(M)[:, cols]
    out.sort_indices()
    return out


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
    Pc = canonical_columns(P, canonical_aggregate_order(AggOp), B0.shape[1])
    np.save(a.out, np.asarray(Pc.data, dtype=np.float64).ravel(), allow_pickle=False)

if __name__ == "__main__":
    main()
