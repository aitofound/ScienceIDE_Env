#!/usr/bin/env python3
"""Probe: dense/CSR/BSR/CSC input equivalence for ruge_stuben_solver on a genuinely
blocked linear_elasticity operator (blocksize 2x2), instead of a reshaped scalar
Poisson matrix as in TestSolverPerformance.test_matrix_formats. Grades the
coarsest-level operator built from each input format.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import warnings
from scipy.sparse import SparseEfficiencyWarning
from pyamg.gallery import linear_elasticity
from pyamg.classical.classical import ruge_stuben_solver

warnings.simplefilter("ignore", SparseEfficiencyWarning)


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    v = A2.data[0]
    A2.data[0] = v + ulps * np.spacing(v)
    return A2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", required=True, type=int)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    Ael, _ = linear_elasticity((a.size, a.size))
    Ael = perturb(Ael.tobsr(blocksize=(2, 2)), int(cfg["matrix_perturb_ulps"]))
    forms = {
        "bsr": Ael,
        "csr": Ael.tocsr(),
        "csc": Ael.tocsr().tocsc(),
        "dense": Ael.tocsr().toarray(),
    }
    chunks = []
    for name in ("bsr", "csr", "csc", "dense"):
        ml = ruge_stuben_solver(forms[name], max_coarse=20)
        Ac = ml.levels[-1].A
        Ac = Ac.toarray() if hasattr(Ac, "toarray") else Ac
        chunks.append(np.ravel(Ac).astype(np.float64))
        chunks.append(np.asarray([len(ml.levels)], dtype=np.float64))
    np.save(a.out, np.concatenate(chunks), allow_pickle=False)


if __name__ == "__main__":
    main()
