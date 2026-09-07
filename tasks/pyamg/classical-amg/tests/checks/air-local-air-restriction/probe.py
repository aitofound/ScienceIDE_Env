#!/usr/bin/env python3
"""Probe: local approximate ideal restriction (local_air) on recirc_flow.

TestAIR.test_air_restrict checks the exact restriction weights on 5- and
9-point structured 1-D/2-D cases. recirc_flow's irregular, nonsymmetric
connectivity gives continuous, non-trivial weights (unlike the structured
stencil's repeating 0.5/1.0 pattern), exercising the same weight formula.

The graded array is indexed by fine-node identity only: see canonicalize().
"""
import argparse
import json
from pathlib import Path

import numpy as np


# --- canonicalization -------------------------------------------------------
# A coarse row number is not a physical identity. pyamg numbers the coarse
# unknowns by the rank of their C-point in fine order, but that is an
# implementation choice: a correct port may number them differently and would
# then build the same restriction operator with its rows permuted, failing
# a positional comparison on storage order alone. The operator carries the
# identity itself, because approximate ideal restriction takes every C-point
# from itself with weight one: the block of R at the C-point columns is a permutation
# matrix whose unit entry in row j sits in the column of that row's own C-point
# (coarse_to_fine is given R transposed, so it reads the same block).
# coarse_to_fine reads that map out of the operator (rather than assuming
# pyamg's numbering), and canonicalize scatters every coarse row onto the
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
        raise SystemExit("probe: a C-point is not restricted from itself with weight one")
    return cpts[rank]


def canonicalize(R, splitting):
    """The graded array: the C/F labels, then R with every coarse row on its fine node."""
    R = np.asarray(R, dtype=np.float64)
    n = np.asarray(splitting).size
    full = np.zeros((n, R.shape[1]), dtype=np.float64)
    full[coarse_to_fine(R.T, splitting), :] = R
    return np.concatenate((np.asarray(splitting, dtype=np.float64), full.ravel()))


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    # index 5 (not 0): recirc_flow's entry 0 is pruned before local_air's
    # local stencil is assembled for any node, so perturbing it is inert;
    # entry 5 reaches an active stencil weight.
    v = A2.data[5]
    A2.data[5] = v + ulps * np.spacing(v)
    return A2


def build(ulps):
    """The shipped problem and the operator under test (pyamg imported here so the
    canonicalization above can be exercised without a built pyamg)."""
    from pyamg.gallery import load_example
    from pyamg.strength import classical_strength_of_connection
    from pyamg.classical import split
    from pyamg.classical.interpolate import local_air

    A = load_example("recirc_flow")["A"].tocsr()
    A = perturb(A, ulps)
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S).astype("intc")
    R = local_air(A, splitting)
    return R.toarray(), splitting


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    R, splitting = build(int(cfg["matrix_perturb_ulps"]))
    np.save(a.out, canonicalize(R, splitting), allow_pickle=False)


if __name__ == "__main__":
    main()
