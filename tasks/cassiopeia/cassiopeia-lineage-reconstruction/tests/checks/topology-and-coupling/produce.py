#!/usr/bin/env python3
"""直接调用官方 topology API 并导出返回值；不重算任何公式，不采集 assertions。"""
from __future__ import annotations

import argparse
import json
from numbers import Integral, Real
from pathlib import Path
import time

import networkx as nx
import numpy as np
import pandas as pd

import cassiopeia as cas
from cassiopeia.tools import topology


def build_tree(config):
    graph = nx.DiGraph()
    graph.add_edges_from([tuple(edge) for edge in config['tree']['edges']])
    matrix = config['character_matrix']
    frame = pd.DataFrame(matrix['states'], index=matrix['cells'])
    return cas.data.CassiopeiaTree(character_matrix=frame, tree=graph)


def frame_of(config, name):
    columns = config['matrices']['columns']
    rows = config['matrices'][name]
    return pd.DataFrame.from_dict(rows, orient='index', columns=columns)


def real(value, name):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, Real):
        raise TypeError(f'{name}: 生产返回不是实数')
    value = float(value)
    if not np.isfinite(value):
        raise ValueError(f'{name}: 生产返回非有限')
    return value


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

    started = time.perf_counter()
    ids, values = [], []
    for case in config['choose_cases']:
        result = topology.nCk(case['n'], case['k'])
        if isinstance(result, (bool, np.bool_)) or not isinstance(result, Real):
            raise TypeError('nCk 生产返回不是实数')
        if float(result) != int(result):
            raise ValueError('nCk 生产返回不是整数值')
        ids.append(case['id'])
        values.append(int(result))
    arrays['choose.ids'] = np.asarray(ids, dtype=str)
    arrays['choose.values'] = np.asarray(values, dtype=np.int64)

    ids, values = [], []
    for case in config['coalescent_cases']:
        ids.append(case['id'])
        values.append(real(topology.simple_coalescent_probability(
            case['n'], case['b'], case['k']), case['id']))
    arrays['coalescent.ids'] = np.asarray(ids, dtype=str)
    arrays['coalescent.values'] = np.asarray(values, dtype=np.float64)
    timings.append({'stage': 'closed_form', 'seconds': time.perf_counter() - started})

    for case in config['expansion_cases']:
        started = time.perf_counter()
        tree = build_tree(config)
        returned = cas.tl.compute_expansion_pvalues(
            tree, min_clade_size=case['min_clade_size'], min_depth=case['min_depth'],
            copy=case['copy'])
        target = returned if case['copy'] else tree
        if case['copy'] and target is None:
            raise ValueError('copy=True 必须返回一棵新树')
        if not case['copy'] and returned is not None:
            raise ValueError('copy=False 必须原地写入并返回 None')
        nodes = list(target.depth_first_traverse_nodes(postorder=False))
        if len(set(nodes)) != len(nodes):
            raise ValueError('生产遍历给出重复节点身份')
        pvalues = [real(target.get_attribute(node, 'expansion_pvalue'), node)
                   for node in nodes]
        arrays[f"expansion.{case['id']}.nodes"] = np.asarray(nodes, dtype=str)
        arrays[f"expansion.{case['id']}.pvalues"] = np.asarray(pvalues, dtype=np.float64)
        timings.append({'stage': f"expansion/{case['id']}",
                        'seconds': time.perf_counter() - started})

    ids, correlations, significances = [], [], []
    for case in config['cophenetic_cases']:
        started = time.perf_counter()
        kwargs = {}
        if case['dissimilarity_map'] is not None:
            kwargs['dissimilarity_map'] = frame_of(config, case['dissimilarity_map'])
        if case['weights'] is not None:
            kwargs['weights'] = frame_of(config, case['weights'])
        result = cas.tl.compute_cophenetic_correlation(build_tree(config), **kwargs)
        members = list(result)
        if len(members) != 2:
            raise ValueError('cophenetic 生产返回不是二元组')
        ids.append(case['id'])
        correlations.append(real(members[0], case['id'] + '/correlation'))
        significances.append(real(members[1], case['id'] + '/significance'))
        timings.append({'stage': f"cophenetic/{case['id']}",
                        'seconds': time.perf_counter() - started})
    arrays['cophenetic.ids'] = np.asarray(ids, dtype=str)
    arrays['cophenetic.correlation'] = np.asarray(correlations, dtype=np.float64)
    arrays['cophenetic.significance'] = np.asarray(significances, dtype=np.float64)

    ids, kinds = [], []
    for case in config['error_cases']:
        ids.append(case['id'])
        kinds.append(observe_error(config, case))
    arrays['errors.ids'] = np.asarray(ids, dtype=str)
    arrays['errors.exception'] = np.asarray(kinds, dtype=str)
    # 声明这份产物出自哪个 IC：nominal 与 variant 的受判值不同（两张浮点矩阵差 2 ULP），
    # 判分器的独立复算必须用同一个 IC，否则第三条腿会把合法的 variant 判成错。
    # 它不是受判量；两侧必须声明同一个，否则是合同失败。
    arrays['ic.name'] = np.asarray([config['ic_name']], dtype=str)

    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(
        json.dumps({'ungraded': True, 'timings': timings}, indent=2) + '\n')
    print(f"已执行 {len(config['choose_cases']) + len(config['coalescent_cases'])} 个闭式调用、"
          f"{len(config['expansion_cases'])} 个 expansion 配置、"
          f"{len(config['cophenetic_cases'])} 个 cophenetic 配置、"
          f"{len(config['error_cases'])} 个异常场景")


def observe_error(config, case):
    """记录生产在非法配置下抛出的异常类型名；不抛出时记 'no-exception'。"""
    try:
        if case['api'] == 'nCk':
            topology.nCk(*case['args'])
        elif case['api'] == 'simple_coalescent_probability':
            topology.simple_coalescent_probability(*case['args'])
        elif case['api'] == 'get_attribute_after_copy':
            tree = build_tree(config)
            cas.tl.compute_expansion_pvalues(tree, min_clade_size=2, min_depth=1, copy=True)
            # 原树必须仍然干净：copy=True 不得写回调用方的树。
            tree.get_attribute(list(tree.nodes)[0], 'expansion_pvalue')
        else:
            raise ValueError('未知异常场景')
    except Exception as exc:  # noqa: BLE001
        return type(exc).__name__
    return 'no-exception'


if __name__ == '__main__':
    main()
