#!/usr/bin/env python3
"""在官方vanillagreedy_test的固定输入上跑VanillaGreedySolver；只导出标签无关的科学观测。"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path
import networkx as nx
import pandas as pd
import cassiopeia as cas
from cassiopeia.solver import missing_data_methods, solver_utilities
from cassiopeia.solver.VanillaGreedySolver import VanillaGreedySolver


def decode(state):
    # IC 用 {"ambiguous": [...]} 显式编码官方 fixture 里的 tuple 状态。
    return tuple(state['ambiguous']) if isinstance(state, dict) else state


def frame(spec):
    return pd.DataFrame.from_dict({cell: [decode(s) for s in row] for cell, row in spec['rows'].items()},
                                  orient='index', columns=list(spec['columns']))


def unique_frame(matrix):
    keep = (matrix.apply(lambda column: [set(s) if isinstance(s, tuple) else {s} for s in column.values], axis=0)
            .apply(tuple, axis=1).drop_duplicates().index.values)
    return matrix.loc[keep].copy()


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


def produce(inputs):
    missing = inputs['missing_state_indicator']
    priors = {int(c): {int(s): float(p) for s, p in v.items()} for c, v in inputs['priors'].items()}
    weights = solver_utilities.transform_priors(priors, inputs['prior_transformation'])
    fixtures = inputs['fixtures']
    solver = VanillaGreedySolver()

    frequencies = []
    for probe in inputs['frequency_probes']:
        matrix = unique_frame(frame(fixtures[probe['fixture']]))
        table = solver.compute_mutation_frequencies(list(probe['samples']), matrix, missing)
        for character in sorted(table):
            for state in sorted(table[character]):
                frequencies.append({'probe': probe['id'], 'character': int(character),
                                    'state': int(state), 'count': int(table[character][state])})

    assignments = []
    for probe in inputs['missing_probes']:
        matrix = frame(fixtures[probe['fixture']])
        left, right = missing_data_methods.assign_missing_average(
            matrix, missing, list(probe['left']), list(probe['right']), list(probe['missing']),
            weights if probe['use_priors'] else None)
        assignments.append({'probe': probe['id'], 'left': [str(x) for x in left],
                            'right': [str(x) for x in right]})

    splits, structures = [], []
    for probe in inputs['solve_probes']:
        spec = fixtures[probe['fixture']]
        matrix = frame(spec)
        unique = unique_frame(matrix)
        applied = weights if probe['use_priors'] else None
        left, right = solver.perform_split(unique, list(unique.index), applied, missing)
        splits.append({'probe': probe['id'], 'left': [str(x) for x in left],
                       'right': [str(x) for x in right]})
        if not probe['topology_graded']:
            # 该 fixture 的贪心分裂存在并列，拓扑不是唯一确定的，故不交付拓扑观测。
            continue
        tree = cas.data.CassiopeiaTree(matrix, missing_state_indicator=missing,
                                       priors=priors if probe['use_priors'] else None)
        VanillaGreedySolver().solve(tree, collapse_mutationless_edges=True)
        topology = tree.get_tree_topology()
        leaves = sorted(spec['rows'])
        for triplet in itertools.combinations(leaves, 3):
            structures.append({'probe': probe['id'], 'triplet': list(triplet),
                               'structure': structure(triplet, topology)})

    return {'schema_version': 1, 'frequencies': frequencies, 'missing_assignment': assignments,
            'splits': splits, 'topology_triplets': structures}


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
    print('wrote complete frequency, missing-assignment, split and label-free topology tables')


if __name__ == '__main__':
    main()
