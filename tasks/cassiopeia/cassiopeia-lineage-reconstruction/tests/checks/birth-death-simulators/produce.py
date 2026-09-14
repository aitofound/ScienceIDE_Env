"""产出 birth-death-simulators 的受判产物。

覆盖 birth_death_simulator_test.py 的全部 14 个 test（15 个异常用例 + 18 棵树）与
simple_fit_subclone_simulator_test.py 的 2 个 test。

**两种可复现性，区别对待。** BirthDeathFitnessSimulator 的 `random_seed=N` 是**构造
参数**，是 API 明写的复现承诺，所以那些用例的时刻逐值受判。SimpleFitSubcloneSimulator
的随机用例靠的是上游 test 里的 `np.random.seed(1)`——一个**全局** numpy 种子，不是这个
类的任何承诺；一个正确的移植可以合法地换个顺序消耗全局流。所以它只判结构，与上游自己
唯一的断言（内部边长两两互异）口径一致。

实测（2026-09-13）：20 个成功用例每个连跑 4 次，(nodes, edges, times) 摘要都唯一。
"""
from __future__ import annotations

import argparse
import json
import logging
import time
import warnings
from pathlib import Path

import networkx as nx
import numpy as np

from cassiopeia.data.CassiopeiaTree import CassiopeiaTree
from cassiopeia.simulator import SimpleFitSubcloneSimulator
from cassiopeia.simulator.BirthDeathFitnessSimulator import (
    BirthDeathFitnessSimulator,
)

# 上游把等待时间写成 lambda；JSON 存不下 lambda。这是一个**封闭词表**，不是通用求值器：
# 任何没列在这里的 kind 都直接报错，绝不 eval 输入里的字符串。
SCALED = 'exponential_scaled'


def distribution(spec, *, takes_scale):
    """把声明式的分布规格还原成上游那几个 lambda。"""
    if not isinstance(spec, dict):
        raise ValueError(f'分布规格必须是对象，得到 {type(spec).__name__}')
    kind = spec.get('kind')
    if kind == 'constant':
        value = spec['value']
        return (lambda _: value) if takes_scale else (lambda: value)
    if kind == SCALED:
        if not takes_scale:
            raise ValueError(f'{SCALED} 只用于 birth 侧')
        return lambda scale: np.random.exponential(scale)
    if kind == 'exponential':
        scale = spec['scale']
        return lambda: np.random.exponential(scale)
    if kind == 'bernoulli':
        p = spec['p']
        return lambda: 1 if np.random.uniform() < p else 0
    if kind == 'uniform':
        low, high = spec['low'], spec['high']
        return lambda: np.random.uniform(low, high)
    raise ValueError(f'未知的分布 kind={kind!r}；本 check 只认封闭词表里的那几种')


DIST_KEYS = {
    'birth_waiting_distribution': True,       # 值＝是否吃 scale 参数
    'death_waiting_distribution': False,
    'mutation_distribution': False,
    'fitness_distribution': False,
}


def build_simulator(kwargs, initial_tree=None):
    out = {}
    for key, value in kwargs.items():
        if key in DIST_KEYS:
            out[key] = distribution(value, takes_scale=DIST_KEYS[key])
        else:
            out[key] = value
    if initial_tree is not None:
        out['initial_tree'] = initial_tree
    return BirthDeathFitnessSimulator(**out)


def explicit_tree(spec):
    graph = nx.DiGraph()
    graph.add_edges_from([tuple(e) for e in spec['edges']])
    return CassiopeiaTree(tree=graph)


def observe(tree, *, want_times, want_birth_scale, seed_leaves=None):
    """按上游 extract_tree_statistics 的形状取观测量，再补完整拓扑。"""
    nodes = sorted(tree.nodes)
    times = tree.get_times()
    # 上游 extract_tree_statistics:39-49：收叶时刻、数叶、去掉 root 之上那个单分叉
    # 节点后要求出度只有 0 或 2
    out_degrees = [len(tree.children(n)) for n in tree.nodes][1:]
    record = {
        'nodes': np.asarray(nodes, dtype=str),
        'edges': np.asarray(sorted([list(e) for e in tree.edges]) or [['', '']],
                            dtype=str),
        'leaf_count': np.asarray(sum(1 for n in tree.nodes if tree.is_leaf(n)),
                                 dtype=np.int64),
        'correct_degrees': np.asarray(
            all(d in (0, 2) for d in out_degrees), dtype=bool),
    }
    if want_times:
        record['times'] = np.asarray([float(times[n]) for n in nodes],
                                     dtype=np.float64)
    if want_birth_scale:
        record['birth_scale'] = np.asarray(
            [float(tree.get_attribute(n, 'birth_scale')) for n in nodes],
            dtype=np.float64)
    if seed_leaves is not None:
        record['seed_leaf_birth_scale'] = np.asarray(
            [float(tree.get_attribute(n, 'birth_scale'))
             for n in sorted(seed_leaves)], dtype=np.float64)
    return record


