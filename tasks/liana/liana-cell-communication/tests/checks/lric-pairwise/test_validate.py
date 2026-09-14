"""通过完整 CSV payload 检查 identity、定义域和数值失败协议。

判分器带第三条腿之后，payload 的**值**必须能被它从一份 `ic/` 独立推出来，否则每一条
「合法情形应通过」的用例都会变成假阳性。这里现造一份**合成的** 8 细胞 / 2 基因 /
2 个 cell type 的 h5ad + 单行 resource（不是真实 nominal，也不调 LIANA），
再让判分器自己的 `recompute` 算出基线。坐标是挑过的，好让三种定义域都出现：
全有限、整行 NaN、以及**只有 `g_expr` 是 NaN** 的混合行（该环里 A-B 没有边、
但 A-A 有，于是 `T_SR = 0 < T`）。
"""
import csv
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import anndata
import numpy as np
import pandas

CHECK = Path(__file__).resolve().parent
HEADER = ['source', 'target', 'ligand_complex', 'receptor_complex', 'interaction', 'radius', 'g', 'g_expr', 'g_pcf']

_SPEC = importlib.util.spec_from_file_location('lric_pairwise_validator', CHECK / 'validate.py')
VALIDATOR = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(VALIDATOR)


def _synthetic_ic():
    expression = np.array([[3., 1.], [1., 2.], [2., 4.], [4., 1.],
                           [1., 3.], [2., 1.], [3., 2.], [1., 1.]], dtype=np.float32)
    frame = anndata.AnnData(expression)
    frame.var_names = ['L', 'R']
    frame.obs['cell_type'] = ['A', 'A', 'A', 'A', 'B', 'B', 'B', 'B']
    # A 在 0/10/150/220，B 在 5/15/25/35：环 [60,80) 里只有 A-A 的一条边（150↔220），
    # 没有任何 A-B 边 → 那一行 g/g_pcf 为 0 而 g_expr 为 NaN。
    frame.obsm['spatial'] = np.array([[0., 0.], [10., 0.], [150., 0.], [220., 0.],
                                      [5., 0.], [15., 0.], [25., 0.], [35., 0.]])
    frame.raw = frame
    root = Path(tempfile.mkdtemp(prefix='lric-pairwise-ic-'))
    (root / 'nominal').mkdir()
    frame.write_h5ad(root / 'nominal' / 'expression.h5ad')
    pandas.DataFrame({'ligand': ['L'], 'receptor': ['R']}).to_csv(
        root / 'nominal' / 'resource.csv', index=False)
    return root


IC_ROOT = _synthetic_ic()
_TRUTH = VALIDATOR.recompute(IC_ROOT / 'nominal')


def _cell(value):
    return 'nan' if math.isnan(value) else repr(value)


ROWS = [[source, target, ligand, receptor, f'{ligand}^{receptor}', radius,
         *[_cell(v) for v in _TRUTH[(source, target, ligand, receptor, radius)]]]
        for (source, target, ligand, receptor, radius) in sorted(_TRUTH)]

# 用途固定的三行，按性质**找**出来而不是写死行号；找不到就直接失败，不会静默空转。
def _row_where(predicate, description):
    for index, row in enumerate(ROWS):
        if predicate(row):
            return index
    raise AssertionError(f'合成初值里没有{description}的行——请调整坐标')


def _finite(row, column):
    return row[HEADER.index(column)] != 'nan'


ALL_FINITE = _row_where(lambda r: all(_finite(r, c) for c in ('g', 'g_expr', 'g_pcf')), '三列全有限')
MIXED_NAN = _row_where(lambda r: _finite(r, 'g') and not _finite(r, 'g_expr'),
                       'g 有限而 g_expr 为 NaN')
OTHER_FINITE = _row_where(
    lambda r: all(_finite(r, c) for c in ('g', 'g_expr', 'g_pcf')) and r[6:] != ROWS[ALL_FINITE][6:],
    '三列全有限且数值与第一行不同')


