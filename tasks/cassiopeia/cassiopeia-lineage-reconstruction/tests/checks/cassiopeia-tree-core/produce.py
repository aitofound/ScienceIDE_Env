#!/usr/bin/env python3
"""直接调用官方 CassiopeiaTree 的结构 API 并导出返回值；不重算任何规则。

所有集合类返回在导出前按身份排序——storage order 既不受判也不作位置键
（SPEC.html:127）。有序的只有 `get_all_ancestors`，它返回的是父链，
次序由树本身决定而不是存放顺序。
"""
from __future__ import annotations

import argparse
import json
from numbers import Integral
from pathlib import Path
import time

import networkx as nx
import numpy as np
import pandas as pd

import cassiopeia as cas

RESOLVERS = {'first_state': (lambda state: state[0])}


def whole(value, context):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, Integral):
        raise TypeError(f'{context}: 生产返回不是整数')
    return int(value)


def tree_with_matrix(config, name):
    graph = nx.DiGraph()
    graph.add_edges_from([tuple(edge) for edge in config['tree']['edges']])
    rows = config['matrices'][name]
    frame = pd.DataFrame.from_dict(
        {cell: [tuple(v) if isinstance(v, list) else v for v in states]
         for cell, states in rows.items()}, orient='index')
    return cas.data.CassiopeiaTree(character_matrix=frame, tree=graph,
                                   missing_state_indicator=config['missing_state_indicator'])


def bare_tree(case):
    graph = nx.DiGraph()
    graph.add_nodes_from(case['nodes'])
    graph.add_edges_from([tuple(edge) for edge in case['edges']])
    tree = cas.data.CassiopeiaTree(tree=graph)
    tree.set_all_character_states({k: list(v) for k, v in case['states'].items()})
    return tree


def ragged(rows):
    """把 {身份: 序列} 编码成 (ids, offsets, values) —— 序列内部次序保留。"""
    ids = sorted(rows)
    offsets, values = [0], []
    for key in ids:
        values.extend(rows[key])
        offsets.append(len(values))
    return ids, offsets, values


def pack_states(prefix, rows, arrays):
    """state 向量可能含 tuple（歧义态），所以再套一层 ragged。"""
    ids = sorted(rows)
    site_offsets, cell_offsets, values = [0], [0], []
    for key in ids:
        for state in rows[key]:
            items = state if isinstance(state, tuple) else (state,)
            values.extend(whole(x, prefix) for x in items)
            site_offsets.append(len(values))
        cell_offsets.append(len(site_offsets) - 1)
    arrays[prefix + '.ids'] = np.asarray(ids, dtype=str)
    arrays[prefix + '.cell_offsets'] = np.asarray(cell_offsets, dtype=np.int64)
    arrays[prefix + '.site_offsets'] = np.asarray(site_offsets, dtype=np.int64)
    arrays[prefix + '.values'] = np.asarray(values, dtype=np.int64)


