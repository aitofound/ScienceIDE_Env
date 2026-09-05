#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.classical.classical import ruge_stuben_solver
from pyamg.gallery import poisson


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--size", required=True, type=int)
    args = parser.parse_args()
    scale = float(json.loads(Path(args.input).read_text())["rhs_scale"])
    matrix = poisson((args.size, args.size), format="csr").astype(np.float64)
    rhs = np.linspace(0.5, 1.5, matrix.shape[0], dtype=np.float64)
    rhs[0] *= scale
    residuals = []
    solver = ruge_stuben_solver(matrix, max_coarse=5)
    solution = solver.solve(rhs, x0=np.zeros_like(rhs), tol=0.0, maxiter=4,
                            residuals=residuals)
    values = np.concatenate((solution, np.asarray(residuals, dtype=np.float64),
                             np.asarray([len(solver.levels)], dtype=np.float64)))
    np.save(args.out, values, allow_pickle=False)


if __name__ == "__main__":
    main()
