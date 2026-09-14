#!/usr/bin/env python3
"""在官方 greedy_variants_test 的固定输入上调用真实 API。

导出三样官方 API 真正返回的东西：
  * SpectralGreedySolver / MaxCutGreedySolver.perform_split(...) 的 left/right
  * solve(tree, collapse_mutationless_edges=True) 之后 get_tree_topology() 的三元组结构
  * 在声明输入上 solve() 抛出的异常类型（None 表示未抛）

producer 只导出官方 API 的返回值，不做任何守卫式的合理性判断——
输出值是否合理是判分的事，拦在这里会把判别力从 validator 手里拿走。
"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path

import networkx as nx
import pandas as pd
import cassiopeia as cas
from cassiopeia.solver.SpectralGreedySolver import SpectralGreedySolver
from cassiopeia.solver.MaxCutGreedySolver import MaxCutGreedySolver
from cassiopeia.solver import solver_utilities

SOLVERS = {'spectral': SpectralGreedySolver, 'maxcut': MaxCutGreedySolver}


def cell_value(state):
    """ambiguous state 在 JSON 里是列表，官方 API 要的是元组。"""
    return tuple(state) if isinstance(state, list) else state


def frame(spec):
    return pd.DataFrame.from_dict(
        {cell: [cell_value(s) for s in spec['rows'][cell]] for cell in spec['order']},
        orient='index', columns=list(spec['columns']))


def produce(inputs):
    missing = inputs['missing_state_indicator']
    priors = {name: {int(c): {int(s): float(p) for s, p in table.items()}
                     for c, table in spec.items()}
              for name, spec in inputs['prior_tables'].items()}
    document = {'schema_version': 1, 'split': [], 'topology': [], 'error_behaviour': []}

    for config in inputs['configs']:
        spec = inputs['character_matrices'][config['matrix']]
        raw = priors[config['priors']] if config['priors'] else None
        weights = (solver_utilities.transform_priors(raw, 'negative_log') if raw else None)
        matrix = frame(spec)
        unique = matrix.drop_duplicates()
        left, right = SOLVERS[config['solver']]().perform_split(
            unique, unique.index, weights=weights, missing_state_indicator=missing)
        document['split'].append({'config': config['id'],
                                  'left': [str(x) for x in left],
                                  'right': [str(x) for x in right]})
        tree = cas.data.CassiopeiaTree(matrix, missing_state_indicator=missing, priors=raw)
        SOLVERS[config['solver']]().solve(tree, collapse_mutationless_edges=True)
        topology = tree.get_tree_topology()
        leaves = sorted(str(x) for x in spec['order'])
        ancestors = {leaf: set(nx.ancestors(topology, leaf)) for leaf in leaves}
        triplets = {}
        for a, b, c in itertools.combinations(leaves, 3):
            ab = len(ancestors[a] & ancestors[b])
            ac = len(ancestors[a] & ancestors[c])
            bc = len(ancestors[b] & ancestors[c])
            triplets['|'.join((a, b, c))] = ('ab' if ab > bc and ab > ac else
                                             'ac' if ac > bc and ac > ab else
                                             'bc' if bc > ab and bc > ac else '-')
        document['topology'].append({'config': config['id'], 'triplets': triplets})

    for probe in inputs['error_probes']:
        spec = inputs['character_matrices'][probe['matrix']]
        raw = priors[probe['priors']] if probe['priors'] else None
        tree = cas.data.CassiopeiaTree(frame(spec), missing_state_indicator=missing, priors=raw)
        try:
            SOLVERS[probe['solver']]().solve(tree)
            raised = None
        except Exception as exc:                      # noqa: BLE001 —— 记录类型本身就是产物
            raised = f'{type(exc).__module__}.{type(exc).__qualname__}'
        document['error_behaviour'].append({'probe': probe['id'], 'raised': raised})
    return document


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
    print('wrote greedy-variant splits, topologies and exception behaviour')


if __name__ == '__main__':
    main()
