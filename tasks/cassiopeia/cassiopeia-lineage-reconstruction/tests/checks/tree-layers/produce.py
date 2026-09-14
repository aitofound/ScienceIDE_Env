#!/usr/bin/env python3
"""在官方layers_test的固定输入上执行官方操作序列；只导出Layers的隔离与传播语义。"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import networkx as nx
import pandas as pd
import cassiopeia as cas


def frame(spec):
    return pd.DataFrame.from_dict({cell: list(row) for cell, row in spec['rows'].items()},
                                  orient='index', columns=list(spec['columns']))


def fresh_tree(inputs):
    topology = nx.DiGraph()
    topology.add_nodes_from(inputs['topology']['nodes'])
    topology.add_edges_from((a, b) for a, b in inputs['topology']['edges'])
    return cas.data.CassiopeiaTree(
        character_matrix=frame(inputs['matrices'][inputs['base_matrix']]), tree=topology)


def read_table(table):
    return {'columns': [str(c) for c in table.columns],
            'rows': {str(cell): [int(v) for v in table.loc[cell]] for cell in table.index}}


def snapshot(tree, cells):
    # Layers 继承 dict 却把数据放在 self._data，只覆盖了 __iter__/__len__/__contains__/__getitem__，
    # 所以枚举层名必须走 __iter__，不能用 dict 继承来的 keys()。
    return {'character_matrix': read_table(tree.character_matrix),
            'layers': {name: read_table(tree.layers[name]) for name in iter(tree.layers)},
            'character_states': {cell: [int(v) for v in tree.get_character_states(cell)] for cell in cells}}


def produce(inputs):
    cells = sorted(inputs['matrices'][inputs['base_matrix']]['rows'])
    tree = fresh_tree(inputs)
    steps = []
    for entry in inputs['program']:
        operation = entry['operation']
        if operation == 'start':
            pass
        elif operation == 'set_layer':
            tree.layers[entry['layer']] = frame(inputs['matrices'][entry['matrix']])
        elif operation == 'set_character_states':
            tree.set_character_states(entry['cell'], list(entry['states']), layer=entry['layer'])
        elif operation == 'solve_on_layer':
            tree = fresh_tree(inputs)
            tree.layers[entry['layer']] = frame(inputs['matrices'][entry['matrix']])
            cas.solver.VanillaGreedySolver().solve(
                tree, collapse_mutationless_edges=True, layer=entry['layer'])
        else:
            raise ValueError('未知操作')
        steps.append({'step': entry['step'], **snapshot(tree, cells)})

    probe = inputs['layer_probe']
    container_tree = fresh_tree(inputs)
    container_tree.layers[probe['layer']] = frame(inputs['matrices'][probe['matrix']])
    container = {'iterated': [str(name) for name in iter(container_tree.layers)],
                 'length': int(len(container_tree.layers)),
                 'contains': {probe['layer']: bool(probe['layer'] in container_tree.layers),
                              probe['absent_layer']: bool(probe['absent_layer'] in container_tree.layers)}}

    outcomes = {}
    for entry in inputs['outcome_probes']:
        candidate = fresh_tree(inputs)
        try:
            candidate.layers[inputs['layer_probe']['layer']] = frame(inputs['matrices'][entry['matrix']])
            outcomes[entry['id']] = 'accepted'
        except Exception as exc:
            outcomes[entry['id']] = f'{type(exc).__module__}.{type(exc).__qualname__}'

    wide_probe = inputs['wide_probe']
    wide_tree = fresh_tree(inputs)
    wide_tree.layers[wide_probe['layer']] = frame(inputs['matrices'][wide_probe['matrix']])
    before = {cell: [int(v) for v in wide_tree.get_character_states(cell)] for cell in cells}
    wide_tree.set_character_states_at_leaves(layer=wide_probe['layer'])
    after = {cell: [int(v) for v in wide_tree.get_character_states(cell)] for cell in cells}
    wide = {'rows': {str(cell): [int(v) for v in wide_tree.layers[wide_probe['layer']].loc[cell]]
                     for cell in cells},
            'states_before_propagation': before, 'states_after_propagation': after}

    return {'schema_version': 1, 'steps': steps, 'layer_container': container,
            'outcomes': outcomes, 'wide_layer': wide}


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
    print('wrote complete Layers isolation and propagation tables')


if __name__ == '__main__':
    main()
