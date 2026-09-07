#!/usr/bin/env python3
"""Probe: amg_core.remove_strong_FF_connections on a 2-D Poisson strength graph.

The upstream unit test exercises this C++ primitive by hand on a fixed 6x6
matrix; this probe runs the same primitive, through the same Python call
(RS splitting then removal), on a larger Poisson-derived strength graph.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg import amg_core
from pyamg.gallery import poisson
from pyamg.strength import classical_strength_of_connection
from pyamg.classical import split


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
    A = poisson((a.size, a.size), format="csr").astype(np.float64)
    A = perturb(A, int(cfg["matrix_perturb_ulps"]))
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S)
    C = S.copy()
    C.data[:] = 1.0
    C = C.multiply(A).tocsr()
    amg_core.remove_strong_FF_connections(A.shape[0], C.indptr, C.indices, C.data,
                                          splitting.astype(C.indices.dtype))
    np.save(a.out, C.toarray().ravel(), allow_pickle=False)


if __name__ == "__main__":
    main()