def pack_pairs(prefix, rows, arrays):
    ids, offsets, values = ragged(rows)
    arrays[prefix + '.ids'] = np.asarray(ids, dtype=str)
    arrays[prefix + '.offsets'] = np.asarray(offsets, dtype=np.int64)
    arrays[prefix + '.values'] = np.asarray(values, dtype=str)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.inputs.read_text(encoding='utf-8'))
    if config['schema_version'] != 1:
        raise ValueError('unsupported IC schema')
    args.out.mkdir(parents=True, exist_ok=True)
    arrays, timings = {}, []

    for case in config['structure_cases']:
        started = time.perf_counter()
        tree = tree_with_matrix(config, case['matrix'])
        nodes = sorted(tree.nodes)
        arrays[f"structure.{case['id']}.nodes"] = np.asarray(nodes, dtype=str)
        for name, fn in (('is_leaf', tree.is_leaf), ('is_root', tree.is_root),
                         ('is_internal', tree.is_internal_node)):
            flags = [fn(node) for node in nodes]
            if any(not isinstance(f, (bool, np.bool_)) for f in flags):
                raise TypeError(f'{name} 生产返回不是布尔')
            arrays[f"structure.{case['id']}.{name}"] = np.asarray(
                [int(bool(f)) for f in flags], dtype=np.int8)
        pack_pairs(f"structure.{case['id']}.children",
                   {n: sorted(tree.children(n)) for n in nodes}, arrays)
        pack_pairs(f"structure.{case['id']}.leaves_in_subtree",
                   {n: sorted(tree.leaves_in_subtree(n)) for n in nodes}, arrays)
        # subset_clade(node, copy=True) 返回一棵新树；受判的是它的 root / nodes / leaves，
        # 三者都按身份排序后导出（root 是单值）。
        clades = {}
        roots = []
        for node in nodes:
            subtree = tree.subset_clade(node, copy=True)
            if subtree is None:
                raise ValueError('subset_clade(copy=True) 必须返回一棵新树')
            roots.append(subtree.root)
            clades[node] = (['nodes'] + sorted(subtree.nodes)
                            + ['leaves'] + sorted(subtree.leaves))
        arrays[f"structure.{case['id']}.subset_clade_root"] = np.asarray(roots, dtype=str)
        pack_pairs(f"structure.{case['id']}.subset_clade", clades, arrays)
        timings.append({'stage': f"structure/{case['id']}",
                        'seconds': time.perf_counter() - started})

    for case in config['ancestral_cases']:
        started = time.perf_counter()
        tree = tree_with_matrix(config, case['matrix'])
        tree.reconstruct_ancestral_characters()
        pack_states(f"ancestral.{case['id']}.states",
                    {n: tuple(tree.get_character_states(n)) for n in tree.nodes}, arrays)
        mutations, unmutated = {}, {}
        for parent, child in tree.edges:
            key = parent + '|' + child
            mutations[key] = [f'{whole(site, "mut")}:{whole(state, "mut")}'
                              for site, state in tree.get_mutations_along_edge(parent, child)]
            unmutated[key] = [str(whole(site, 'unmut'))
                              for site in tree.get_unmutated_characters_along_edge(parent, child)]
        pack_pairs(f"ancestral.{case['id']}.mutations", mutations, arrays)
        pack_pairs(f"ancestral.{case['id']}.unmutated", unmutated, arrays)
        timings.append({'stage': f"ancestral/{case['id']}",
                        'seconds': time.perf_counter() - started})

    for case in config['lca_cases']:
        started = time.perf_counter()
        tree = tree_with_matrix(config, case['matrix'])
        nodes = sorted(tree.nodes)
        # get_all_ancestors 返回父链，次序由树决定而非存放顺序，所以保留原序。
        pack_pairs(f"lca.{case['id']}.ancestors",
                   {n: list(tree.get_all_ancestors(n)) for n in nodes}, arrays)
        pack_pairs(f"lca.{case['id']}.ancestors_inclusive",
                   {n: list(tree.get_all_ancestors(n, include_node=True)) for n in nodes},
                   arrays)
        pairs = [tuple(p) for p in case['pairs']]
        arrays[f"lca.{case['id']}.pair_a"] = np.asarray([a for a, _ in pairs], dtype=str)
        arrays[f"lca.{case['id']}.pair_b"] = np.asarray([b for _, b in pairs], dtype=str)
        arrays[f"lca.{case['id']}.find_lca"] = np.asarray(
            [tree.find_lca(a, b) for a, b in pairs], dtype=str)
        found = dict(tree.find_lcas_of_pairs(pairs=pairs))
        arrays[f"lca.{case['id']}.find_lcas_of_pairs"] = np.asarray(
            [found[pair] for pair in pairs], dtype=str)
        timings.append({'stage': f"lca/{case['id']}", 'seconds': time.perf_counter() - started})

    for case in config['ambiguity_cases']:
        started = time.perf_counter()
        tree = tree_with_matrix(config, case['matrix'])
        nodes = sorted(tree.nodes)
        arrays[f"ambiguity.{case['id']}.nodes"] = np.asarray(nodes, dtype=str)
        arrays[f"ambiguity.{case['id']}.is_ambiguous"] = np.asarray(
            [int(bool(tree.is_ambiguous(n))) for n in nodes], dtype=np.int8)
        collapsed = tree_with_matrix(config, case['matrix'])
        collapsed.collapse_ambiguous_characters()
        pack_states(f"ambiguity.{case['id']}.collapsed",
                    {n: tuple(collapsed.get_character_states(n)) for n in collapsed.leaves},
                    arrays)
        resolved = tree_with_matrix(config, case['matrix'])
        # 只走官方 test 实际传入的显式 resolver；默认 resolve_most_abundant 在并列时
        # 用未设种子的 np.random.choice，本 fixture 上八个字符有七个并列，不可评。
        resolved.resolve_ambiguous_characters(RESOLVERS[case['resolver']])
        pack_states(f"ambiguity.{case['id']}.resolved",
                    {n: tuple(resolved.get_character_states(n)) for n in resolved.leaves},
                    arrays)
        timings.append({'stage': f"ambiguity/{case['id']}",
                        'seconds': time.perf_counter() - started})

    for case in config['edge_mutation_cases']:
        tree = bare_tree(case)
        rows = {}
        for flag in case['treat_missing_as_mutations']:
            key = f'treat_missing_{int(bool(flag))}'
            observed = tree.get_mutations_along_edge(
                case['edges'][0][0], case['edges'][0][1], treat_missing_as_mutations=flag)
            rows[key] = [f'{whole(s, "edge")}:{whole(v, "edge")}' for s, v in observed]
        pack_pairs(f"edge_mutations.{case['id']}", rows, arrays)

    for case in config['imputation_cases']:
        tree = bare_tree(case)
        tree.impute_deducible_missing_states()
        pack_states(f"imputation.{case['id']}",
                    {n: tuple(tree.get_character_states(n)) for n in tree.nodes}, arrays)

    ids, kinds = [], []
    for case in config['error_cases']:
        ids.append(case['id'])
        kinds.append(observe_error(config, case))
    arrays['errors.ids'] = np.asarray(ids, dtype=str)
    arrays['errors.exception'] = np.asarray(kinds, dtype=str)

    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(
        json.dumps({'ungraded': True, 'timings': timings}, indent=2) + '\n')
    print(f'已导出 {len(arrays)} 个数组，覆盖 structure/ancestral/lca/ambiguity/'
          f'edge_mutations/imputation/errors 七段')


def observe_error(config, case):
    try:
        if case['api'] == 'get_mutations_along_edge':
            tree = tree_with_matrix(config, 'plain')
            tree.reconstruct_ancestral_characters()
            tree.get_mutations_along_edge(*case['args'])
        elif case['api'] == 'find_lca':
            tree_with_matrix(config, 'plain').find_lca(*case['args'])
        elif case['api'] == 'uninitialized':
            cas.data.CassiopeiaTree().children('anything')
        else:
            raise ValueError('未知异常场景')
    except Exception as exc:  # noqa: BLE001
        return type(exc).__name__
    return 'no-exception'


if __name__ == '__main__':
    main()
