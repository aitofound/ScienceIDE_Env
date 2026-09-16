#!/usr/bin/env python3
"""Probe: AIR-preconditioned convergence on the nonsymmetric advection_2d operator.

TestAIR.test_upwind_advection checks that AIR restriction is exact in one
iteration on a hand-built 1-D upwind-advection bidiagonal matrix. This probe
exercises the same air_solver two-level reduction on the genuinely 2-D,
gallery-generated, nonsymmetric advection_2d operator (the class of problem
AIR restriction targets) at a knob-controlled, larger size.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import advection_2d
from pyamg.classical.air import air_solver


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", required=True, type=int)
    ap.add_argument("--maxiter", required=True, type=int)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    scale = float(cfg["rhs_scale"])
    A, _ = advection_2d((a.size, a.size))
    A = A.tocsr()
    f_relax = ("fc_jacobi", {"iterations": 1, "f_iterations": 1, "c_iterations": 0})
    ml = air_solver(A, postsmoother=f_relax, max_levels=4, max_coarse=20)
    np.random.seed(0)
    x0 = np.random.rand(A.shape[0])
    rand_b = np.random.rand(A.shape[0])
    rand_b[0] *= scale
    b = A @ rand_b
    residuals = []
    x = ml.solve(b, x0=x0, tol=0.0, maxiter=a.maxiter, residuals=residuals)
    values = np.concatenate((x, np.asarray(residuals, dtype=np.float64),
                             np.asarray([len(ml.levels)], dtype=np.float64)))
    np.save(a.out, values, allow_pickle=False)


if __name__ == "__main__":
    main()
