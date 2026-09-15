#!/usr/bin/env python3
"""lineage-group-calling 判分器的自检。

不 import cassiopeia：全部用例都从**已跑出的** results.npz 出发做扰动，只需 numpy。
两个 IC 的产物由 `--fixtures` 指定，缺失时用 produce.py 现跑（那一步才需要
cassiopeia）。**默认落点在 check 目录之外**：`environment/Dockerfile` 的
`COPY tests/` 会把这里的一切发给 solver，而这些产物就是参考答案。

每一族受判量都有一对 RED→GREEN：只有 RED 的判分器可以靠「永远拒绝」蒙混，
只有 GREEN 的可以靠「永远接受」蒙混，两侧都断言才说明界落在中间。这一格是离散的，
所以 GREEN 的形式是「**不改变科学的重写**必须仍通过」——交换谱系群的原始编号、
置换存放顺序，都属于此。
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
# 上游 call_lineage_groups_test.py 自己断言的 (cellBC,intBC) -> UMI
UPSTREAM_UMI = {
    'basic_grouping': {'A|XX': 3, 'B|XX': 1, 'C|XZ': 1},
    'reassign': {'A|XX': 2, 'B|XX': 2, 'C|XX': 2, 'D|XX': 1, 'E|XZ': 1, 'F|XZ': 3},
    'filter_and_reassign': {'A|XX': 1, 'B|XX': 1, 'C|XX': 1, 'D|XX': 1,
                            'E|XZ': 2, 'F|XZ': 2},
    'doublet': {'A|XX': 2, 'B|XX': 2, 'D|XY': 2},
    'single_lineage_allele_table': {'A|XX': 3, 'B|XX': 1, 'C|XZ': 1},
}
# 上游按标签隐含的分组（同一标签的 cell 必须同组，不同标签必须不同组）
UPSTREAM_PARTITION = {
    'basic_grouping': [['A', 'B'], ['C']],
    'reassign': [['A', 'B', 'C', 'D', 'E', 'F']],
    'filter_and_reassign': [['A', 'B', 'C', 'D'], ['E', 'F']],
    'doublet': [['A', 'B'], ['D']],
    'single_lineage_allele_table': [['A', 'B', 'C']],
}


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
                        default=Path(tempfile.gettempdir()) / 'sab-lgc-fixtures')
    args = parser.parse_args()

    for name in ('nominal', 'variant'):
        target = args.fixtures / name
        if target.is_dir():
            continue
        target.mkdir(parents=True)
        subprocess.run([sys.executable, str(HERE / 'produce.py'),
                        '--inputs', str(HERE / 'ic' / name / 'inputs.json'),
                        '--out', str(target)], check=True)
    nominal, variant = args.fixtures / 'nominal', args.fixtures / 'variant'

    ok = []
    tmpdir = tempfile.mkdtemp()
    rubric = rubric_with(tmpdir)

    base = run_validator(nominal, nominal, rubric)
    ok.append(check('identity_passes', base['passed']))
    ok.append(check('every_item_has_a_third_leg',
                    base['measurements']['items_without_a_third_leg'] == [],
                    f"{base['measurements']['items_with_a_third_leg']}/"
                    f"{base['measurements']['graded_items']}"))
    ok.append(check('variant_passes_on_its_own_ic',
                    run_validator(variant, variant, rubric)['passed']))

    # ---- 第三条腿必须对得上上游自己的断言 ----
    spec = importlib.util.spec_from_file_location('sab_lgc_validate',
                                                  HERE / 'validate.py')
    V = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(V)
    config = json.loads((HERE / 'ic' / 'nominal' / 'inputs.json').read_text())
    rec = V.expected_tables(config)

    bad = []
    for cid, wanted in UPSTREAM_UMI.items():
        pid = list(rec[f'{cid}.pair_ids'])
        umi = rec[f'{cid}.pair_umi']
        for key, value in wanted.items():
            if key not in pid or int(umi[pid.index(key)]) != value:
                bad.append(f'{cid}:{key}')
    ok.append(check('third_leg_matches_upstream_umi', not bad, str(bad)))

    bad = []
    for cid, blocks in UPSTREAM_PARTITION.items():
        cells = list(rec[f'{cid}.cell_ids'])
        grp = rec[f'{cid}.canonical_group']
        mapping = {c: int(g) for c, g in zip(cells, grp)}
        seen = []
        for block in blocks:
            ids = {mapping[c] for c in block if c in mapping}
            if len(ids) != 1:
                bad.append(f'{cid}:{block} 未落在同一组')
            seen.append(tuple(sorted(ids)))
        if len(set(seen)) != len(seen):
            bad.append(f'{cid}: 上游的不同标签落进了同一组')
    ok.append(check('third_leg_matches_upstream_partition', not bad, str(bad)))
    ok.append(check('third_leg_drops_YZ_in_filter_and_reassign',
                    'YZ' not in list(rec['filter_and_reassign.intbc_ids'])))

    # ---- RED：每一族受判量的科学性改动都必须被拒 ----
    reds = {
        'partition_merged': lambda t: t.__setitem__(
            'doublet.canonical_group', np.zeros_like(t['doublet.canonical_group'])),
        'one_cell_moved': lambda t: t.__setitem__(
            'filter_and_reassign.canonical_group',
            np.concatenate([t['filter_and_reassign.canonical_group'][:1] + 1,
                            t['filter_and_reassign.canonical_group'][1:]])),
        'umi_count': lambda t: t.__setitem__(
            'basic_grouping.pair_umi', t['basic_grouping.pair_umi'] + 1),
        'cell_dropped': lambda t: t.__setitem__(
            'reassign.cell_ids', np.asarray(
                ['ZZZ'] + list(t['reassign.cell_ids'][1:]), dtype=str)),
        'intbc_set': lambda t: t.__setitem__(
            'filter_and_reassign.intbc_ids', np.asarray(
                list(t['filter_and_reassign.intbc_ids']) + ['YZ'], dtype=str)),
        'row_count': lambda t: t.__setitem__(
            'doublet.row_count', t['doublet.row_count'] + 1),
        'column_missing': lambda t: t.__setitem__(
            'doublet.columns_present',
            np.asarray(list(t['doublet.columns_present'])[:-1], dtype=str)),
        'dropped_item': lambda t: t.pop('doublet.row_count'),
    }
    for i, (name, mutate) in enumerate(reds.items()):
        cand = clone(nominal, Path(tmpdir) / f'red{i}', mutate)
        ok.append(check(f'RED_{name}_is_rejected',
                        not run_validator(nominal, cand, rubric)['passed']))

    # ---- GREEN：不改变科学的重写必须仍通过 ----
    rng = np.random.default_rng(0)

    def permute_cells(t):
        n = len(t['doublet.cell_ids'])
        p = rng.permutation(n)
        t['doublet.cell_ids'] = t['doublet.cell_ids'][p]
        t['doublet.canonical_group'] = t['doublet.canonical_group'][p]

    def permute_pairs(t):
        n = len(t['basic_grouping.pair_ids'])
        p = rng.permutation(n)
        t['basic_grouping.pair_ids'] = t['basic_grouping.pair_ids'][p]
        t['basic_grouping.pair_umi'] = t['basic_grouping.pair_umi'][p]

    cand = clone(nominal, Path(tmpdir) / 'green0', permute_cells)
    ok.append(check('GREEN_permuted_cell_storage_order_still_passes',
                    run_validator(nominal, cand, rubric)['passed']))
    cand = clone(nominal, Path(tmpdir) / 'green1', permute_pairs)
    ok.append(check('GREEN_permuted_pair_storage_order_still_passes',
                    run_validator(nominal, cand, rubric)['passed']))

    # 反向对照：只置换身份而不动数值 —— 映射被改掉了，必须被拒
    def permute_ids_only(t):
        n = len(t['doublet.cell_ids'])
        t['doublet.cell_ids'] = t['doublet.cell_ids'][rng.permutation(n)]
    cand = clone(nominal, Path(tmpdir) / 'red-perm', permute_ids_only)
    ok.append(check('permuted_ids_without_values_is_NOT_accepted',
                    not run_validator(nominal, cand, rubric)['passed']))

    # ---- IC 归属不可伪造 ----
    # `sab.py task selfcheck` 的计分跑就是 reference=nominal、candidate=variant。
    # 判分器必须**接受**这个形状——早先要求两侧 IC 相同的写法会把 selfcheck 判失败，
    # 而当时的自检还把那个错误行为写成了断言。这两条就是为了不再退回去。
    res = run_validator(nominal, variant, rubric)
    ok.append(check('selfcheck_shape_nominal_vs_variant_passes', res['passed'],
                    f"bound_fraction={res['bound_fraction']}"))
    ok.append(check('each_side_resolves_its_own_ic',
                    res['measurements'].get('initial_condition')
                    == {'reference': 'nominal', 'candidate': 'variant'},
                    str(res['measurements'].get('initial_condition'))))
    forged = Path(tmpdir) / 'forged-ic'
    shutil.copytree(nominal, forged)
    doc = json.loads((forged / 'inputs.used.json').read_text())
    doc['cases'][0]['params']['min_intbc_thresh'] = 0.9
    (forged / 'inputs.used.json').write_text(json.dumps(doc))
    ok.append(check('forged_ic_is_rejected',
                    not run_validator(forged, forged, rubric)['passed']))

    # ---- rubric 自身的前提：这一格是离散的 ----
    ok.append(check('nonzero_tolerance_is_rejected',
                    not run_validator(nominal, nominal,
                                      rubric_with(tmpdir, atol=1e-9))['passed']))

    shutil.rmtree(tmpdir, ignore_errors=True)
    print(f'\n{sum(ok)}/{len(ok)} 通过')
    return 0 if all(ok) else 1


if __name__ == '__main__':
    raise SystemExit(main())
