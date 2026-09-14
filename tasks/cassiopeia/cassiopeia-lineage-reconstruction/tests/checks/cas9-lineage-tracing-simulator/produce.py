"""产出 cas9-lineage-tracing-simulator 的受判产物。

覆盖 cas9_lineage_tracing_simulator_test.py 的全部 12 个 test：4 个 `overlay_data`
配置、14 个异常配置、三次 `collapse_sites`、`get_cassettes`、构造属性、
标量/长度3/长度6 的参数广播等价性，以及两个靠全局种子的辅助方法。

**两种可复现性，区别对待。** 源码 `:246-247` 显示构造参数 `random_seed` **只在
`overlay_data` 内部**调 `np.random.seed`。所以 4 个 overlay 用例是 API 承诺的可复现，
矩阵逐值受判（上游自己也硬编码了期望矩阵）；而 `introduce_states`、`silence_cassettes`
直接调用时不碰它，上游是现设全局种子，那不是本类的承诺——只判结构。
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

import cassiopeia as cas
from cassiopeia.simulator import Cas9LineageTracingDataSimulator


def distribution(spec):
    """封闭词表，不是通用求值器。"""
    if not isinstance(spec, dict) or spec.get('kind') != 'exponential':
        raise ValueError(f'本 check 只认 exponential 分布，得到 {spec!r}')
    scale = spec['scale']
    return lambda: np.random.exponential(scale)


def int_keys(mapping):
    return {int(k): v for k, v in mapping.items()}


def simulator_kwargs(spec):
    out = {}
    for key, value in spec.items():
        if key == 'state_priors':
            out[key] = int_keys(value) if isinstance(value, dict) else value
        elif key == 'state_priors_repeat':
            out['state_priors'] = [int_keys(value['value'])] * value['times']
        elif key == 'state_generating_distribution':
            out[key] = distribution(value)
        else:
            out[key] = value
    return out


def build_tree(spec):
    graph = nx.DiGraph()
    graph.add_edges_from([tuple(e) for e in spec['edges']])
    tree = cas.data.CassiopeiaTree(tree=graph)
    tree.set_times({k: v for k, v in spec['times'].items()})
    return tree


def inheritance_ok(tree):
    """上游 :391-410 的两条继承不变量：父为 -1 则子必为 -1；父非 0 则子必非 0。"""
    for node in tree.depth_first_traverse_nodes(postorder=False):
        if tree.is_root(node):
            if any(s != 0 for s in tree.get_character_states(node)):
                return False
            continue
        parent = tree.get_character_states(tree.parent(node))
        child = tree.get_character_states(node)
        for p, c in zip(parent, child):
            if p == -1 and c != -1:
                return False
            if p != 0 and c == 0:
                return False
    return True


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
    arrays = {}
    specs = config['simulators']

    def make(name):
        return Cas9LineageTracingDataSimulator(**simulator_kwargs(specs[name]))

    # ---- 4 个 overlay 用例：整张矩阵逐值受判 ----
    for name in config['overlay_cases']:
        tree = build_tree(config['tree'])
        # 必须复用这一个实例：`mutation_priors_per_character` 对
        # state_generating_distribution 配置来说是 overlay_data **之后**才填上的，
        # 上游 :497-500 也是 overlay 后才断言它的长度。
        overlaid = make(name)
        overlaid.overlay_data(tree)
        matrix = tree.character_matrix
        arrays[f'{name}/character_matrix'] = matrix.to_numpy().astype(np.int64)
        arrays[f'{name}/leaf_order'] = np.asarray(list(matrix.index), dtype=str)
        nodes = sorted(tree.nodes)
        arrays[f'{name}/node_order'] = np.asarray(nodes, dtype=str)
        arrays[f'{name}/states_by_node'] = np.asarray(
            [tree.get_character_states(n) for n in nodes], dtype=np.int64)
        arrays[f'{name}/inheritance_ok'] = np.asarray(inheritance_ok(tree),
                                                      dtype=bool)
        arrays[f'{name}/n_priors_per_character'] = np.asarray(
            len(overlaid.mutation_priors_per_character[0]), dtype=np.int64)

    # ---- 14 个异常配置 ----
    ids, exceptions = [], []
    for case in config['error_cases']:
        ids.append(case['id'])
        try:
            Cas9LineageTracingDataSimulator(**simulator_kwargs(case['kwargs']))
            exceptions.append('no-exception')
        except Exception as exc:  # noqa: BLE001
            exceptions.append(type(exc).__name__)
    arrays['error_ids'] = np.asarray(ids, dtype=str)
    arrays['error_exceptions'] = np.asarray(exceptions, dtype=str)

    # ---- 构造属性（无 RNG）----
    setup = config['setup_attributes']
    sim = make(setup['simulator'])
    spec = specs[setup['simulator']]
    arrays['setup/number_of_characters'] = np.asarray(
        sim.number_of_cassettes * sim.size_of_cassette, dtype=np.int64)
    arrays['setup/heritable_silencing_rate'] = np.asarray(
        sim.heritable_silencing_rate, dtype=np.float64)
    arrays['setup/stochastic_silencing_rate'] = np.asarray(
        sim.stochastic_silencing_rate, dtype=np.float64)
    arrays['setup/n_priors_per_character'] = np.asarray(
        len(sim.mutation_priors_per_character), dtype=np.int64)
    arrays['setup/n_mutation_priors'] = np.asarray(
        len(sim.mutation_priors), dtype=np.int64)
    arrays['setup/mutation_rate_per_character'] = np.asarray(
        sim.mutation_rate_per_character, dtype=np.float64)
    arrays['setup/prior_sums'] = np.asarray(
        [float(sum(d.values())) for d in sim.mutation_priors_per_character],
        dtype=np.float64)
    del spec

    # ---- get_cassettes 与 collapse_sites（无 RNG）----
    helpers = config['helper_cases']
    arrays['cassettes'] = np.asarray(
        make(helpers['get_cassettes']['simulator']).get_cassettes(),
        dtype=np.int64)
    for call in helpers['collapse_sites']['calls']:
        array, remaining = make(call['simulator']).collapse_sites(
            list(call['character_array']), list(call['cuts']))
        arrays[f"collapse/{call['id']}/array"] = np.asarray(array,
                                                            dtype=np.int64)
        arrays[f"collapse/{call['id']}/remaining_cuts"] = np.asarray(
            sorted(remaining) or [-999], dtype=np.int64)

    # ---- 两个靠全局种子的辅助方法：只判结构 ----
    spec = helpers['introduce_states']
    np.random.seed(spec['global_seed'])            # 照抄上游 test:281
    sim = make(spec['simulator'])
    updated = sim.introduce_states(list(spec['character_array']),
                                   list(spec['cuts']))
    cuts = set(spec['cuts'])
    allowed = set(sim.mutation_priors_per_character[0])
    arrays['introduce_states/untouched_stay_zero'] = np.asarray(
        all(v == 0 for i, v in enumerate(updated) if i not in cuts), dtype=bool)
    arrays['introduce_states/cut_sites_take_a_valid_state'] = np.asarray(
        all(updated[i] in allowed for i in cuts), dtype=bool)
    arrays['introduce_states/length'] = np.asarray(len(updated),
                                                   dtype=np.int64)

    spec = helpers['silence_cassettes']
    np.random.seed(spec['global_seed'])            # 照抄上游 test:300
    sim = make(spec['simulator'])
    updated = sim.silence_cassettes(list(spec['character_array']),
                                    spec['silencing_rate'])
    size = sim.size_of_cassette
    groups = [updated[i:i + size] for i in range(0, len(updated), size)]
    arrays['silence_cassettes/whole_cassettes_only'] = np.asarray(
        all(len(set(g)) == 1 for g in groups), dtype=bool)
    arrays['silence_cassettes/values_are_zero_or_missing'] = np.asarray(
        set(updated) <= {0, sim.stochastic_missing_data_state}, dtype=bool)
    arrays['silence_cassettes/length'] = np.asarray(len(updated),
                                                    dtype=np.int64)

    # ---- 参数广播等价性（无 RNG）----
    broadcast = config['broadcast_cases']
    for group, attribute in (('priors', 'mutation_priors_per_character'),
                             ('rates', 'mutation_rate_per_character')):
        for case in broadcast[group]:
            kwargs = dict(broadcast['base'])
            kwargs.update({k: v for k, v in case.items() if k != 'id'})
            value = getattr(
                Cas9LineageTracingDataSimulator(**simulator_kwargs(kwargs)),
                attribute)
            if group == 'priors':
                flat = [x for d in value for pair in sorted(d.items())
                        for x in (float(pair[0]), float(pair[1]))]
            else:
                flat = [float(x) for x in value]
            arrays[f"broadcast/{case['id']}"] = np.asarray(flat,
                                                           dtype=np.float64)

    elapsed = time.perf_counter() - start
    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(json.dumps(
        {'ungraded': True, 'seconds': elapsed,
         'note': 'introduce_states / silence_cassettes 的返回值只判结构，'
                 '不逐值受判：它们靠 test 现设的全局 np.random 种子，'
                 '不是本类的 API 承诺'}, indent=2) + '\n')
    print(f'已导出 {len(arrays)} 个数组；'
          f'{len(config["overlay_cases"])} 个 overlay 用例 + '
          f'{len(ids)} 个异常用例')


if __name__ == '__main__':
    main()
