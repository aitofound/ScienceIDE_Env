#!/usr/bin/env python3
"""Probe for sa-parameters-complex: smoothed_aggregation_solver on the shipped complex helmholtz_2D operator and its own near-null-space candidates (pyamg/aggregation/aggregation.py:26)."""
import argparse
import json
import os
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.aggregation import smoothed_aggregation_solver


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    seed = int(cfg["seed"])
    scale = float(cfg["variant_scale"])

    maxiter = int(os.environ.get("SAB_ITERS", "4"))
    d = load_example("helmholtz_2D")
    A = d["A"].tocsr()
    B0 = d["B"]
    n = A.shape[0]
    np.random.seed(seed)
    ml = smoothed_aggregation_solver(A, B0, max_coarse=25)
    np.random.seed(seed)
    x0 = np.random.rand(n) + 1j * np.random.rand(n)
    # Consistent right-hand side, built the way the upstream complex solver
    # tests build it (b = A @ random, test_aggregation.py:493).
    b = A @ (np.random.rand(n) * scale)
    residuals = []
    x = np.asarray(ml.solve(b, x0=x0, tol=0.0, maxiter=maxiter, residuals=residuals)).ravel()
    # The solution field is the production quantity of the solve (upstream
    # compares it directly in test_precision); the residual history and the
    # hierarchy depth follow it as deterministic algorithm diagnostics.
    values = np.concatenate((x.real.astype(np.float64), x.imag.astype(np.float64),
                             np.asarray(residuals, dtype=np.float64),
                             np.asarray([len(ml.levels)], dtype=np.float64)))
    np.save(a.out, values, allow_pickle=False)


if __name__ == "__main__":
    main()
