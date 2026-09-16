#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import gauge_laplacian
from pyamg.relaxation.relaxation import schwarz


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--grid", required=True, type=int)
    p.add_argument("--iterations", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # gauge_laplacian draws its gauge phases from numpy's legacy global stream, and PyAMG's
    # spectral-radius estimator (pyamg/util/linalg.py:179) starts Arnoldi from np.random.rand
    # when no initial guess is given. Pin the stream from the initial condition, as every probe
    # of this leaf does; nominal and variant carry the same seed, so the generated operator is
    # identical between them and only rhs_scale differs.
    np.random.seed(int(cfg["seed"]))
    A = gauge_laplacian(a.grid, beta=0.1).tocsr()
    n = A.shape[0]
    b = (np.linspace(0.5, 1.5, n) + 1.0j * np.linspace(1.5, 0.5, n)).astype(np.complex128)
    b[0] *= complex(cfg["rhs_scale"])
    x = np.zeros(n, dtype=np.complex128)
    schwarz(A, x, b, iterations=a.iterations, sweep="symmetric")
    residual = np.array([np.linalg.norm(b - A @ x)], dtype=np.complex128)
    vals = np.concatenate((x, residual))
    np.save(a.out, np.concatenate((vals.real, vals.imag)), allow_pickle=False)


if __name__ == "__main__":
    main()
