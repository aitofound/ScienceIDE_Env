#!/usr/bin/env python3
"""Probe: compatible-relaxation C/F selection on a 2-D Poisson problem.

TestCR.test_cr only bounds the coarsening fraction into a range; this probe
grades the actual splitting vector CR.CR produces (a deterministic, per-node
discrete label, since B=None gives the fixed constant target vector and no
random relaxation start) as an exact-match pointwise output.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import poisson
from pyamg.classical.cr import CR


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
    splitting = CR(A, method="habituated", thetacr=0.7, thetacs="auto", maxiter=20)
    np.save(a.out, splitting.astype(np.float64), allow_pickle=False)


if __name__ == "__main__":
    main()
