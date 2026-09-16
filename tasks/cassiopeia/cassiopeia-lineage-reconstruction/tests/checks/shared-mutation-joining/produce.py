#!/usr/bin/env python3
"""在官方sharedmutationjoiner_test的固定输入上调用真实API；不导出与tie-break相关的拓扑。"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path
import numba
import numpy as np
import pandas as pd
import scipy.spatial.distance
from cassiopeia.data import utilities as data_utilities
from cassiopeia.solver import dissimilarity_functions, solver_utilities
from cassiopeia.solver.SharedMutationJoiningSolver import SharedMutationJoiningSolver


def frame(spec):
    return pd.DataFrame.from_dict({cell: list(row) for cell, row in spec['rows'].items()},
                                  orient='index', columns=list(spec['columns']))


def similarity_frame(spec):
    names = sorted({cell for a, row in spec.items() for cell in (a, *row)})
    values = {a: [0.0 if a == b else float(spec.get(a, {}).get(b, spec.get(b, {}).get(a)))
                  for b in names] for a in names}
    return pd.DataFrame.from_dict(values, orient='index', columns=names)


def numba_weights(weights):
    table = numba.typed.Dict.empty(numba.types.int64,
                                   numba.types.DictType(numba.types.int64, numba.types.float64))
    if weights:
        for character, states in weights.items():
            entry = numba.typed.Dict.empty(numba.types.int64, numba.types.float64)
            for state, value in states.items():
                entry[state] = value
            table[character] = entry
    return table


def produce(inputs):
    missing = inputs['missing_state_indicator']
    priors = {int(c): {int(s): float(p) for s, p in v.items()} for c, v in inputs['priors'].items()}
    weights = solver_utilities.transform_priors(priors, inputs['prior_transformation'])
    solver = SharedMutationJoiningSolver(
        similarity_function=dissimilarity_functions.hamming_similarity_without_missing)

    similarity_rows = []
    for config in inputs['similarity_configs']:
        matrix = frame(inputs['character_matrices'][config['matrix']])
        applied = weights if config['use_priors'] else None
        # SMJ.solve 内部走的就是这个入口；它不把相似度矩阵写回 tree，所以直接调它。
        condensed = data_utilities.compute_dissimilarity_map(
            matrix.to_numpy(), matrix.shape[0],
            dissimilarity_functions.hamming_similarity_without_missing, applied, missing)
        square = pd.DataFrame(scipy.spatial.distance.squareform(condensed),
                              index=matrix.index, columns=matrix.index)
        for a, b in itertools.combinations(sorted(matrix.index), 2):
            similarity_rows.append({'config': config['id'], 'cell_i': str(a), 'cell_j': str(b),
                                    'value': float(square.loc[a, b])})

    maximal = []
    for probe in inputs['maximal_pair_probes']:
        square = similarity_frame(inputs['explicit_similarity_maps'][probe['similarity_map']])
        names = [str(x) for x in square.index]
        values = {tuple(sorted((a, b))): float(square.loc[a, b])
                  for a, b in itertools.combinations(names, 2)}
        best = max(values.values())
        maximal.append({'probe': probe['id'], 'max_similarity': best,
                        'pairs': sorted([list(pair) for pair, value in values.items() if value == best])})

    lca_rows = []
    for probe in inputs['lca_probes']:
        matrix = frame(inputs['character_matrices'][probe['matrix']])
        states = data_utilities.get_lca_characters(
            [list(matrix.loc[probe['cell_i']]), list(matrix.loc[probe['cell_j']])], missing)
        lca_rows.append({'probe': probe['id'], 'cell_i': probe['cell_i'], 'cell_j': probe['cell_j'],
                         'states': [int(x) for x in states]})

    updates = []
    for probe in inputs['update_probes']:
        matrix = frame(inputs['character_matrices'][probe['matrix']])
        square = similarity_frame(inputs['explicit_similarity_maps'][probe['similarity_map']]).astype(float)
        applied = numba_weights(weights if probe['use_priors'] else None)
        cherry = (probe['cherry'][0], probe['cherry'][1])
        updated = solver.update_similarity_map_and_character_matrix(
            matrix, solver.nb_similarity_function, square, cherry, probe['new_node'], missing, applied)
        names = sorted(str(x) for x in updated.index)
        updates.append({'probe': probe['id'], 'cherry': sorted(cherry),
                        'new_node_states': [int(x) for x in matrix.loc[probe['new_node']]],
                        'map': [{'node_i': a, 'node_j': b, 'value': float(updated.loc[a, b])}
                                for a, b in itertools.combinations(names, 2)]})

    return {'schema_version': 1, 'similarity': similarity_rows, 'maximal_pairs': maximal,
            'lca': lca_rows, 'update_steps': updates}


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
    print('wrote complete similarity, maximal-pair, LCA and merge-update tables')


if __name__ == '__main__':
    main()
