#!/usr/bin/env python3
"""真实Moran API生成权重、归一化和标准化；adapter不重写公式。"""
import argparse
import json
from pathlib import Path
import time

import networkx as nx
import numpy as np
import pandas as pd

from cassiopeia.data import CassiopeiaTree
from cassiopeia.tools import autocorrelation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.inputs.read_text(encoding="utf-8"))
    if config["schema_version"] != 1:
        raise ValueError("unsupported IC schema")
    args.out.mkdir(parents=True, exist_ok=True)
    for case in config["scenarios"]:
        graph = nx.DiGraph()
        graph.add_nodes_from(config["tree"]["nodes"])
        for edge in config["tree"]["edges"]:
            graph.add_edge(edge["parent"], edge["child"], length=edge["length"])
        tree = CassiopeiaTree(tree=graph)
        realization = {"scenario": case["id"], "leaf_ids": list(tree.leaves),
                       "edge_lengths": [{"parent": edge["parent"], "child": edge["child"],
                                         "length": float(tree.get_branch_length(edge["parent"], edge["child"]))}
                                        for edge in config["tree"]["edges"]],
                       "node_times": {name: float(value) for name, value in tree.get_times().items()}}
        print("TREE_REALIZATION=" + json.dumps(realization, sort_keys=True, allow_nan=False), flush=True)
        observations = config["observations"]
        X = pd.DataFrame(observations["values"], index=observations["leaf_ids"],
                         columns=observations["variable_ids"])[case["variables"]].copy()
        started = time.monotonic()
        if case["weight_source"] == "tree":
            result = autocorrelation.compute_morans_i(tree, X=X)
        elif case["weight_source"] == "custom":
            weights = config["custom_weights"]
            W = pd.DataFrame(weights["values"], index=weights["row_leaf_ids"],
                             columns=weights["column_leaf_ids"])
            result = autocorrelation.compute_morans_i(tree, X=X, W=W)
        else:
            raise ValueError("unsupported weight source")
        if len(case["variables"]) == 1:
            values = np.asarray(result)
            if values.size != 1 or values.dtype.kind not in "fiu":
                raise TypeError("univariate production Moran result must be one real number")
            values = values.reshape(1, 1)
            row_ids = column_ids = case["variables"]
        else:
            if not isinstance(result, pd.DataFrame):
                raise TypeError("multivariate production Moran result must carry its two variable axes")
            row_ids, column_ids = list(result.index), list(result.columns)
            values = result.to_numpy()
            if values.dtype.kind not in "fiu":
                raise TypeError("multivariate Moran values must be real numbers")
        if not all(isinstance(value, str) for value in row_ids + column_ids):
            raise TypeError("production variable role IDs must be strings")
        np.savez(args.out / (case["id"] + ".npz"), row_variables=np.asarray(row_ids, dtype=str),
                 column_variables=np.asarray(column_ids, dtype=str), morans_i=np.asarray(values, dtype=np.float64))
        print(f"scenario={case['id']} shape={values.shape[0]}x{values.shape[1]} "
              f"elapsed_seconds={time.monotonic()-started:.9f}", flush=True)


if __name__ == "__main__":
    main()
