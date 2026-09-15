"""人工小型置换 p 值 payload 自测，不读取真实nominal也不依赖LIANA。"""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECK = Path(__file__).resolve().parent
HEADER = ['center', 'pair', 'pvalue', 'count']
ROWS = [['spot-0', 'pair-0', 0.25, 25], ['spot-0', 'pair-1', 0.0, 0],
        ['spot-1', 'pair-0', 1.0, 100], ['spot-1', 'pair-1', 0.5, 50]]
COMPARISON = {'atol': 1e-12, 'rtol': 0.0, 'expected_centers': 2, 'expected_pairs': 2, 'n_perms': 100,
              'files': [{'path': 'pvalues.csv'}]}


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
        payload = out.read_bytes()
        self.assertTrue(payload.isascii())
        record = json.loads(payload, parse_constant=lambda s: self.fail('非标准JSON ' + s))
        self.assertIs(record['passed'], passed, record)
        return record

    def test_identical_tables_pass_and_count_every_value(self):
        result = self.check(ROWS, True)
        self.assertEqual(result['distance'], 0)
        self.assertEqual(result['values'], 4)
        self.assertEqual(result['n_perms'], 100)

    def test_row_permutation_passes(self):
        self.check(list(reversed(ROWS)), True)

    def test_column_order_is_not_science(self):
        self.check([[d, c, b, a] for a, b, c, d in ROWS], True, list(reversed(HEADER)))

    def test_zero_and_one_boundaries_are_legal(self):
        self.check(ROWS, True)

    def test_representation_roundoff_below_the_grid_passes(self):
        self.check([['spot-0', 'pair-0', 0.25 + 1e-15, 25], *ROWS[1:]], True)

    def test_one_grid_step_off_fails(self):
        result = self.check([['spot-0', 'pair-0', 0.26, 26], *ROWS[1:]], False)
        self.assertGreater(result['bound_fraction'], 1)

    def test_pvalue_not_on_the_grid_fails(self):
        self.check([['spot-0', 'pair-0', 0.253, 25], *ROWS[1:]], False)

    def test_pvalue_and_count_disagreeing_within_a_file_fails(self):
        self.check([['spot-0', 'pair-0', 0.25, 30], *ROWS[1:]], False)

    def test_a_one_step_count_difference_shows_up_numerically(self):
        result = self.check([['spot-0', 'pair-0', 0.26, 26], *ROWS[1:]], False)
        self.assertEqual(result['counts_differing'], 1)
        self.assertGreater(result['distance'], 0.009)

    def test_count_above_n_perms_fails(self):
        self.check([['spot-0', 'pair-0', 1.01, 101], *ROWS[1:]], False)

    def test_negative_count_fails(self):
        self.check([['spot-0', 'pair-0', -0.25, -25], *ROWS[1:]], False)

    def test_pvalue_outside_unit_interval_fails(self):
        self.check([['spot-0', 'pair-0', 1.5, 150], *ROWS[1:]], False)

    def test_center_pair_binding_error_fails(self):
        self.check([['spot-0', 'pair-0', 0.0, 0], ['spot-0', 'pair-1', 0.25, 25], *ROWS[2:]], False)

    def test_transposed_axes_fail(self):
        self.check([[b, a, v, c] for a, b, v, c in ROWS], False)

    def test_missing_row_fails(self):
        self.check(ROWS[:-1], False)

    def test_duplicate_key_fails(self):
        self.check([ROWS[0], ROWS[0], *ROWS[2:]], False)

    def test_unknown_center_fails(self):
        self.check([['spot-9', 'pair-0', 0.25, 25], *ROWS[1:]], False)

    def test_incomplete_cartesian_product_fails(self):
        self.check([['spot-0', 'pair-0', 0.25, 25], ['spot-0', 'pair-1', 0.0, 0],
                    ['spot-1', 'pair-0', 1.0, 100], ['spot-2', 'pair-0', 0.5, 50]], False)

    def test_nan_fails(self):
        self.check([['spot-0', 'pair-0', 'nan', 25], *ROWS[1:]], False)

    def test_inf_fails(self):
        self.check([['spot-0', 'pair-0', 'inf', 25], *ROWS[1:]], False)

    def test_non_integer_count_fails(self):
        self.check([['spot-0', 'pair-0', 0.25, '25.0'], *ROWS[1:]], False)

    def test_empty_identity_fails(self):
        self.check([['', 'pair-0', 0.25, 25], *ROWS[1:]], False)

    def test_empty_payload_fails(self):
        self.check([], False)

    def test_duplicate_header_fails(self):
        self.check(ROWS, False, ['center', 'center', 'pvalue', 'count'])

    def test_malformed_numeric_field_fails_with_json(self):
        self.check([['spot-0', 'pair-0', 'x', 25], *ROWS[1:]], False)

    def test_invalid_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(ROWS, False)

    def broken(self, **overrides):
        comparison = dict(COMPARISON)
        comparison.update(overrides)
        self.rubric.write_text(json.dumps({'comparison': comparison}))

    def test_nan_tolerance_fails_with_json(self):
        self.rubric.write_text('{"comparison":{"atol":NaN,"rtol":0,"expected_centers":2,"expected_pairs":2,"n_perms":100}}')
        self.check(ROWS, False)

    def test_negative_tolerance_fails_with_json(self):
        self.broken(atol=-1)
        self.check(ROWS, False)

    def test_atol_reaching_the_grid_step_is_rejected(self):
        # 容差一旦达到 1/n_perms 就等于不评分，validator 必须拒绝这样的 rubric
        self.broken(atol=0.01)
        self.check(ROWS, False)

    def test_wrong_expected_counts_fail_with_json(self):
        self.broken(expected_centers=3)
        self.check(ROWS, False)

    def test_boolean_n_perms_fails_with_json(self):
        self.broken(n_perms=True)
        self.check(ROWS, False)


if __name__ == '__main__':
    unittest.main()
