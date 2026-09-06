#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import poisson
from pyamg.relaxation.relaxation import gauss_seidel, jacobi


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--size", required=True, type=int)
    p.add_argument("--jacobi-sweeps", required=True, type=int)
    p.add_argument("--gs-sweeps", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    A = poisson((a.size, a.size), format="csr").astype(np.float64)
    n = A.shape[0]
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    b[0] *= float(cfg["rhs_scale"])
    x_j = np.zeros(n, dtype=np.float64)
    x_g = np.zeros(n, dtype=np.float64)
    jacobi(A, x_j, b, iterations=a.jacobi_sweeps, omega=2.0 / 3.0)
    gauss_seidel(A, x_g, b, iterations=a.gs_sweeps, sweep="symmetric")
    residuals = np.array([np.linalg.norm(b - A @ x_j), np.linalg.norm(b - A @ x_g)], dtype=np.float64)
    np.save(a.out, np.concatenate((x_j, x_g, residuals)), allow_pickle=False)


if __name__ == "__main__":
    main()
