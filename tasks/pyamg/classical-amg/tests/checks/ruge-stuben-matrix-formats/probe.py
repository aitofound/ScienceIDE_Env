#!/usr/bin/env python3
"""Probe: dense/CSR/BSR/CSC input equivalence for ruge_stuben_solver on a genuinely
blocked linear_elasticity operator (blocksize 2x2), instead of a reshaped scalar
Poisson matrix as in TestSolverPerformance.test_matrix_formats. Grades the
coarsest-level operator built from each input format.

The graded array is indexed by fine-node identity only: see canonicalize().
"""
import argparse
import json
import warnings
from pathlib import Path

import numpy as np


# --- canonicalization -------------------------------------------------------
# A coarse row or column number is not a physical identity, and the coarsest
# level of a hierarchy is several coarsenings away from the fine grid. pyamg
# numbers the coarse unknowns of each level by the rank of their C-point in that
# level's own order, but that is an implementation choice: a correct port may
# number them differently and would then build the same coarse operator with its
# rows and columns permuted, failing a positional comparison on storage order
# alone. Each level's interpolation operator carries the identity of its own
# coarse dofs, because classical AMG interpolates every C-point from itself with
# weight one: the block of P at the C-point rows is a permutation matrix whose
# unit entry in column j sits in the row of that column's own C-point.
# coarse_to_fine reads that map out of each level's P (rather than assuming
# pyamg's numbering), canonicalize composes those maps through the hierarchy to
# give the finest-grid node behind every coarsest dof, and the coarsest operator
# is then ordered by that fine-node index. The composed fine indices and the
# level-0 C/F splitting are graded alongside, so the identity itself is compared
# and not only the values. A level whose C-point block is not a permutation
# matrix stops the probe loudly rather than being graded in an unknown order.
def coarse_to_fine(M, splitting):
    """Fine-node index of every coarse dof, read out of the operator itself."""
    M = np.asarray(M, dtype=np.float64)
    cpts = np.flatnonzero(np.asarray(splitting) == 1)
    if M.shape[1] != cpts.size:
        raise SystemExit(f"probe: {M.shape[1]} coarse dofs but {cpts.size} C-points")
    block = M[cpts, :]
    hit = np.abs(block) > 0.5
    if not (hit.sum(axis=0) == 1).all() or not (hit.sum(axis=1) == 1).all():
        raise SystemExit("probe: the C-point block of the operator is not a permutation matrix")
    rank = np.argmax(hit, axis=0)
    if np.abs(block[rank, np.arange(M.shape[1])] - 1.0).max() > 1e-8:
        raise SystemExit("probe: a C-point does not interpolate from itself with weight one")
    return cpts[rank]


def dense(M):
    return M.toarray() if hasattr(M, "toarray") else np.asarray(M, dtype=np.float64)


def canonicalize(hierarchies):
    """The graded array, per input format: the level-0 C/F labels, the finest-grid
    node behind every coarsest dof, the coarsest operator in that fine-node order,
    and the number of levels. `hierarchies` is one level list per input format;
    every level but the coarsest carries .P and .splitting, and every level .A."""
    chunks = []
    for levels in hierarchies:
        idx = np.arange(dense(levels[-1].A).shape[0])
        for lvl in reversed(levels[:-1]):
            idx = coarse_to_fine(dense(lvl.P), lvl.splitting)[idx]
        order = np.argsort(idx)
        Ac = dense(levels[-1].A)[np.ix_(order, order)]
        chunks.append(np.asarray(levels[0].splitting, dtype=np.float64))
        chunks.append(idx[order].astype(np.float64))
        chunks.append(np.ravel(Ac).astype(np.float64))
        chunks.append(np.asarray([len(levels)], dtype=np.float64))
    return np.concatenate(chunks)


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    v = A2.data[0]
    A2.data[0] = v + ulps * np.spacing(v)
    return A2


def build(size, ulps):
    """The shipped problem and the four hierarchies under test (pyamg imported here
    so the canonicalization above can be exercised without a built pyamg)."""
    from scipy.sparse import SparseEfficiencyWarning
    from pyamg.gallery import linear_elasticity
    from pyamg.classical.classical import ruge_stuben_solver

    warnings.simplefilter("ignore", SparseEfficiencyWarning)
    Ael, _ = linear_elasticity((size, size))
    Ael = perturb(Ael.tobsr(blocksize=(2, 2)), ulps)
    forms = {
        "bsr": Ael,
        "csr": Ael.tocsr(),
        "csc": Ael.tocsr().tocsc(),
        "dense": Ael.tocsr().toarray(),
    }
    return [ruge_stuben_solver(forms[name], max_coarse=20).levels
            for name in ("bsr", "csr", "csc", "dense")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", required=True, type=int)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    np.save(a.out, canonicalize(build(a.size, int(cfg["matrix_perturb_ulps"]))), allow_pickle=False)


if __name__ == "__main__":
    main()
