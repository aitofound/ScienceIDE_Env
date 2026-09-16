#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import load_example
from pyamg.relaxation.relaxation import schwarz


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--iterations", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    A = load_example("airfoil")["A"].tocsr().astype(np.float64)
    n = A.shape[0]
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    b[0] *= float(cfg["rhs_scale"])
    x = np.zeros(n, dtype=np.float64)
    schwarz(A, x, b, iterations=a.iterations, sweep="symmetric")
    residual = np.array([np.linalg.norm(b - A @ x)], dtype=np.float64)
    np.save(a.out, np.concatenate((x, residual)), allow_pickle=False)


if __name__ == "__main__":
    main()
