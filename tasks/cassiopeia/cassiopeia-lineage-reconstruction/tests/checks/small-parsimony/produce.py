#!/usr/bin/env python3
"""将真实 Cassiopeia API 的树、状态及转移计数写成独立科学 NPZ。"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import networkx as nx
import numpy as np
import pandas as pd

import cassiopeia as cas
from cassiopeia.tools.small_parsimony import fitch_hartigan_bottom_up, fitch_hartigan_top_down


def make_tree(fixture):
    graph = nx.DiGraph()
    graph.add_edges_from(fixture["edges"])
    metadata = pd.DataFrame({"nucleotide": fixture["nucleotide"], "quality": fixture["quality"]},
                            index=fixture["cell_ids"])
    return cas.data.CassiopeiaTree(tree=graph, cell_meta=metadata)


def produce_case(case, fixture, meta_item):
    tree = make_tree(fixture)
    operation = case["operation"]
    state_key = case.get("state_key", "S1")
    label_key = case.get("label_key", "label")
    root = case.get("root")
    states = case.get("unique_states", list(dict.fromkeys(fixture["nucleotide"])))
    matrix, reported_score = None, None
    if operation == "bottom_up":
        result = fitch_hartigan_bottom_up(tree, meta_item, add_key=state_key, copy=case["copy"])
        if case["copy"]:
            tree = result
        fitch_hartigan_top_down(tree, state_key=state_key, label_key=label_key)
    elif operation == "top_down":
        tree = fitch_hartigan_bottom_up(tree, meta_item, copy=True)
        fitch_hartigan_top_down(tree, label_key=label_key)
    elif operation == "fitch":
        cas.tl.fitch_hartigan(tree, meta_item)
    elif operation == "score_infer":
        reported_score = cas.tl.score_small_parsimony(tree, meta_item, infer_ancestral_states=True)
        # score API 在私有副本推断；另调公开生产 API 生成可验证全赋值，绝不伪造内部赋值。
        cas.tl.fitch_hartigan(tree, meta_item)
    elif operation == "score_assigned":
        tree = cas.tl.fitch_hartigan(tree, meta_item, label_key=label_key, copy=True)
    elif operation == "count":
        infer = case.get("infer_ancestral_states", True)
        if not infer:
            fitch_hartigan_bottom_up(tree, meta_item, add_key=state_key)
        matrix = cas.tl.fitch_count(tree, meta_item, root=root, infer_ancestral_states=infer,
                                    state_key=state_key, unique_states=case.get("unique_states"))
        cas.tl.fitch_hartigan(tree, meta_item, root=root)
        if root is not None:
            tree = tree.subset_clade(root, copy=True)
    else:
        raise ValueError(f"unsupported operation {operation}")
    if reported_score is None:
        reported_score = cas.tl.score_small_parsimony(tree, meta_item, infer_ancestral_states=False,
                                                    label_key=label_key)
    nodes = tree.nodes
    payload = {
        "node_ids": np.asarray(nodes, dtype=np.str_),
        "leaf_ids": np.asarray([n if tree.is_leaf(n) else "" for n in nodes], dtype=np.str_),
        "edges": np.asarray(tree.edges, dtype=np.str_),
        "assignment": np.asarray([tree.get_attribute(n, label_key) for n in nodes], dtype=np.str_),
        "score": np.asarray([reported_score]),
        "states": np.asarray(states, dtype=np.str_),
    }
    if case["kind"] == "sets":
        payload["root_state_membership"] = np.asarray(
            [[state in tree.get_attribute(node, state_key) for state in states] for node in nodes],
            dtype=np.bool_)
    if matrix is not None:
        payload.update(row_states=np.asarray(matrix.index, dtype=np.str_),
                       column_states=np.asarray(matrix.columns, dtype=np.str_),
                       transitions=matrix.to_numpy(dtype=np.float64))
    return payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    inputs = json.loads(Path(args.inputs).read_text(encoding="utf-8"))
    output = Path(args.out)
    output.mkdir(parents=True, exist_ok=True)
    for case in inputs["cases"]:
        payload = produce_case(case, inputs["trees"][case["tree"]], inputs["meta_item"])
        np.savez(output / (case["id"] + ".npz"), **payload)
        print(f"{case['selector']} / {case['id']}: wrote scientific {case['kind']} payload", flush=True)


if __name__ == "__main__":
    main()
