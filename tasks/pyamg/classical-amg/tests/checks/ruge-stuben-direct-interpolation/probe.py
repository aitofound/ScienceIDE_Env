#!/usr/bin/env python3
"""Probe: direct interpolation weights on the shipped bar unstructured-mesh matrix.

The graded array is indexed by fine-node identity only: see canonicalize().
"""
import argparse
import json
from pathlib import Path

import numpy as np


# --- canonicalization -------------------------------------------------------
# A coarse column number is not a physical identity. pyamg numbers the coarse
# unknowns by the rank of their C-point in fine order, but that is an
# implementation choice: a correct port may number them differently and would
# then build the same interpolation operator with its columns permuted, failing
# a positional comparison on storage order alone. The operator carries the
# identity itself, because classical AMG interpolates every C-point from itself
# with weight one: the block of P at the C-point rows is a permutation matrix
# whose unit entry in column j sits in the row of that column's own C-point.
# coarse_to_fine reads that map out of the operator (rather than assuming
# pyamg's numbering), and canonicalize scatters every coarse column onto the
# fine node it belongs to, so the graded array is indexed by fine-node identity
# alone. The C/F splitting is graded alongside, as the fine-indexed label array
# it is. Any operator whose C-point block is not a permutation matrix stops the
# probe loudly rather than being graded in an unknown order.
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


def canonicalize(P, splitting):
    """The graded array: the C/F labels, then P with every coarse column on its fine node."""
    P = np.asarray(P, dtype=np.float64)
    n = np.asarray(splitting).size
    full = np.zeros((P.shape[0], n), dtype=np.float64)
    full[:, coarse_to_fine(P, splitting)] = P
    return np.concatenate((np.asarray(splitting, dtype=np.float64), full.ravel()))


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    v = A2.data[0]
    A2.data[0] = v + ulps * np.spacing(v)
    return A2


def build(ulps):
    """The shipped problem and the operator under test (pyamg imported here so the
    canonicalization above can be exercised without a built pyamg)."""
    from pyamg.gallery import load_example
    from pyamg.strength import classical_strength_of_connection
    from pyamg.classical import split
    from pyamg.classical.interpolate import direct_interpolation

    A = load_example("bar")["A"].tocsr()
    A = perturb(A, ulps)
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S)
    P = direct_interpolation(A, S, splitting)
    return P.toarray(), splitting


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    P, splitting = build(int(cfg["matrix_perturb_ulps"]))
    np.save(a.out, canonicalize(P, splitting), allow_pickle=False)


if __name__ == "__main__":
    main()
