#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import load_example
from pyamg.relaxation.relaxation import polynomial
from pyamg.relaxation.chebyshev import chebyshev_polynomial_coefficients
from pyamg.util.linalg import approximate_spectral_radius


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--iterations", required=True, type=int)
    p.add_argument("--degree", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    A = load_example("unit_cube")["A"].tocsr().astype(np.float64)
    n = A.shape[0]
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    b[0] *= float(cfg["rhs_scale"])
    # setup_chebyshev (pyamg/relaxation/smoothing.py:630) scales its spectral window by the
    # spectral radius of A itself, not of D^-1 A: polynomial() below is applied to A, so the
    # window must bracket A's spectrum. Using rho(D^-1 A) here (about 1.2 rather than 120 on
    # this operator) makes the degree-3 polynomial an amplifier and the iterate overflows.
    rho = approximate_spectral_radius(A)
    lower, upper = (1.0 / 30.0) * rho, 1.1 * rho
    coeffs = -chebyshev_polynomial_coefficients(lower, upper, a.degree)[:-1]
    x = np.zeros(n, dtype=np.float64)
    polynomial(A, x, b, coeffs, iterations=a.iterations)
    residual = np.array([np.linalg.norm(b - A @ x)], dtype=np.float64)
    np.save(a.out, np.concatenate((x, residual)), allow_pickle=False)


if __name__ == "__main__":
    main()
