#!/usr/bin/env python3
"""Probe: fixed-maxiter Ruge-Stuben convergence under direct and classical interpolation.

Mirrors TestSolverPerformance.test_poisson's loop over interpolation choices on
a 2-D Poisson problem, but at a larger, knob-controlled grid size and with a
fixed iteration count (tol=0) instead of the gate's own convergence-ratio bound.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import poisson
from pyamg.classical.classical import ruge_stuben_solver


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", required=True, type=int)
    ap.add_argument("--maxiter", required=True, type=int)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    scale = float(cfg["rhs_scale"])
    A = poisson((a.size, a.size), format="csr").astype(np.float64)
    np.random.seed(0)
    x0 = np.random.rand(A.shape[0])
    rand_b = np.random.rand(A.shape[0])
    rand_b[0] *= scale
    b = A @ rand_b
    chunks = []
    for interp in ["direct", ("classical", {"modified": False}), ("classical", {"modified": True})]:
        ml = ruge_stuben_solver(A, interpolation=interp, max_coarse=50)
        residuals = []
        x = ml.solve(b, x0=x0.copy(), tol=0.0, maxiter=a.maxiter, residuals=residuals)
        chunks.append(x)
        chunks.append(np.asarray(residuals, dtype=np.float64))
        chunks.append(np.asarray([len(ml.levels)], dtype=np.float64))
    np.save(a.out, np.concatenate(chunks), allow_pickle=False)


if __name__ == "__main__":
    main()
