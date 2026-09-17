"""人工小型 AUC/支持计数/散度 payload 自测，不读取真实nominal也不依赖LIANA。

判分器带第三条腿之后，payload 的**值、支持计数与 direction 标签**都要能被它从一份
`ic/` 独立推出来，否则每一条「合法情形应通过」的用例都会变成假阳性。这里现造三张
**合成的**小曲线表（不是真实 nominal，也不调 LIANA），再让判分器自己的 `recompute`
算出基线。规模是挑过的：`cross_pcf-d25-b99-empty` 仍给出 0 支持（保住那条「支持为 0
合法」的用例），`lric_ag-zeroed-strict` 仍会因 `log2(0) = -inf` 掉一个 interaction。
"""
import csv
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECK = Path(__file__).resolve().parent
SCHEMA = {
    'auc.csv': ['case', 'identity', 'score', 'peak_radius'],
    'support.csv': ['case', 'interactions'],
    'divergence.csv': ['case', 'divergence', 'r_star', 'delta_star', 'direction'],
}

_SPEC = importlib.util.spec_from_file_location('curve_summaries_validator', CHECK / 'validate.py')
VALIDATOR = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(VALIDATOR)

RADII = [0, 40, 60, 80, 100]


def _synthetic_ic():
    root = Path(tempfile.mkdtemp(prefix='curve-summaries-ic-'))
    (root / 'nominal').mkdir()

    def dump(name, header, rows):
        with (root / 'nominal' / name).open('w', newline='', encoding='utf-8') as stream:
            writer = csv.writer(stream)
            writer.writerow(header)
            writer.writerows(rows)

    shape = [1.0, 0.8, 1.3, 0.5, 1.1]
    dump('curves-cross_pcf.csv', ['source', 'target', 'interaction', 'radius', 'g'],
         [[s, t, f'{s}^{t}', r, base * k]
          for s, t, base in (('A', 'B', 1.4), ('A', 'C', 0.7))
          for r, k in zip(RADII, shape)])
    dump('curves-lric_ag.csv', ['ligand_complex', 'receptor_complex', 'interaction', 'radius', 'g'],
         [[l, rc, f'{l}^{rc}', r, base * k]
          for l, rc, base in (('L1', 'R1', 1.2), ('L2', 'R2', 0.9))
          for r, k in zip(RADII, [1.0, 1.5, 0.6, 1.2, 0.9])])
    dump('curves-lric_ct.csv',
         ['source', 'target', 'ligand_complex', 'receptor_complex', 'interaction', 'radius', 'g'],
         [['A', 'B', 'L1', 'R1', 'L1^R1', r, g] for r, g in zip(RADII, [1.1, 0.9, 1.4, 0.8, 1.0])])
    return root


IC_ROOT = _synthetic_ic()
_AUC, _SUPPORT, _DIVERGENCE = VALIDATOR.recompute(IC_ROOT / 'nominal')

TABLES = {
    'auc.csv': [[case, identity, repr(score), repr(peak)]
                for (case, identity), (score, peak) in sorted(_AUC.items())],
    'support.csv': [[case, count] for (case,), count in sorted(_SUPPORT.items())],
    'divergence.csv': [[case, repr(d), repr(r), repr(delta), direction]
                       for (case,), (d, r, delta, direction) in sorted(_DIVERGENCE.items())],
}
COMPARISON = {'atol': 1e-6, 'rtol': 1e-6, 'expected_auc_cases': len(TABLES['support.csv']),
              'inputs_root': str(IC_ROOT), 'files': [{'path': name} for name in SCHEMA]}

AUC_ROW = 0                                    # 第一条 AUC 行，值与 peak 都有定义
OTHER_AUC_ROW = next(i for i, r in enumerate(TABLES['auc.csv'])
                     if r[0] == TABLES['auc.csv'][0][0] and r[2] != TABLES['auc.csv'][0][2])
ZERO_SUPPORT = next(i for i, r in enumerate(TABLES['support.csv']) if r[1] == 0)
NONZERO_SUPPORT = next(i for i, r in enumerate(TABLES['support.csv']) if r[1])
DIV_ROW = next(i for i, r in enumerate(TABLES['divergence.csv']) if float(r[1]) > 0)
OTHER_DIRECTION = {'A > B': 'B > A', 'B > A': 'A > B'}[TABLES['divergence.csv'][DIV_ROW][4]]


