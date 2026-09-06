#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import diffusion_stencil_2d, stencil_grid
from pyamg import smoothed_aggregation_solver
from pyamg.relaxation.smoothing import change_smoothers


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--grid", required=True, type=int)
    p.add_argument("--cycles", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    stencil = diffusion_stencil_2d(epsilon=0.01, theta=np.pi / 4.0, type="FE")
    A = stencil_grid(stencil, (a.grid, a.grid), format="csr").astype(np.float64)
    n = A.shape[0]
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    b[0] *= float(cfg["rhs_scale"])
    ml = smoothed_aggregation_solver(A, max_coarse=10)
    change_smoothers(ml, presmoother=("gauss_seidel", {"sweep": "symmetric"}), postsmoother=("chebyshev", {}))
    x0 = np.zeros(n, dtype=np.float64)
    x = ml.solve(b, x0=x0, tol=0.0, maxiter=a.cycles, cycle="V")
    residual = np.array([np.linalg.norm(b - A @ x)], dtype=np.float64)
    symmetric_flag = np.array([1.0 if ml.symmetric_smoothing else 0.0], dtype=np.float64)
    np.save(a.out, np.concatenate((x, residual, symmetric_flag)), allow_pickle=False)


if __name__ == "__main__":
    main()
