#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import advection_2d, poisson
from pyamg import ruge_stuben_solver


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--grid", required=True, type=int)
    p.add_argument("--iterations", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    A, _rhs = advection_2d((a.grid, a.grid), theta=np.pi / 4.0)
    A = A.tocsr().astype(np.float64)
    n = A.shape[0]
    # advection_2d eliminates the left/bottom boundary DOFs, so its matrix is smaller than
    # grid*grid; build Anew at exactly A's size (a 1-D Poisson operator of length n) so
    # change_solve_matrix swaps in a same-shape matrix, as the upstream test itself does
    # (A = poisson((20,)); Anew = eye_array(A.shape[0])).
    ml = ruge_stuben_solver(A, presmoother="chebyshev", postsmoother=None, max_levels=2, max_coarse=10)
    diag_before = np.array([ml.levels[0].A.diagonal()[0]], dtype=np.float64)
    Anew = poisson((n,), format="csr").astype(np.float64)
    ml.change_solve_matrix(Anew)
    diag_after = np.array([ml.levels[0].A.diagonal()[0]], dtype=np.float64)
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    b[0] *= float(cfg["rhs_scale"])
    x = np.zeros(n, dtype=np.float64)
    ml.levels[0].presmoother(ml.levels[0].A, x, b)
    for _ in range(1, a.iterations):
        ml.levels[0].presmoother(ml.levels[0].A, x, b)
    residual = np.array([np.linalg.norm(b - Anew @ x)], dtype=np.float64)
    np.save(a.out, np.concatenate((diag_before, diag_after, x, residual)), allow_pickle=False)


if __name__ == "__main__":
    main()
