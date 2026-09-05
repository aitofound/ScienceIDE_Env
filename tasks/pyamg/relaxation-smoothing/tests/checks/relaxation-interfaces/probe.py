#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import poisson
from pyamg.relaxation.relaxation import gauss_seidel, jacobi


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--size", required=True, type=int)
    args = parser.parse_args()
    scale = float(json.loads(Path(args.input).read_text())["rhs_scale"])
    matrix = poisson((args.size,), format="csr").astype(np.float64)
    rhs = np.linspace(0.5, 1.5, matrix.shape[0], dtype=np.float64)
    rhs[0] *= scale
    x_jacobi = np.zeros_like(rhs)
    x_gs = np.zeros_like(rhs)
    jacobi(matrix, x_jacobi, rhs, iterations=3, omega=2.0 / 3.0)
    gauss_seidel(matrix, x_gs, rhs, iterations=2, sweep="symmetric")
    residuals = np.asarray([np.linalg.norm(rhs - matrix @ x_jacobi),
                            np.linalg.norm(rhs - matrix @ x_gs)], dtype=np.float64)
    np.save(args.out, np.concatenate((x_jacobi, x_gs, residuals)), allow_pickle=False)


if __name__ == "__main__":
    main()
