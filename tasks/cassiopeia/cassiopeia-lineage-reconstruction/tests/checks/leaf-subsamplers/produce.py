"""产出 leaf-subsamplers 的受判产物。

覆盖 spatial_leaf_subsampler_test.py 的六个 test：`SpatialLeafSubsampler` 在四个
空间过滤配置下的精确结果、`keep_singular_root_edge` 两档、两个随机配置的不变量，
以及 12 个非法配置的异常。

**确定与随机分开处理。** 实测（2026-09-13，不播种连跑 4 次）：`bounding_box` 与
`space` 四个配置各只有一种输出——它们是纯空间过滤，不用 RNG；而 `ratio` 与
`number_of_leaves` 分别得到 2 种与 3 种互异输出。所以前者评精确边集，
后者只评不变量。上游对后者播 `np.random.seed(10)` 再断言精确边集，本 check 不这么做。
"""
from __future__ import annotations

import argparse
import json
import logging
import warnings
from pathlib import Path

import networkx as nx
import numpy as np

from cassiopeia.data import CassiopeiaTree
from cassiopeia.simulator import SpatialLeafSubsampler


def build_fixture(config):
    spec = config['fixture']
    b = spec['balanced_tree']
    graph = nx.balanced_tree(b['r'], b['h'], create_using=nx.DiGraph)
    graph = nx.relabel_nodes(graph, {i: f"{b['relabel_prefix']}{i}" for i in graph.nodes})
    plain = CassiopeiaTree(tree=graph)

    three = plain.copy()
    for node, xyz in spec['spatial_3d'].items():
        three.set_attribute(node, 'spatial', tuple(xyz))

    two = three.copy()
    for node in two.leaves:
        two.set_attribute(node, 'spatial',
                          tuple(two.get_attribute(node, 'spatial')[:2]))

    outlier = two.copy()
    outlier.set_attribute('node3', 'spatial', (100, 10))

    regions = {}
    for name in ('space_3d', 'space_2d'):
        s = spec[name]
        arr = np.zeros(tuple(s['shape']))
        lo, hi = s['true_slice']
        arr[lo:hi] = 1
        regions[name] = arr.astype(bool)
    regions['bounding_box_3d'] = [tuple(p) for p in spec['bounding_box_3d']]
    regions['bounding_box_2d'] = [tuple(p) for p in spec['bounding_box_2d']]
    return {'plain': plain, '3d': three, '2d': two, '2d_outlier': outlier}, regions


def leaf_is_in_region(tree, node, region, regions):
    """坐标是否落在该区域内；语义与 SpatialLeafSubsampler 的过滤一致。

    bounding box 是**逐维**的 `(lo, hi)` 列表，不是两个角点。
    """
    coord = tree.get_attribute(node, 'spatial')
    if 'bounding_box' in region:
        box = region['bounding_box']
        box = regions[box] if isinstance(box, str) else [tuple(p) for p in box]
        return all(lo <= c <= hi for c, (lo, hi) in zip(coord, box))
    mask = regions[region['space']]
    idx = tuple(int(c) for c in coord)
    if any(i < 0 or i >= s for i, s in zip(idx, mask.shape)):
        return False
    return bool(mask[idx[0]])


def descendant_leaves(tree, node):
    """node 在源树上的后代叶集合；node 本身是叶时就是 {node}。"""
    if node not in set(tree.nodes):
        return set()
    out, stack = set(), [node]
    while stack:
        current = stack.pop()
        kids = list(tree.children(current))
        if not kids:
            out.add(current)
        else:
            stack.extend(kids)
    return out


def resolve(kwargs, regions):
    out = {}
    for key, value in kwargs.items():
        if isinstance(value, str) and value in regions:
            out[key] = regions[value]
        elif key == 'bounding_box' and isinstance(value, list):
            out[key] = [tuple(p) for p in value]
        else:
            out[key] = value
    return out


