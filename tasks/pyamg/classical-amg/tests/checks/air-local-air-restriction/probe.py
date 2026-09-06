#!/usr/bin/env python3
"""Probe: local approximate ideal restriction (local_air) on recirc_flow.

TestAIR.test_air_restrict checks the exact restriction weights on 5- and
9-point structured 1-D/2-D cases. recirc_flow's irregular, nonsymmetric
connectivity gives continuous, non-trivial weights (unlike the structured
stencil's repeating 0.5/1.0 pattern), exercising the same weight formula.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from pyamg.gallery import load_example
from pyamg.strength import classical_strength_of_connection
from pyamg.classical import split
from pyamg.classical.interpolate import local_air


def perturb(A, ulps):
    if ulps == 0:
        return A
    A2 = A.copy()
    # index 5 (not 0): recirc_flow's entry 0 is pruned before local_air's
    # local stencil is assembled for any node, so perturbing it is inert;
    # entry 5 reaches an active stencil weight.
    v = A2.data[5]
    A2.data[5] = v + ulps * np.spacing(v)
    return A2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    A = load_example("recirc_flow")["A"].tocsr()
    A = perturb(A, int(cfg["matrix_perturb_ulps"]))
    S = classical_strength_of_connection(A, 0.0)
    splitting = split.RS(S).astype("intc")
    R = local_air(A, splitting)
    np.save(a.out, R.toarray().ravel(), allow_pickle=False)


if __name__ == "__main__":
    main()
