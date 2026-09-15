#!/usr/bin/env python3
"""在官方setUp的固定输入上跑NeighborJoiningSolver；只导出标签无关的科学观测。"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path
import networkx as nx
import pandas as pd
import cassiopeia as cas


def delta_fn(x, y, missing_state, priors):
    """与官方 test 文件里的字面实现一致；显式循环，供 numba nopython 编译。"""
    d = 0
    for i in range(len(x)):
        if x[i] != y[i]:
            d += 1
    return d


def matrix_frame(matrix, characters):
    return pd.DataFrame.from_dict({cell: list(states) for cell, states in matrix.items()},
                                  orient='index', columns=list(characters))


def explicit_frame(table):
    cells = sorted({cell for a, row in table.items() for cell in (a, *row)})
    values = {a: [0.0 if a == b else float(table.get(a, {}).get(b, table.get(b, {}).get(a)))
                  for b in cells] for a in cells}
    return pd.DataFrame.from_dict(values, orient='index', columns=cells)


def structure(triplet, topology):
    a, b, c = triplet
    A, B, C = (set(nx.ancestors(topology, x)) for x in (a, b, c))
    ab, ac, bc = len(A & B), len(A & C), len(B & C)
    if ab > bc and ab > ac:
        return 'ab'
    if ac > bc and ac > ab:
        return 'ac'
    if bc > ab and bc > ac:
        return 'bc'
    return '-'


def build(config, inputs, priors):
    kwargs = {}
    source = config['source']
    if config['matrix'] is not None:
        kwargs['character_matrix'] = matrix_frame(inputs['character_matrices'][config['matrix']],
                                                  inputs['characters'])
    if source.startswith('explicit:'):
        table = inputs['explicit_dissimilarity_maps'][source.split(':', 1)[1]]
        kwargs['dissimilarity_map'] = explicit_frame(table)
    if config['use_priors']:
        kwargs['priors'] = priors
    if not config['implicit_root']:
        # 官方给了显式根样本，因此不追加隐式全零根。
        kwargs['root_sample_name'] = config['root_sample']
    if source == 'hamming':
        function = delta_fn
    else:
        function = cas.solver.dissimilarity.weighted_hamming_distance
    tree = cas.data.CassiopeiaTree(**kwargs)
    solver = cas.solver.NeighborJoiningSolver(dissimilarity_function=function, add_root=True)
    solver.solve(tree)
    return tree


def produce(inputs):
    priors = {int(c): {int(s): float(p) for s, p in v.items()} for c, v in inputs['priors'].items()}
    dissimilarity, triplets, paths = [], [], []
    for config in inputs['configs']:
        name = config['id']
        tree = build(config, inputs, priors)
        computed = tree.get_dissimilarity_map()
        cells = sorted(str(x) for x in computed.index)
        for a, b in itertools.combinations(cells, 2):
            dissimilarity.append({'config': name, 'cell_i': a, 'cell_j': b,
                                  'value': float(computed.loc[a, b])})
        if not config['topology_graded']:
            # 该配置的合并存在并列，拓扑不是唯一确定的，故不交付拓扑观测。
            continue
        topology = tree.get_tree_topology()
        leaves = sorted(tree.leaves)
        for triplet in itertools.combinations(leaves, 3):
            triplets.append({'config': name, 'triplet': list(triplet),
                             'structure': structure(triplet, topology)})
        undirected = topology.to_undirected()
        for a, b in itertools.combinations(leaves, 2):
            paths.append({'config': name, 'leaf_i': a, 'leaf_j': b,
                          'length': int(nx.shortest_path_length(undirected, a, b))})
    solver = cas.solver.NeighborJoiningSolver(add_root=True)
    delta = explicit_frame(inputs['explicit_dissimilarity_maps'][inputs['q_source']])
    q = solver.compute_q(delta.values)
    names = [str(x) for x in delta.index]
    q_rows = [{'cell_i': names[i], 'cell_j': names[j], 'value': float(q[i][j])}
              for i in range(len(names)) for j in range(i + 1, len(names))]
    steps = []
    delta = explicit_frame(inputs['explicit_dissimilarity_maps'][inputs['cherry_steps']['source']])
    for name in inputs['cherry_steps']['names']:
        cherry = solver.find_cherry(delta.values)
        node_i, node_j = str(delta.index[cherry[0]]), str(delta.index[cherry[1]])
        delta = solver.update_dissimilarity_map(delta, (node_i, node_j), name)
        remaining = sorted(str(x) for x in delta.index)
        steps.append({'step': name, 'cherry': sorted([node_i, node_j]),
                      'map': [{'node_i': a, 'node_j': b, 'value': float(delta.loc[a, b])}
                              for a, b in itertools.combinations(remaining, 2)]})
    return {'schema_version': 1, 'dissimilarity': dissimilarity, 'q_criterion': q_rows,
            'cherry_steps': steps, 'topology_triplets': triplets, 'topology_paths': paths}


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
    print('wrote complete dissimilarity, Q, cherry-step and label-free topology tables')


if __name__ == '__main__':
    main()