def pack(cid, result):
    edges = sorted(f'{u}->{v}' for u, v in result.edges)
    return {f'{cid}.edge_ids': np.asarray(edges, dtype=str),
            f'{cid}.leaf_ids': np.asarray(sorted(result.leaves), dtype=str),
            f'{cid}.node_count': np.asarray(len(list(result.nodes)), dtype=np.int64)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    logging.disable(logging.CRITICAL)
    warnings.filterwarnings('ignore')
    config = json.loads(args.inputs.read_text())
    args.out.mkdir(parents=True, exist_ok=True)
    if any(p.name not in {'run.ok', 'run.failed', 'run.skipped', 'run.log'}
           for p in args.out.iterdir()):
        raise ValueError('OUT_DIR 必须为空')

    trees, regions = build_fixture(config)
    arrays = {}

    for spec in config['deterministic_configs']:
        sampler = SpatialLeafSubsampler(**resolve(spec['kwargs'], regions))
        arrays.update(pack(spec['id'], sampler.subsample_leaves(trees[spec['tree']])))

    sampler = SpatialLeafSubsampler(bounding_box=regions['bounding_box_3d'])
    for spec in config['root_edge_configs']:
        result = sampler.subsample_leaves(
            trees['3d'], keep_singular_root_edge=spec['keep_singular_root_edge'])
        arrays.update(pack(spec['id'], result))

    # 随机配置：只评不变量，不评具体边集
    for spec in config['stochastic_configs']:
        sampler = SpatialLeafSubsampler(**resolve(spec['kwargs'], regions))
        result = sampler.subsample_leaves(trees[spec['tree']])
        got = set(result.leaves)
        # ⚠ 不变量是「⊆ 源树**全部节点**」而不是「⊆ 源树的叶」。实测：`ratio=0.5`
        # 只留下 1 片叶时，单分叉被折叠，存活节点沿用**祖先**的名字（结果叶是
        # `node1` 或 `node2`，它们在源树里是内部节点）。初版写成 ⊆ 源叶，被这次
        # 实测证伪。
        source_nodes = set(trees[spec['tree']].nodes)
        arrays[f'{spec["id"]}.leaf_count'] = np.asarray(len(got), dtype=np.int64)
        arrays[f'{spec["id"]}.leaves_subset_of_source_nodes'] = np.asarray(
            int(got <= source_nodes), dtype=np.int64)
        arrays[f'{spec["id"]}.result_is_a_tree'] = np.asarray(
            int(len(list(result.edges)) == len(list(result.nodes)) - 1),
            dtype=np.int64)
        # 上面三项全都可以只读 `ic/` 算出来（叶数由 SpatialLeafSubsampler:203-205
        # 的规则定死，两个布尔恒为真），所以它们对「抽样抽对了没有」**没有信息量**。
        # 这一项补上信息量：它恒为真、因而跨运行稳定，但**计算依赖真实抽中的那几片叶**
        # ——每个结果叶在源树上的后代叶里必须至少有一片落在区域内。抽到区域外就为假。
        # 注意不能写成「结果叶 ⊆ 区域内源叶」：只剩一片叶时单分叉折叠会让存活节点
        # 沿用**祖先**的名字（见上面那条实测记录）。
        source = trees[spec['tree']]
        region = {k: v for k, v in spec['kwargs'].items()
                  if k in ('bounding_box', 'space')}
        in_region_leaves = {
            n for n in source.leaves
            if leaf_is_in_region(source, n, region, regions)}
        arrays[f'{spec["id"]}.drawn_leaves_all_trace_to_region'] = np.asarray(
            int(all(bool(descendant_leaves(source, x) & in_region_leaves)
                    for x in got)), dtype=np.int64)

    ids, kinds = [], []
    for case in config['init_error_cases']:
        ids.append(f'init.{case["id"]}')
        try:
            SpatialLeafSubsampler(**resolve(case['kwargs'], regions))
            kinds.append('no-exception')
        except Exception as exc:  # noqa: BLE001
            kinds.append(type(exc).__name__)
    for case in config['subsample_error_cases']:
        ids.append(f'subsample.{case["id"]}')
        try:
            s = SpatialLeafSubsampler(**resolve(case['kwargs'], regions))
            s.subsample_leaves(trees[case['tree']])
            kinds.append('no-exception')
        except Exception as exc:  # noqa: BLE001
            kinds.append(type(exc).__name__)
    arrays['errors.ids'] = np.asarray(ids, dtype=str)
    arrays['errors.exception'] = np.asarray(kinds, dtype=str)

    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(
        json.dumps({'ungraded': True}, indent=2) + '\n')
    print(f'已导出 {len(arrays)} 个数组')


if __name__ == '__main__':
    main()
