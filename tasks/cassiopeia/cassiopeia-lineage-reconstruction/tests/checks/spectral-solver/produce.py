#!/usr/bin/env python3
"""在官方 spectral_test 的固定输入上调用真实 API。

只导出官方 API 真正返回的三样东西：
  * dissimilarity_functions.hamming_similarity_without_missing(...) 的成对相似度
  * graph_utilities.construct_similarity_graph(...) 返回的图（节点与全部带权边）
  * graph_utilities.spectral_improve_cut(G, cut) 返回的那一侧划分

SpectralSolver.perform_split / solve 的 partition 与拓扑不导出：它依赖
sp.linalg.eig 的返回列序（SpectralSolver.py:99-128），是人工 scope 排除项。
"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path

import networkx as nx
import pandas as pd
from cassiopeia.solver import dissimilarity_functions, graph_utilities

SIMILARITY = {'hamming_similarity_without_missing':
              dissimilarity_functions.hamming_similarity_without_missing}


def frame(spec):
    """按 IC 里显式声明的 order 建表——drop_duplicates 保留首次出现的那一行，
    所以行顺序决定重复行里哪个**名字**留下，不能交给 dict 的键序去定。"""
    return pd.DataFrame.from_dict({cell: list(spec['rows'][cell]) for cell in spec['order']},
                                  orient='index', columns=list(spec['columns']))


def weight_table(spec):
    return {int(character): {int(state): value for state, value in states.items()}
            for character, states in spec.items()}


def build_graph(spec):
    graph = nx.Graph()
    for node in spec['nodes']:
        graph.add_node(node)
    for u, v, weight in spec['edges']:
        graph.add_edge(u, v, weight=weight)
    return graph


def produce(inputs):
    missing = inputs['missing_state_indicator']
    threshold = inputs['threshold']
    similarity_function = SIMILARITY[inputs['similarity_function']]
    tables = {name: weight_table(spec) for name, spec in inputs['weight_tables'].items()}
    similarity_rows, graph_rows, cut_rows = [], [], []
    for config in inputs['configs']:
        kind = config['kind']
        if kind in ('similarity', 'graph'):
            matrix = frame(inputs['character_matrices'][config['matrix']])
            weights = tables[config['weights']] if config['weights'] else None
        if kind == 'similarity':
            names = [str(name) for name in matrix.index]
            values = matrix.to_numpy()
            for i, j in itertools.combinations(range(len(names)), 2):
                a, b = sorted((names[i], names[j]))
                similarity_rows.append({'config': config['id'], 'cell_i': a, 'cell_j': b,
                                        'value': float(similarity_function(
                                            list(values[i]), list(values[j]), missing, weights))})
        elif kind == 'graph':
            unique = matrix.drop_duplicates()
            graph = graph_utilities.construct_similarity_graph(
                unique, missing, unique.index, similarity_function=similarity_function,
                threshold=threshold, weights=weights)
            graph_rows.append({
                'config': config['id'],
                'nodes': sorted(str(node) for node in graph.nodes),
                'edges': sorted([sorted((str(u), str(v))) + [float(data['weight'])]
                                 for u, v, data in graph.edges(data=True)])})
        else:
            side = graph_utilities.spectral_improve_cut(
                build_graph(inputs['graphs'][config['graph']]), list(config['initial_cut']))
            cut_rows.append({'config': config['id'], 'side': [int(node) for node in side]})
    return {'schema_version': 1, 'similarity': similarity_rows,
            'graph': graph_rows, 'improved_cut': cut_rows}


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
    print('wrote pairwise similarity, similarity graphs and the improved cut')


if __name__ == '__main__':
    main()
