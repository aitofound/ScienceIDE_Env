#!/usr/bin/env python3
"""在官方setUp的五棵固定树上调用critique的三个确定性API；不采样triplet，不引入RNG。"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path
import networkx as nx
import cassiopeia as cas
from cassiopeia.critique import critique_utilities


def build(edges):
    graph = nx.DiGraph()
    graph.add_edges_from((str(a), str(b)) for a, b in edges)
    return cas.data.CassiopeiaTree(tree=graph)


def produce(inputs):
    fixtures = inputs['fixtures']
    outgroups, depths = [], []
    for name in sorted(fixtures):
        edges = fixtures[name]
        # annotate_tree_depths 会就地写属性，所以深度与outgroup各用一棵新树，保持原调用形态。
        annotated = build(edges)
        critique_utilities.annotate_tree_depths(annotated)
        for node in sorted(annotated.nodes):
            depths.append({'tree': name, 'node': node,
                           'depth': int(annotated.get_attribute(node, 'depth')),
                           'number_of_triplets': int(annotated.get_attribute(node, 'number_of_triplets'))})
        queried = build(edges)
        for triplet in itertools.combinations(sorted(queried.leaves), 3):
            outgroups.append({'tree': name, 'triplet': list(triplet),
                              'outgroup': str(critique_utilities.get_outgroup(queried, triplet))})
    robinson_foulds = []
    for first, second in inputs['robinson_foulds_pairs']:
        rf, rf_max = cas.critique.robinson_foulds(build(fixtures[first]), build(fixtures[second]))
        robinson_foulds.append({'pair': f'{first}|{second}', 'rf': float(rf), 'rf_max': float(rf_max)})
    return {'schema_version': 1, 'outgroups': outgroups, 'node_depths': depths,
            'robinson_foulds': robinson_foulds}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'results.json').exists():
        raise ValueError('拒绝覆盖旧结果')
    inputs = json.loads(Path(args.inputs).read_text(encoding='utf-8'))
    wire = json.dumps(produce(inputs), ensure_ascii=True, allow_nan=False, indent=2).encode('utf-8')
    (out / 'results.json').write_bytes(wire + b'\n')
    print('wrote complete outgroup, node-depth and Robinson-Foulds tables')


if __name__ == '__main__':
    main()
