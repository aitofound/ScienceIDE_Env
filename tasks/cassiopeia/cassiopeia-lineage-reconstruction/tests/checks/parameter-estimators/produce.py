#!/usr/bin/env python3
"""按官方方法生命周期调用真实估计 API，仅命名和导出返回值，不重算估计公式。"""
from __future__ import annotations

import argparse
import json
import math
from numbers import Real
from pathlib import Path
import time

import numpy as np

RETURN_NAMES = {
    'get_proportion_of_mutation': ['mutated_proportion'],
    'get_proportion_of_missing_data': ['missing_proportion'],
    'estimate_mutation_rate': ['mutation_rate'],
    'estimate_missing_data_rates': ['stochastic_missing_probability', 'heritable_missing_rate'],
}


def build_trees(specifications):
    import networkx as nx
    import pandas as pd
    import cassiopeia as cas

    trees = {}
    for name, specification in specifications.items():
        graph = nx.DiGraph()
        graph.add_edges_from(specification['edges'])
        matrix = specification['character_matrix']
        frame = pd.DataFrame(matrix['states'], index=matrix['cells'], columns=matrix['characters'])
        priors = {int(c): {int(s): p for s, p in values.items()} for c, values in specification['priors'].items()}
        tree = cas.data.CassiopeiaTree(tree=graph, character_matrix=frame, priors=priors,
                                     missing_state_indicator=specification['missing_state_indicator'])
        for parent, child, length in specification['branch_length_updates']:
            tree.set_branch_length(parent, child, length)
        trees[name] = tree
    return trees


def collect(inputs, tree_factory, apis):
    call_ids, observables, values = [], [], []
    diagnostics = {'ungraded': True, 'calls': [], 'fixture_construction': []}
    for method in inputs['methods']:
        start = time.perf_counter()
        trees = tree_factory(inputs['trees'])
        diagnostics['fixture_construction'].append({'selector': method['selector'],
                                                    'seconds': time.perf_counter() - start})
        for call in method['calls']:
            for update in call['updates_before']:
                trees[update['tree']].parameters[update['parameter']] = update['value']
            tree = trees[call['tree']]
            names = RETURN_NAMES[call['api']]
            if names != call['observables']:
                raise ValueError('调用声明的科学分量与真实 API 返回约定不符')
            parameters_before = dict(tree.parameters)
            start = time.perf_counter()
            result = apis[call['api']](tree, **call['kwargs'])
            seconds = time.perf_counter() - start
            members = [result] if len(names) == 1 else list(result)
            if len(members) != len(names):
                raise ValueError('生产返回 tuple 不完整')
            for name, member in zip(names, members):
                if isinstance(member, (bool, np.bool_)) or not isinstance(member, Real) or not math.isfinite(float(member)):
                    raise ValueError('生产科学参数不是有限实数')
                call_ids.append(call['call_id'])
                observables.append(name)
                values.append(float(member))
            diagnostics['calls'].append({'call_id': call['call_id'], 'selector': method['selector'],
                                         'api': call['api'], 'tree': call['tree'],
                                         'parameters_before': parameters_before,
                                         'kwargs': call['kwargs'], 'elapsed_seconds': seconds})
    arrays = {'call_ids': np.asarray(call_ids, dtype=str), 'observables': np.asarray(observables, dtype=str),
              'values': np.asarray(values, dtype=np.float64)}
    return arrays, diagnostics


def main():
    from cassiopeia.tools import parameter_estimators

    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    inputs = json.loads(args.inputs.read_text())
    apis = {name: getattr(parameter_estimators, name) for name in RETURN_NAMES}
    arrays, diagnostics = collect(inputs, build_trees, apis)
    args.out.mkdir(parents=True, exist_ok=True)
    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(json.dumps(diagnostics, ensure_ascii=True, allow_nan=False, indent=2) + '\n')
    print(f'已执行 {len(diagnostics["calls"])} 次真实科学调用，导出 {len(arrays["values"])} 个完整返回分量；异常方法未评分')


if __name__ == '__main__':
    main()
