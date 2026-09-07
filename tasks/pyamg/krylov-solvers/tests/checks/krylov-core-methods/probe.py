#!/usr/bin/env python3
"""krylov-core-methods probe: the leaf's acceleration workload.

Five solvers run on one diagonally shifted 1-D Poisson operator of
SAB_PROBE_SIZE unknowns, each for a fixed number of steps at tol=0, and the
solution and the zero-padded residual history of each are graded. The operator
is large so that the sparse matrix-vector products and the global reductions --
the part a port to an accelerator has to make fast -- dominate the work.

Reads ic/<nominal|variant>/input.json, whose only active value is rhs_scale
(a uniform scale factor on every entry of the right-hand side); seed is carried
for provenance only.

The solver's second return value is not graded: with tol=0 pyamg's halting
status degenerates to the iteration count (pyamg/krylov/_cg.py:196
`return (x, it)`), and an iteration count is bookkeeping.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import poisson
from pyamg.krylov import bicgstab, cg, cr, fgmres, gmres


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--size', type=int, required=True)
    ap.add_argument('--iterations', type=int, required=True,
                    help='fixed step count for cg and cr')
    ap.add_argument('--gmres-iterations', type=int, required=True,
                    help='fixed step count for gmres and fgmres (also the Krylov basis width)')
    ap.add_argument('--bicgstab-iterations', type=int, required=True,
                    help='fixed step count for bicgstab')
    a = ap.parse_args()

    scale = float(json.loads(Path(a.input).read_text(encoding='utf-8'))['rhs_scale'])
    A = poisson((a.size,), format='csr').astype(np.float64)
    A.setdiag(A.diagonal() + 0.1)          # diagonal shift, as the previous round set it: it keeps
                                           # the 300000-unknown operator well conditioned
    b = np.linspace(0.5, 1.5, A.shape[0]) * scale

    windows = {cg: a.iterations, cr: a.iterations,
               gmres: a.gmres_iterations, fgmres: a.gmres_iterations,
               bicgstab: a.bicgstab_iterations}
    values = []
    for method in (cg, cr, gmres, fgmres, bicgstab):
        k = windows[method]
        residuals = []
        solution, _halting_status = method(A, b, x0=np.zeros_like(b), tol=0.0,
                                           maxiter=k, residuals=residuals)
        padded = np.zeros(k + 2, dtype=np.float64)
        take = min(len(residuals), padded.size)
        padded[:take] = np.asarray(residuals[:take], dtype=np.float64)
        values.extend([np.asarray(solution).ravel().astype(np.float64), padded])
    np.save(a.out, np.concatenate(values), allow_pickle=False)


if __name__ == '__main__':
    raise SystemExit(main())
