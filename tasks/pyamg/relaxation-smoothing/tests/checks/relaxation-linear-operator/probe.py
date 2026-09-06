#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np
from pyamg.gallery import poisson, elasticity
from pyamg.relaxation.utils import relaxation_as_linear_operator


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--size", required=True, type=int)
    a = p.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    # PyAMG's spectral-radius estimator (pyamg/util/linalg.py:179) starts its Arnoldi
    # iteration from np.random.rand when no initial guess is given, so any code path that
    # reaches it depends on numpy's legacy global stream. Pin it from the initial condition
    # in every probe of this leaf so the graded observable is reproducible; nominal and
    # variant carry the same seed, so the only difference between them is rhs_scale.
    np.random.seed(int(cfg["seed"]))
    seed = int(cfg["seed"])
    rng = np.random.RandomState(seed)
    values = []
    matrices = [poisson((a.size, a.size), format="csr").astype(np.float64),
                (1.0j * poisson((a.size, a.size), format="csr")).astype(np.complex128),
                elasticity.linear_elasticity((a.size, a.size))[0].astype(np.float64),
                (1.0j * elasticity.linear_elasticity((a.size, a.size))[0]).astype(np.complex128)]
    for A in matrices:
        complex_case = np.iscomplexobj(A)
        b = rng.rand(A.shape[0])
        x = rng.rand(A.shape[0])
        if complex_case:
            b = b + 1j * rng.rand(A.shape[0])
            x = x + 1j * rng.rand(A.shape[0])
        b[0] *= float(cfg["rhs_scale"])
        for method in ("gauss_seidel", "jacobi", "block_gauss_seidel", "block_jacobi"):
            y = relaxation_as_linear_operator((method, {"iterations": 2}), A, b) @ x
            values.extend((np.asarray(y.real, dtype=np.float64), np.asarray(y.imag, dtype=np.float64)))
    np.save(a.out, np.concatenate(values), allow_pickle=False)


if __name__ == "__main__":
    main()
