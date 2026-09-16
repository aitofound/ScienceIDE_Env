#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import load_example
from pyamg.classical.split import RS
from pyamg.relaxation.relaxation import cf_jacobi, fc_jacobi


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
    A = load_example("knot")["A"].tocsr().astype(np.float64)
    n = A.shape[0]
    b = np.linspace(0.5, 1.5, n, dtype=np.float64)
    # Unlike the other probes of this leaf, the variant scales the whole right-hand side and
    # not only its first entry: a five-ulp change to one F-point entry of b is absorbed below
    # one ulp of the graded vector by the omega=0.7 damped C/F cycles (measured: the two
    # observables came out bit-identical), so a single-entry variant would supply no
    # calibration evidence at all.
    b *= float(cfg["rhs_scale"])
    splitting = RS(A)
    c_pts = np.where(splitting == 1)[0]
    f_pts = np.where(splitting == 0)[0]
    x_cf = np.zeros(n, dtype=np.float64)
    x_fc = np.zeros(n, dtype=np.float64)
    cf_jacobi(A, x_cf, b, c_pts, f_pts, omega=0.7, iterations=a.iterations, f_iterations=2, c_iterations=2)
    fc_jacobi(A, x_fc, b, c_pts, f_pts, omega=0.7, iterations=a.iterations, f_iterations=2, c_iterations=2)
    residuals = np.array([np.linalg.norm(b - A @ x_cf), np.linalg.norm(b - A @ x_fc)], dtype=np.float64)
    coarse_fraction = np.array([float(c_pts.size) / n], dtype=np.float64)
    np.save(a.out, np.concatenate((x_cf, x_fc, residuals, coarse_fraction)), allow_pickle=False)


if __name__ == "__main__":
    main()