class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reference = self.root / 'reference'
        self.candidate = self.root / 'candidate'
        self.reference.mkdir()
        self.candidate.mkdir()
        self.rubric = self.root / 'rubric.json'
        self.rubric.write_text(json.dumps({'comparison': {'inputs_root': str(IC_ROOT), 'atol': 1e-6, 'rtol': 1e-5, 'expected_rows': len(ROWS), 'files': [{'path': 'curves.csv', 'format': 'lric-csv'}]}}))
        self.write(self.reference, ROWS)

    def write(self, directory, rows, header=HEADER):
        with (directory / 'curves.csv').open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(header)
            writer.writerows(rows)

    def check(self, rows, passed, header=HEADER):
        self.write(self.candidate, rows, header)
        output = self.root / 'result.json'
        proc = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.reference), '--candidate', str(self.candidate), '--rubric', str(self.rubric), '--out', str(output)], capture_output=True, text=True, timeout=15)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue(output.is_file(), proc.stderr)
        result = json.loads(output.read_text(), parse_constant=lambda x: self.fail('non-standard JSON ' + x))
        self.assertIs(result['passed'], passed, result)
        return result

    def changed(self, row, col, value):
        rows = [r.copy() for r in ROWS]
        rows[row][HEADER.index(col)] = value
        return rows

    def test_identical_with_distinct_nan_domains_passes(self):
        result = self.check(ROWS, True)
        self.assertEqual(result['distance'], 0.0)
        self.assertEqual(result['bound_fraction'], 0.0)

    def test_full_row_permutation_passes(self):
        self.check(list(reversed(ROWS)), True)

    def test_numeric_column_order_is_not_storage_contract(self):
        order = list(reversed(range(len(HEADER))))
        self.check([[r[i] for i in order] for r in ROWS], True, [HEADER[i] for i in order])

    def test_correct_finite_roundoff_passes(self):
        value = float(ROWS[ALL_FINITE][HEADER.index('g')])
        self.check(self.changed(ALL_FINITE, 'g', value * (1 + 2e-6)), True)

    def test_wrong_finite_value_fails(self):
        value = float(ROWS[ALL_FINITE][HEADER.index('g')])
        result = self.check(self.changed(ALL_FINITE, 'g', value + 1.0), False)
        self.assertGreater(result['distance'], 0.1)
        self.assertGreater(result['bound_fraction'], 1)

    def test_nan_to_zero_fails(self):
        self.check(self.changed(MIXED_NAN, 'g_expr', 0), False)

    def test_finite_to_nan_fails(self):
        self.check(self.changed(ALL_FINITE, 'g', 'nan'), False)

    def test_moved_nan_mask_fails(self):
        rows = [r.copy() for r in ROWS]
        column = HEADER.index('g_expr')
        rows[ALL_FINITE][column], rows[MIXED_NAN][column] = rows[MIXED_NAN][column], rows[ALL_FINITE][column]
        self.check(rows, False)

    def test_inf_fails(self):
        self.check(self.changed(ALL_FINITE, 'g', 'inf'), False)

    def test_duplicate_biological_key_fails(self):
        self.check([ROWS[0]] + ROWS[:-1], False)

    def test_missing_row_fails(self):
        self.check(ROWS[:-1], False)

    def test_changed_radius_identity_fails(self):
        self.check(self.changed(0, 'radius', 20), False)  # 20 不是本配置下的输出半径

    def test_interaction_label_mismatch_fails(self):
        self.check(self.changed(0, 'interaction', 'R^L'), False)

    def test_numeric_values_detached_from_keys_fail(self):
        rows = [r.copy() for r in ROWS]
        rows[ALL_FINITE][6:], rows[OTHER_FINITE][6:] = rows[OTHER_FINITE][6:], rows[ALL_FINITE][6:]
        self.check(rows, False)

    def test_duplicate_column_fails(self):
        self.check(ROWS, False, HEADER[:-1] + ['g'])

    def test_empty_payload_fails(self):
        self.check([], False)

    def test_extra_field_fails(self):
        self.check([r + [0] for r in ROWS], False)

    def test_bad_number_fails_with_json(self):
        self.check(self.changed(0, 'g', 'not-a-number'), False)

    def test_negative_tolerance_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': {'atol': -1, 'rtol': 0, 'expected_rows': len(ROWS)}}))
        self.check(ROWS, False)

    def test_malformed_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(ROWS, False)

    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐值比较结构上拒不掉，只有独立重算能拒。"""
        value = float(ROWS[ALL_FINITE][HEADER.index('g')])
        polluted = self.changed(ALL_FINITE, 'g', value + 1.0)
        self.write(self.reference, polluted)
        result = self.check(polluted, False)
        self.assertEqual(sum(c['over_bound'] for c in result['columns'].values()), 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_two_sided_nan_mask_pollution_is_caught_by_the_third_leg(self):
        """NaN 定义域两侧同错：位置一致所以比较放行，但第三条腿独立推了定义域。"""
        polluted = self.changed(MIXED_NAN, 'g_expr', 0.0)
        self.write(self.reference, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_third_leg_covers_every_finite_value(self):
        measurements = self.check(ROWS, True)['measurements']
        self.assertEqual(measurements['items_with_a_third_leg'], measurements['graded_items'])
        self.assertIs(measurements['third_leg_is_partial'], False)
        self.assertEqual(measurements['third_leg']['reference']['max_abs_gap'], 0.0)


if __name__ == '__main__':
    unittest.main()