def run_error_case(case):
    """抛在构造还是抛在 simulate_tree，上游分得很清楚，这里照抄。"""
    try:
        sim = build_simulator(case['kwargs'])
        if case['raises_at'] == 'construct':
            return 'no-exception'
        sim.simulate_tree()
    except Exception as exc:  # noqa: BLE001
        return type(exc).__name__
    return 'no-exception'


def subclone_simulator(case):
    kwargs = dict(case['kwargs'])
    for key in ('branch_length_neutral', 'branch_length_fit'):
        if isinstance(kwargs[key], dict):
            kwargs[key] = distribution(kwargs[key], takes_scale=False)
    if case.get('global_seed') is not None:
        np.random.seed(case['global_seed'])       # 照抄上游 test:51 的全局 seed
    return SimpleFitSubcloneSimulator(**kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    logging.disable(logging.CRITICAL)
    warnings.filterwarnings('ignore')
    config = json.loads(args.inputs.read_bytes())
    args.out.mkdir(parents=True, exist_ok=True)
    if any(p.name not in {'run.ok', 'run.failed', 'run.skipped', 'run.log'}
           for p in args.out.iterdir()):
        raise ValueError('OUT_DIR 必须为空')

    start = time.perf_counter()
    arrays, diagnostics = {}, {}

    # ---- 15 个异常用例：只判异常类型名 ----
    error_ids = [c['id'] for c in config['error_cases']]
    arrays['error_ids'] = np.asarray(error_ids, dtype=str)
    arrays['error_exceptions'] = np.asarray(
        [run_error_case(c) for c in config['error_cases']], dtype=str)

    # ---- 18 棵 birth-death 树 ----
    produced = {}
    for case in config['tree_cases']:
        spec = case.get('initial_tree')
        initial, seed_leaves = None, None
        if spec is not None:
            if spec['kind'] == 'explicit':
                initial = explicit_tree(spec)
            elif spec['kind'] == 'from_case':
                initial = produced[spec['case']]       # 链式：复用前一棵已生成的树
            else:
                raise ValueError(f"未知 initial_tree kind={spec['kind']!r}")
            seed_leaves = list(initial.leaves)
        tree = build_simulator(case['kwargs'], initial).simulate_tree()
        produced[case['id']] = tree
        record = observe(tree, want_times=case.get('grade_times', False),
                         want_birth_scale=case.get('grade_birth_scale', False),
                         seed_leaves=seed_leaves
                         if case.get('grade_seed_leaf_birth_scale') else None)
        for key, value in record.items():
            arrays[f"{case['id']}/{key}"] = value

    # ---- 2 个 subclone 用例 ----
    for case in config['subclone_cases']:
        tree = subclone_simulator(case).simulate_tree()
        record = observe(tree, want_times=case.get('grade_times', False),
                         want_birth_scale=False)
        if not case.get('grade_times', False):
            # 上游 test:65-73 唯一断言的东西：内部边长两两互异
            internal = [tree.get_branch_length(p, c) for p, c in tree.edges
                        if not tree.is_leaf(c)]
            record['distinct_internal_branch_lengths'] = np.asarray(
                len(internal) == len(set(internal)), dtype=bool)
            record['internal_branch_count'] = np.asarray(len(internal),
                                                         dtype=np.int64)
            diagnostics[case['id'] + '/internal_branch_lengths'] = internal
        for key, value in record.items():
            arrays[f"{case['id']}/{key}"] = value

    elapsed = time.perf_counter() - start
    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(json.dumps(
        {'ungraded': True, 'seconds': elapsed,
         'note': 'subclone_stochastic 的边长只在此留档，不受判：它靠的是全局 '
                 'np.random.seed(1)，不是该类的 API 承诺',
         **{k: [float(x) for x in v] for k, v in diagnostics.items()}},
        indent=2) + '\n')
    print(f'已导出 {len(arrays)} 个数组；'
          f'{len(error_ids)} 个异常用例 + {len(config["tree_cases"])} 棵树 + '
          f'{len(config["subclone_cases"])} 个 subclone 用例')


if __name__ == '__main__':
    main()
