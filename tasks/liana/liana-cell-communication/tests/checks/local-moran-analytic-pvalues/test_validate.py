"""人工小型 center/pair 概率矩阵payload自测，不读取真实nominal也不依赖LIANA。"""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECK = Path(__file__).resolve().parent

# 基线不再是编造的小表，而是判分器对 `ic/nominal` 的**独立重算**结果（身份名与真实
# fixture 一致）。判分器现在带第三条腿，会把物理上不对的概率拒掉——用合成值做基线，
# 测的只是比较逻辑自己跟自己。重算是纯 Python 的，不依赖 LIANA 也不依赖 scipy。
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location('_check_validate', CHECK / 'validate.py')
_validate = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_validate)
_TRUTH = _validate.recompute(CHECK / 'ic' / 'nominal')
ROWS = [[center, pair, value] for (center, pair), value in sorted(_TRUTH.items())]
CENTERS = len({r[0] for r in ROWS})
PAIRS = len({r[1] for r in ROWS})

HEADER = ['center', 'pair', 'pvalue']


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
        self.rubric.write_text(json.dumps({'comparison': {'atol': 1e-10, 'rtol': 1e-8, 'expected_centers': 2, 'expected_centers': CENTERS, 'expected_pairs': PAIRS, 'files': [{'path': 'pvalues.csv', 'format': 'center-pair-probability-csv'}]}}))
        self.write(self.ref, ROWS)

    def write(self, folder, rows, header=HEADER):
        with (folder / 'pvalues.csv').open('w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(header)
            writer.writerows(rows)

    def check(self, rows, passed, header=HEADER):
        self.write(self.cand, rows, header)
        out = self.root / 'result.json'
        result = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.ref), '--candidate', str(self.cand), '--rubric', str(self.rubric), '--out', str(out)], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(out.is_file())
        payload = out.read_bytes()
        self.assertTrue(payload.isascii())
        record = json.loads(payload, parse_constant=lambda s: self.fail('非标准JSON ' + s))
        self.assertIs(record['passed'], passed, record)
        return record

    def test_full_matrix_and_legal_zero_one_boundaries_pass(self):
        result = self.check(ROWS, True)
        self.assertEqual(result['distance'], 0)
        self.assertEqual(result['bound_fraction'], 0)

    def test_complete_row_permutation_passes(self):
        self.check(list(reversed(ROWS)), True)

    def test_column_order_is_not_science(self):
        self.check([[c, b, a] for a, b, c in ROWS], True, ['pvalue', 'pair', 'center'])

    def test_small_float64_roundoff_passes(self):
        self.check([[ROWS[0][0], ROWS[0][1], ROWS[0][2] + 1e-11], *ROWS[1:]], True)

    def test_large_probability_change_is_numeric_failure(self):
        result = self.check([[ROWS[0][0], ROWS[0][1], ROWS[0][2] + 0.01], *ROWS[1:]], False)
        self.assertGreater(result['distance'], 0.009)
        self.assertGreater(result['bound_fraction'], 1)

    def test_last_value_is_not_ignored(self):
        self.check([*ROWS[:-1], ['spot-1', 'pair-1', 0.9]], False)

    def test_center_pair_value_binding_error_fails(self):
        self.check([['spot-0', 'pair-0', 0.8], ['spot-0', 'pair-1', 0.2], *ROWS[2:]], False)

    def test_transposed_center_and_pair_axes_fail(self):
        self.check([[b, a, v] for a, b, v in ROWS], False)

    def test_missing_row_fails(self):
        self.check(ROWS[:-1], False)

    def test_duplicate_key_fails(self):
        self.check([ROWS[0], ROWS[0], *ROWS[2:]], False)

    def test_unknown_center_fails(self):
        self.check([['not-a-spot', 'pair-0', 0.2], *ROWS[1:]], False)

    def test_unknown_pair_fails(self):
        self.check([['spot-0', 'not-a-pair', 0.2], *ROWS[1:]], False)

    def test_incomplete_cartesian_product_fails(self):
        self.check([['spot-0', 'pair-0', 0.2], ['spot-0', 'pair-1', 0.8],
                    ['spot-1', 'pair-0', 0.0], ['spot-2', 'pair-0', 1.0]], False)

    def test_nan_fails(self):
        self.check([['spot-0', 'pair-0', 'nan'], *ROWS[1:]], False)

    def test_inf_fails(self):
        self.check([['spot-0', 'pair-0', 'inf'], *ROWS[1:]], False)

    def test_negative_probability_fails(self):
        self.check([['spot-0', 'pair-0', -1e-15], *ROWS[1:]], False)

    def test_probability_greater_than_one_fails(self):
        self.check([['spot-0', 'pair-0', 1.000000000000001], *ROWS[1:]], False)

    def test_empty_identity_fails(self):
        self.check([['', 'pair-0', 0.2], *ROWS[1:]], False)

    def test_empty_payload_fails(self):
        self.check([], False)

    def test_duplicate_header_fails(self):
        self.check(ROWS, False, ['center', 'center', 'pvalue'])

    def test_pair_only_vector_contract_is_rejected(self):
        self.check([[b, v] for _, b, v in ROWS], False, ['pair', 'pvalue'])

    def test_malformed_numeric_field_fails_with_json(self):
        self.check([['spot-0', 'pair-0', 'not-a-number'], *ROWS[1:]], False)

    def test_invalid_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(ROWS, False)

    def test_nan_tolerance_fails_with_json(self):
        self.rubric.write_text('{"comparison":{"atol":NaN,"rtol":0,"expected_centers":2,"expected_pairs":2}}')
        self.check(ROWS, False)

    def test_negative_tolerance_fails_with_json(self):
        self.rubric.write_text('{"comparison":{"atol":-1,"rtol":0,"expected_centers":2,"expected_pairs":2}}')
        self.check(ROWS, False)

    def test_boolean_expected_count_fails_with_json(self):
        self.rubric.write_text('{"comparison":{"atol":1e-10,"rtol":1e-8,"expected_centers":true,"expected_pairs":2}}')
        self.check(ROWS, False)



    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        rows = [r.copy() for r in ROWS]
        rows[0][2] = min(0.999, rows[0][2] + 0.1)
        self.write(self.ref, rows)
        result = self.check(rows, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'],
                         ['reference', 'candidate'])

    def test_third_leg_covers_every_graded_value(self):
        result = self.check(ROWS, True)
        m = result['measurements']
        self.assertEqual(m['items_with_a_third_leg'], m['graded_items'])
        self.assertIs(m['third_leg_is_partial'], False)

if __name__ == '__main__':
    unittest.main()