def rows_of(name):
    return [r.copy() for r in TABLES[name]]


def edited(name, index, column, value):
    rows = rows_of(name)
    rows[index][SCHEMA[name].index(column)] = value
    return rows


class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ref = self.root / 'reference'
        self.cand = self.root / 'candidate'
        self.ref.mkdir()
        self.cand.mkdir()
        self.rubric = self.root / 'rubric.json'
        self.rubric.write_text(json.dumps({'comparison': COMPARISON}))
        self.write(self.ref, TABLES)

    def write(self, folder, tables, headers=None):
        headers = headers or SCHEMA
        for name, rows in tables.items():
            with (folder / name).open('w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(headers[name])
                writer.writerows(rows)

    def check(self, tables, passed, headers=None):
        self.write(self.cand, tables, headers)
        out = self.root / 'result.json'
        result = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.ref), '--candidate', str(self.cand), '--rubric', str(self.rubric), '--out', str(out)], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(out.is_file())
        payload = out.read_bytes()
        self.assertTrue(payload.isascii())
        record = json.loads(payload, parse_constant=lambda s: self.fail('非标准JSON ' + s))
        self.assertIs(record['passed'], passed, record)
        return record

    def replace(self, name, rows):
        tables = {k: [r.copy() for r in v] for k, v in TABLES.items()}
        tables[name] = rows
        return tables

    def test_identical_tables_pass_and_count_every_value(self):
        result = self.check(TABLES, True)
        self.assertEqual(result['distance'], 0)
        self.assertEqual(result['values'], 2 * len(TABLES['auc.csv']) + 3 * len(TABLES['divergence.csv']))
        self.assertEqual(result['auc_rows'], len(TABLES['auc.csv']))
        self.assertEqual(result['divergence_cases'], len(TABLES['divergence.csv']))

    def test_row_permutation_passes(self):
        self.check(self.replace('auc.csv', list(reversed(TABLES['auc.csv']))), True)

    def test_column_order_is_not_science(self):
        headers = {k: list(reversed(v)) for k, v in SCHEMA.items()}
        self.check({k: [list(reversed(r)) for r in v] for k, v in TABLES.items()}, True, headers)

    def test_small_float64_roundoff_passes(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'score', repr(float(TABLES['auc.csv'][AUC_ROW][2]) + 1e-9))), True)

    def test_score_change_beyond_the_bound_fails(self):
        rows = TABLES['auc.csv']
        result = self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'score', repr(float(TABLES['auc.csv'][AUC_ROW][2]) + 0.01))), False)
        self.assertGreater(result['bound_fraction'], 1)

    def test_peak_radius_change_fails(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'peak_radius', '999.0')), False)

    def test_identity_score_binding_error_fails(self):
        rows = TABLES['auc.csv']
        rows = rows_of('auc.csv')
        rows[AUC_ROW][2], rows[OTHER_AUC_ROW][2] = rows[OTHER_AUC_ROW][2], rows[AUC_ROW][2]
        self.check(self.replace('auc.csv', rows), False)

    def test_case_is_part_of_the_identity(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'case', 'no-such-case')), False)

    def test_missing_auc_row_fails(self):
        self.check(self.replace('auc.csv', TABLES['auc.csv'][:-1]), False)

    def test_duplicate_auc_key_fails(self):
        rows = TABLES['auc.csv']
        rows = rows_of('auc.csv')
        self.check(self.replace('auc.csv', [rows[0]] + rows[:-1]), False)

    def test_support_count_change_fails(self):
        self.check(self.replace('support.csv', edited('support.csv', NONZERO_SUPPORT, 'interactions', TABLES['support.csv'][NONZERO_SUPPORT][1] + 1)), False)

    def test_support_case_set_change_fails(self):
        self.check(self.replace('support.csv', edited('support.csv', NONZERO_SUPPORT, 'case', 'no-such-case')), False)

    def test_negative_support_fails(self):
        self.check(self.replace('support.csv', edited('support.csv', NONZERO_SUPPORT, 'interactions', -2)), False)

    def test_zero_support_case_is_legal(self):
        # 合成初值里 cross_pcf-d25-b99-empty 本来就是 0 支持，基线即是合法的零。
        self.assertEqual(TABLES['support.csv'][ZERO_SUPPORT][1], 0)
        self.check(TABLES, True)

    def test_direction_change_fails(self):
        self.check(self.replace('divergence.csv', edited('divergence.csv', DIV_ROW, 'direction', OTHER_DIRECTION)), False)

    def test_unknown_direction_fails(self):
        self.check(self.replace('divergence.csv', edited('divergence.csv', DIV_ROW, 'direction', 'sideways')), False)

    def test_divergence_value_change_fails(self):
        self.check(self.replace('divergence.csv', edited('divergence.csv', DIV_ROW, 'divergence', repr(float(TABLES['divergence.csv'][DIV_ROW][1]) + 0.1))), False)

    def test_delta_star_sign_flip_fails(self):
        self.check(self.replace('divergence.csv', edited('divergence.csv', DIV_ROW, 'delta_star', repr(-float(TABLES['divergence.csv'][DIV_ROW][3])))), False)

    def test_negative_divergence_fails(self):
        self.check(self.replace('divergence.csv', edited('divergence.csv', DIV_ROW, 'divergence', '-1.5')), False)

    def test_missing_divergence_case_fails(self):
        self.check(self.replace('divergence.csv', TABLES['divergence.csv'][:1]), False)

    def test_nan_fails(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'score', 'nan')), False)

    def test_inf_fails(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'score', 'inf')), False)

    def test_empty_identity_fails(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'identity', '')), False)

    def test_empty_auc_table_fails(self):
        self.check(self.replace('auc.csv', []), False)

    def test_duplicate_header_fails(self):
        headers = dict(SCHEMA, **{'auc.csv': ['case', 'case', 'score', 'peak_radius']})
        self.check(TABLES, False, headers)

    def test_missing_graded_file_fails_with_json(self):
        self.write(self.cand, TABLES)
        (self.cand / 'divergence.csv').unlink()
        out = self.root / 'result.json'
        result = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.ref), '--candidate', str(self.cand), '--rubric', str(self.rubric), '--out', str(out)], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIs(json.loads(out.read_bytes())['passed'], False)

    def test_malformed_numeric_field_fails_with_json(self):
        rows = TABLES['auc.csv']
        self.check(self.replace('auc.csv', edited('auc.csv', AUC_ROW, 'score', 'not-a-number')), False)

    def test_invalid_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(TABLES, False)

    def test_nan_tolerance_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': dict(COMPARISON, atol='@NAN@', rtol=0)}).replace('"@NAN@"', 'NaN'))
        self.check(TABLES, False)

    def test_negative_tolerance_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': dict(COMPARISON, atol=-1, rtol=0)}))
        self.check(TABLES, False)

    def test_wrong_expected_case_count_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': dict(COMPARISON, expected_auc_cases=len(TABLES['support.csv']) + 1)}))
        self.check(TABLES, False)

    def test_boolean_expected_case_count_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': dict(COMPARISON, expected_auc_cases=True)}))
        self.check(TABLES, False)

    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐值比较结构上拒不掉，只有独立重算能拒。"""
        polluted = self.replace('auc.csv', edited(
            'auc.csv', AUC_ROW, 'score', repr(float(TABLES['auc.csv'][AUC_ROW][2]) + 0.01)))
        self.write(self.ref, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_two_sided_direction_pollution_is_caught_by_the_third_leg(self):
        """direction 是字符串标签，比较只看两侧是否相同；它同样有独立期望。"""
        polluted = self.replace('divergence.csv',
                                edited('divergence.csv', DIV_ROW, 'direction', OTHER_DIRECTION))
        self.write(self.ref, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_two_sided_support_pollution_is_caught_by_the_third_leg(self):
        """支持计数两侧同错：比较只看一致性，计数本身由第三条腿独立推。"""
        polluted = self.replace('support.csv', edited(
            'support.csv', NONZERO_SUPPORT, 'interactions',
            TABLES['support.csv'][NONZERO_SUPPORT][1] + 1))
        self.write(self.ref, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_third_leg_covers_every_graded_item(self):
        measurements = self.check(TABLES, True)['measurements']
        self.assertEqual(measurements['items_with_a_third_leg'], measurements['graded_items'])
        self.assertIs(measurements['third_leg_is_partial'], False)
        self.assertEqual(measurements['third_leg']['reference']['max_abs_gap'], 0.0)


if __name__ == '__main__':
    unittest.main()
