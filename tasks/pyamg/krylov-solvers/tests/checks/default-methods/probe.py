#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import poisson
from pyamg.krylov import bicgstab, cg, cr, fgmres, gmres


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
    values = []
    for method in (cg, cr, gmres, fgmres, bicgstab):
        residuals = []
        solution, flag = method(matrix, rhs, x0=np.zeros_like(rhs), tol=0.0,
                                maxiter=4, residuals=residuals)
        padded = np.zeros(6, dtype=np.float64)
        take = min(len(residuals), padded.size)
        padded[:take] = np.asarray(residuals[:take], dtype=np.float64)
        values.extend((solution, padded,
                       np.asarray([len(residuals), flag], dtype=np.float64)))
    np.save(args.out, np.concatenate(values), allow_pickle=False)


if __name__ == "__main__":
    main()
