#!/usr/bin/env python3
"""fitness-estimator 判分器的自检。

不 import cassiopeia：全部用例都从**已跑出的** results.npz 出发做扰动，只需 numpy。
产物由 `--fixtures` 指定，缺失时用 produce.py 现跑（那一步才需要 cassiopeia）。
**默认落点在 check 目录之外**：`COPY tests/` 会把这里的一切发给 solver。

本 check 的第三条腿不是逐值复算，而是 5 条从树结构导出的必要约束。所以自检额外
承担一件事：**逐条验证那些约束真的会拒**，否则「有独立约束」只是句空话。
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = Path(__file__).resolve().parent
# 12 次运行实测唯一的名次签名：internal-2 > internal-1 > internal-3 > {叶1,2,3} > {叶4,5}
EXPECTED_RANK = {'internal-2': 0, 'internal-1': 1, 'internal-3': 2,
                 'leaf-1': 3, 'leaf-2': 3, 'leaf-3': 3, 'leaf-4': 4, 'leaf-5': 4}
EXPECTED_GROUPS = [1, 1, 1, 3, 2]


def run_validator(ref, cand, rubric):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'result.json'
        proc = subprocess.run(
            [sys.executable, '-B', '-s', '-E', str(HERE / 'validate.py'),
             '--reference', str(ref), '--candidate', str(cand),
             '--rubric', str(rubric), '--out', str(out)],
            cwd=HERE, capture_output=True, text=True)
        if not out.is_file():
            raise AssertionError(f'validate.py 未写出结果：{proc.stderr[-400:]}')
        return json.loads(out.read_text())


def clone(src, dst, mutate=None):
    shutil.copytree(src, dst)
    if mutate is None:
        return dst
    with np.load(dst / 'results.npz', allow_pickle=False) as data:
        tables = {k: data[k].copy() for k in data.files}
    mutate(tables)
    np.savez(dst / 'results.npz', **tables)
    return dst


def rubric_with(tmp, **overrides):
    doc = json.loads((HERE / 'rubric.json').read_text())
    doc['comparison'].update(overrides)
    doc['comparison'].setdefault('inputs_root', str(HERE / 'ic'))
    path = Path(tmp) / f'rubric-{len(list(Path(tmp).glob("rubric-*")))}.json'
    path.write_text(json.dumps(doc))
    return path


def check(name, cond, detail=''):
    print(f'{"PASS" if cond else "FAIL"}  {name}{"  " + detail if detail else ""}')
    return bool(cond)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--fixtures', type=Path,
                        default=Path(tempfile.gettempdir()) / 'sab-fe-fixtures')
    parser.add_argument('--runs', type=int, default=2)
    args = parser.parse_args()

    # 两次**独立**运行：LBI 有随机性，原始数值会漂，受判面必须不漂
    for i in range(args.runs):
        target = args.fixtures / f'run{i}'
        if target.is_dir():
            continue
        target.mkdir(parents=True)
        subprocess.run([sys.executable, str(HERE / 'produce.py'),
                        '--inputs', str(HERE / 'ic' / 'nominal' / 'inputs.json'),
                        '--out', str(target)], check=True)
    a, b = args.fixtures / 'run0', args.fixtures / 'run1'

    ok = []
    tmpdir = tempfile.mkdtemp()
    rubric = rubric_with(tmpdir)

    base = run_validator(a, b, rubric)
    ok.append(check('two_independent_runs_agree', base['passed'],
                    f"bf={base['bound_fraction']}"))
    ok.append(check('all_five_constraints_hold',
                    base['measurements']['constraint_failures'] == [],
                    str(base['measurements']['constraint_failures'])))
    ok.append(check('partial_third_leg_is_declared',
                    base['measurements']['third_leg_is_partial'] is True
                    and base['measurements']['items_with_a_third_leg'] == 0))

    # ---- 原始数值确实在漂，而受判面不漂：这条 check 的立身之本 ----
    raws = [json.loads((d / 'diagnostics.json').read_text())['raw_fitness']
            for d in (a, b)]
    drift = max(abs(raws[0][k] - raws[1][k]) / abs(raws[0][k]) for k in raws[0])
    with np.load(a / 'results.npz') as x, np.load(b / 'results.npz') as y:
        graded_same = all(np.array_equal(x[k], y[k]) for k in x.files)
    ok.append(check('raw_values_drift_but_graded_surface_does_not',
                    drift > 0 and graded_same, f'原始相对漂移={drift:.3e}'))

    # ---- 名次签名就是实测那一个 ----
    with np.load(a / 'results.npz') as x:
        got = {str(n): int(r) for n, r in zip(x['node_ids'], x['rank_of_node'])}
        sizes = [int(s) for s in x['group_sizes']]
    ok.append(check('rank_signature_is_the_measured_one', got == EXPECTED_RANK, str(got)))
    ok.append(check('group_sizes_are_the_measured_ones', sizes == EXPECTED_GROUPS, str(sizes)))

    # ---- RED：受判量的科学性改动必须被拒 ----
    reds = {
        # 注意：原 rank 就是 [1,0,2,...]，写成 [1,0]+rest 是空操作——初版正是这么写的，
        # 被这条 RED 自己抓了出来。这里改成把 internal-1 与 internal-3 真正对调。
        'rank_swapped': lambda t: t.__setitem__(
            'rank_of_node', np.asarray(
                [int(t['rank_of_node'][2]), int(t['rank_of_node'][1]),
                 int(t['rank_of_node'][0])]
                + [int(v) for v in t['rank_of_node'][3:]], dtype=np.int64)),
        'tie_group_split': lambda t: t.__setitem__(
            'group_sizes', np.asarray([1, 1, 1, 2, 3], dtype=np.int64)),
        'group_count': lambda t: t.__setitem__(
            'group_count', t['group_count'] + 1),
        'node_dropped': lambda t: t.__setitem__(
            'node_ids', np.asarray(['ZZZ'] + list(t['node_ids'][1:]), dtype=str)),
        'error_type': lambda t: t.__setitem__(
            'error_exception', np.asarray('ValueError', dtype=str)),
        'dropped_item': lambda t: t.pop('group_count'),
    }
    for i, (name, mutate) in enumerate(reds.items()):
        cand = clone(a, Path(tmpdir) / f'red{i}', mutate)
        ok.append(check(f'RED_{name}_is_rejected',
                        not run_validator(a, cand, rubric)['passed']))

    # ---- 结构约束必须真的会拒（否则「有独立约束」是空话）----
    def break_symmetry(t):
        r = t['rank_of_node'].copy(); r[3] = 9          # 拆散 leaf-1/2/3 同组
        t['rank_of_node'] = r
    cand = clone(a, Path(tmpdir) / 'cons0', break_symmetry)
    res = run_validator(a, cand, rubric)
    ok.append(check('CONSTRAINT_symmetric_siblings_is_enforced',
                    not res['passed'] and any(
                        'symmetric_sibling' in f
                        for f in res['measurements']['constraint_failures']),
                    str(res['measurements']['constraint_failures'])))

    def break_branching(t):
        r = t['rank_of_node'].copy(); r[1], r[2] = r[2], r[1]   # internal-2 / internal-3 互换
        t['rank_of_node'] = r
    cand = clone(a, Path(tmpdir) / 'cons1', break_branching)
    res = run_validator(a, cand, rubric)
    ok.append(check('CONSTRAINT_more_branching_is_enforced',
                    not res['passed'] and any(
                        'more_branching' in f
                        for f in res['measurements']['constraint_failures']),
                    str(res['measurements']['constraint_failures'])))

    # ---- 那条被我写错又收窄的约束：内部子节点排在父之前**必须仍然通过** ----
    # internal-2（rank 0）是 internal-1（rank 1）的子节点。若约束仍是「父在全部子之前」，
    # 参考自身就会被拒。这条断言防止它被改回去。
    ok.append(check('GREEN_internal_child_above_its_parent_is_allowed',
                    run_validator(a, a, rubric)['passed']))

    # ---- rubric 前提 ----
    ok.append(check('nonzero_tolerance_is_rejected',
                    not run_validator(a, a, rubric_with(tmpdir, atol=1e-9))['passed']))

    shutil.rmtree(tmpdir, ignore_errors=True)
    print(f'\n{sum(ok)}/{len(ok)} 通过')
    return 0 if all(ok) else 1


if __name__ == '__main__':
    raise SystemExit(main())
