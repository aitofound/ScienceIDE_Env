#!/usr/bin/env python3
"""从固定 IC 运行官方场景的生产 map 路径，不采集 assertions。"""
import argparse
from functools import partial
import json
from pathlib import Path
import time

import numba
import numpy as np
import pandas as pd

from cassiopeia.data import CassiopeiaTree
from cassiopeia.solver.dissimilarity_functions import cluster_dissimilarity


def delta_fn(x, y, missing_state, priors):
    # 官方 cassiopeia_tree_test.py:21-31；这里 -1 仍参与不等比较。
    d = 0
    for i in range(len(x)):
        if x[i] != y[i]:
            d += 1
    return d


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=2)
    args = parser.parse_args()
    if args.threads < 1:
        parser.error("threads must be positive")
    config = json.loads(args.inputs.read_text(encoding="utf-8"))
    if config["schema_version"] != 1:
        raise ValueError("unsupported IC schema")
    args.out.mkdir(parents=True, exist_ok=True)
    print(f"NUMBA_DISABLE_JIT={numba.config.DISABLE_JIT}", flush=True)
    for case in config["cases"]:
        rows = [[tuple(value) if isinstance(value, list) else value for value in row]
                for row in config["matrices"][case["matrix"]]]
        character_matrix = pd.DataFrame(rows, index=config["cell_ids"], columns=config["character_ids"])
        distance = delta_fn
        if case["distance"] == "cluster_delta_mean_unnormalized":
            distance = partial(cluster_dissimilarity, delta_fn, normalize=False)
        elif case["distance"] != "delta":
            raise ValueError("unsupported distance")
        start = time.monotonic()
        tree = CassiopeiaTree(character_matrix=character_matrix,
                             missing_state_indicator=config["missing_state_indicator"])
        tree.compute_dissimilarity_map(distance, threads=args.threads if case["parallel"] else 1)
        matrix = tree.get_dissimilarity_map()
        if (matrix.shape != (len(config["cell_ids"]),) * 2
                or not matrix.index.is_unique or not matrix.columns.is_unique
                or set(matrix.index) != set(config["cell_ids"])
                or set(matrix.columns) != set(config["cell_ids"])):
            raise ValueError("production map has invalid cell axes")
        cells = list(matrix.index)
        matrix = matrix.loc[cells, cells]
        values = matrix.to_numpy()
        if not np.isfinite(values).all() or not np.array_equal(values, values.T):
            raise ValueError("production map must be finite and symmetric")
        pairs = [(a, b) for i, a in enumerate(cells) for b in cells[i:]]
        distances = np.array([matrix.loc[a, b] for a, b in pairs], dtype=np.float64)
        np.savez(args.out / (case["id"] + ".npz"), cell_ids=np.asarray(cells, dtype=str),
                 pairs=np.asarray(pairs, dtype=str), distance=distances)
        print(f"case={case['id']} pairs={len(pairs)} elapsed_seconds={time.monotonic() - start:.9f}", flush=True)


if __name__ == "__main__":
    main()
