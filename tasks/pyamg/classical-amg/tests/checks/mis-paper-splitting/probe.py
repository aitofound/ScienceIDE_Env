#!/usr/bin/env python3
"""Probe: maximal-independent-set splitting on the published Figure 4.1 reference graph.

TestMIS.test_paper_result is itself an exact-match comparison against a
published reference splitting on a fixed 7x7 mesh with fixed weights (the
size and weights are fixed by the cited figure, not a size we chose). The
probe reuses that same published graph and weights rather than an unrelated
sentinel, and grades the resulting splitting vector (an exact-match discrete
per-node output; MIS is deterministic here because explicit, non-random
weights are passed).
"""
import argparse
import json
from pathlib import Path

import numpy as np
import pyamg


REFERENCE_WEIGHTS = [
    3.2, 5.6, 5.8, 5.6, 5.9, 5.9, 3.0,
    5.0, 8.8, 8.5, 8.6, 8.7, 8.9, 5.3,
    5.3, 8.7, 8.3, 8.4, 8.3, 8.8, 5.9,
    5.7, 8.6, 8.3, 8.8, 8.3, 8.1, 5.0,
    5.9, 8.1, 8.8, 8.9, 8.4, 8.2, 5.9,
    5.2, 8.0, 8.5, 8.2, 8.6, 8.9, 5.1,
    3.7, 5.3, 5.0, 5.9, 5.4, 5.3, 3.4,
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cfg = json.loads(Path(a.input).read_text())
    ulps = int(cfg["weight_perturb_ulps"])
    S = pyamg.gallery.poisson((7, 7), type="FE", format="csr")
    w = np.array(REFERENCE_WEIGHTS, dtype=np.float64)
    if ulps:
        w[0] = w[0] + ulps * np.spacing(w[0])
    splitting = pyamg.classical.split.MIS(S, w)
    np.save(a.out, splitting.astype(np.float64), allow_pickle=False)


if __name__ == "__main__":
    main()
