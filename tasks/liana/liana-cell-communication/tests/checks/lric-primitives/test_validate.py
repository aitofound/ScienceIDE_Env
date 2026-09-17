"""人工 case/key payload 自测：读 `ic/nominal` 做基线，但不依赖 LIANA、不读参考产物。

判分器带上第三条腿之后，原来那份 4 数值 / 2 标签的手编 payload 就用不了了：
`recompute` 无论初值是什么都给出同一套 19 个 case 的身份集合，手编的小表永远对不上，
于是每一条「合法情形应通过」的用例都会变成假阳性。基线改成判分器自己的独立重算，
再按 (case, key) 定位来做各种破坏——不再按行号，行号会随重算内容漂。
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
SCHEMA = {'primitives.csv': ['case', 'key', 'value'], 'labels.csv': ['case', 'key', 'label']}

_SPEC = importlib.util.spec_from_file_location('lric_primitives_validator', CHECK / 'validate.py')
VALIDATOR = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(VALIDATOR)
_NUMBERS, _LABELS = VALIDATOR.recompute(CHECK / 'ic' / 'nominal')

TABLES = {
    'primitives.csv': [[case, key, format(value, '.17g')] for (case, key), value in _NUMBERS.items()],
    'labels.csv': [[case, key, text] for (case, key), text in _LABELS.items()],
}
COMPARISON = {'atol': 1e-12, 'rtol': 0.0,
              'expected_numbers': len(_NUMBERS), 'expected_labels': len(_LABELS),
              'expected_cases': len({case for case, _ in _NUMBERS}),
              'files': [{'path': name} for name in SCHEMA]}

# 定位用的锚点，按 (case, key) 而不是行号。
NUM_A = ('make-radii-default-inner', '[0]')     # 0.0
NUM_B = ('make-radii-default-inner', '[1]')     # 40.0
LAB_A = ('to-dense', 'dtype')                   # float32
LAB_B = ('index-resource-missing', 'name[0]')   # GeneA^GeneB


def rows_of(name):
    return [row.copy() for row in TABLES[name]]


def _find(rows, key):
    for index, row in enumerate(rows):
        if (row[0], row[1]) == key:
            return index
    raise AssertionError(f'锚点 {key} 不在 {len(rows)} 行里——重算内容变了，请更新锚点')


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
        result = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.ref), '--candidate', str(self.cand), '--rubric', str(self.rubric), '--out', str(out)], capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = out.read_bytes()
        self.assertTrue(payload.isascii())
        record = json.loads(payload, parse_constant=lambda s: self.fail('非标准JSON ' + s))
        self.assertIs(record['passed'], passed, record)
        return record

    def replace(self, name, rows):
        tables = {k: [r.copy() for r in v] for k, v in TABLES.items()}
        tables[name] = rows
        return tables

    def set_cell(self, name, key, column, value):
        """把某一行的某一列改掉，其余行原样。"""
        rows = rows_of(name)
        rows[_find(rows, key)][column] = value
        return self.replace(name, rows)

    def drop(self, name, key):
        rows = rows_of(name)
        del rows[_find(rows, key)]
        return self.replace(name, rows)

    def test_identical_tables_pass_and_count_everything(self):
        result = self.check(TABLES, True)
        self.assertEqual(result['distance'], 0)
        self.assertEqual(result['values'], len(_NUMBERS))
        self.assertEqual(result['labels'], len(_LABELS))

    def test_row_permutation_passes(self):
        self.check(self.replace('primitives.csv', list(reversed(rows_of('primitives.csv')))), True)

    def test_column_order_is_not_science(self):
        headers = dict(SCHEMA, **{'primitives.csv': ['value', 'key', 'case']})
        self.check(self.replace('primitives.csv', [[c, b, a] for a, b, c in rows_of('primitives.csv')]), True, headers)

    def test_representation_roundoff_passes(self):
        self.check(self.set_cell('primitives.csv', NUM_B, 2, format(40.0 + 1e-14, '.17g')), True)

    def test_value_change_beyond_the_bound_fails(self):
        result = self.check(self.set_cell('primitives.csv', NUM_B, 2, '40.001'), False)
        self.assertGreater(result['bound_fraction'], 1)

    def test_case_is_part_of_the_identity(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 0, 'not-a-real-case'), False)

    def test_key_is_part_of_the_identity(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 1, '[999]'), False)

    def test_identity_value_binding_error_fails(self):
        """两个值互换：集合一样，绑定错了，必须拒。"""
        rows = rows_of('primitives.csv')
        first, second = _find(rows, NUM_A), _find(rows, NUM_B)
        rows[first][2], rows[second][2] = rows[second][2], rows[first][2]
        self.check(self.replace('primitives.csv', rows), False)

    def test_missing_row_fails(self):
        self.check(self.drop('primitives.csv', NUM_A), False)

    def test_duplicate_key_fails(self):
        rows = rows_of('primitives.csv')
        rows[_find(rows, NUM_B)] = rows[_find(rows, NUM_A)].copy()
        self.check(self.replace('primitives.csv', rows), False)

    def test_label_change_fails(self):
        self.check(self.set_cell('labels.csv', LAB_A, 2, 'float64'), False)

    def test_interaction_name_change_fails(self):
        self.check(self.set_cell('labels.csv', LAB_B, 2, 'GeneA|GeneB'), False)

    def test_missing_label_fails(self):
        self.check(self.drop('labels.csv', LAB_A), False)

    def test_empty_label_fails(self):
        self.check(self.set_cell('labels.csv', LAB_A, 2, ''), False)

    def test_nan_fails(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 2, 'nan'), False)

    def test_inf_fails(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 2, 'inf'), False)

    def test_empty_identity_fails(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 0, ''), False)

    def test_empty_table_fails(self):
        self.check(self.replace('primitives.csv', []), False)

    def test_duplicate_header_fails(self):
        headers = dict(SCHEMA, **{'primitives.csv': ['case', 'case', 'value']})
        self.check(TABLES, False, headers)

    def test_missing_graded_file_fails_with_json(self):
        self.write(self.cand, TABLES)
        (self.cand / 'labels.csv').unlink()
        out = self.root / 'result.json'
        result = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.ref), '--candidate', str(self.cand), '--rubric', str(self.rubric), '--out', str(out)], capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIs(json.loads(out.read_bytes())['passed'], False)

    def test_malformed_numeric_field_fails_with_json(self):
        self.check(self.set_cell('primitives.csv', NUM_A, 2, 'x'), False)

    def test_invalid_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(TABLES, False)

    def broken(self, **overrides):
        comparison = dict(COMPARISON)
        comparison.update(overrides)
        self.rubric.write_text(json.dumps({'comparison': comparison}))

    def test_nan_tolerance_fails_with_json(self):
        self.rubric.write_text(json.dumps({'comparison': dict(COMPARISON, atol='@NAN@')}).replace('"@NAN@"', 'NaN'))
        self.check(TABLES, False)

    def test_negative_tolerance_fails_with_json(self):
        self.broken(atol=-1)
        self.check(TABLES, False)

    def test_wrong_expected_numbers_fails_with_json(self):
        self.broken(expected_numbers=len(_NUMBERS) + 1)
        self.check(TABLES, False)

    def test_wrong_expected_labels_fails_with_json(self):
        self.broken(expected_labels=len(_LABELS) + 1)
        self.check(TABLES, False)

    def test_wrong_expected_cases_fails_with_json(self):
        self.broken(expected_cases=COMPARISON['expected_cases'] + 1)
        self.check(TABLES, False)

    def test_boolean_expected_numbers_fails_with_json(self):
        self.broken(expected_numbers=True)
        self.check(TABLES, False)

    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐值比较结构上拒不掉，只有独立重算能拒。"""
        polluted = self.set_cell('primitives.csv', NUM_B, 2, '40.001')
        self.write(self.ref, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_two_sided_label_pollution_is_caught_only_by_the_third_leg(self):
        """字符串输出同理：两侧同改 dtype，只有独立期望能拒。"""
        polluted = self.set_cell('labels.csv', LAB_A, 2, 'float64')
        self.write(self.ref, polluted)
        result = self.check(polluted, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'], ['reference', 'candidate'])

    def test_third_leg_covers_every_graded_item_and_is_exact(self):
        measurements = self.check(TABLES, True)['measurements']
        self.assertEqual(measurements['items_with_a_third_leg'], measurements['graded_items'])
        self.assertIs(measurements['third_leg_is_partial'], False)
        self.assertEqual(measurements['third_leg']['reference']['max_abs_gap'], 0.0)
        self.assertEqual(measurements['initial_conditions_recomputed'], ['nominal', 'variant'])


if __name__ == '__main__':
    unittest.main()
