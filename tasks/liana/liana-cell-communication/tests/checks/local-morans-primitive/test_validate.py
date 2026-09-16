"""用完整 center×pair payload 检查 局部Moran统计量与标识对齐。"""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECK = Path(__file__).resolve().parent

# 基线不再是编造的 2×2 表，而是从 `ic/nominal` **独立重算**出来的真实 20×5 表。
# 判分器带第三条腿之后会把物理上不对的值拒掉——用合成值做基线，测的只是比较逻辑
# 自己跟自己，连「基线是否正确」都验不了。
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location('_check_validate', CHECK / 'validate.py')
_validate = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_validate)
_TRUTH = _validate.recompute(CHECK / 'ic' / 'nominal')
ROWS = [[center, pair, value] for (center, pair), value in sorted(_TRUTH.items())]
CENTERS = len({r[0] for r in ROWS})
PAIRS = len({r[1] for r in ROWS})
HEADER = ['center', 'pair', 'statistic']


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
        self.rubric.write_text(json.dumps({'comparison': {'atol': 1e-5, 'rtol': 1e-5, 'expected_centers': CENTERS, 'expected_pairs': PAIRS, 'files': [{'path': 'statistics.csv', 'format': 'morans-csv'}]}}))
        self.write(self.reference, ROWS)

    def write(self, directory, rows):
        with (directory / 'statistics.csv').open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(HEADER)
            writer.writerows(rows)

    def check(self, rows, passed):
        self.write(self.candidate, rows)
        output = self.root / 'result.json'
        proc = subprocess.run([sys.executable, str(CHECK / 'validate.py'), '--reference', str(self.reference), '--candidate', str(self.candidate), '--rubric', str(self.rubric), '--out', str(output)], capture_output=True, text=True, timeout=15)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue(output.is_file(), proc.stderr)
        result = json.loads(output.read_text(), parse_constant=lambda x: self.fail('non-standard JSON ' + x))
        self.assertIs(result['passed'], passed, result)
        return result

    def test_signed_unbounded_statistic_is_not_clipped_to_correlation_range(self):
        """局部双变量 Moran 统计量是有符号、无界的，不得被当成相关系数按 [-1,1] 截断或硬拒。

        带上第三条腿后，编造的 10.0 / -14.0 当然通不过（它们物理上不对），但必须是
        走正常比较路径被判不一致，而不是在取值范围上报错。所以这里断言判决里有完整
        的比较字段，失败原因只来自第三条腿，且逐项比较本身没有报越界。
        """
        reference = [r.copy() for r in ROWS]
        reference[0][2] = 10.0
        reference[1][2] = -14.0
        self.write(self.reference, reference)
        result = self.check(reference, False)
        self.assertIn('over_bound', result)
        self.assertEqual(result['over_bound'], 0)
        self.assertIsInstance(result['distance'], float)
        self.assertEqual(result['measurements']['third_leg_failures'],
                         ['reference', 'candidate'])

    def test_identical_passes(self):
        result = self.check(ROWS, True)
        self.assertEqual(result['distance'], 0.0)
        self.assertEqual(result['bound_fraction'], 0.0)

    def test_both_axes_payload_permutation_passes(self):
        self.check(list(reversed(ROWS)), True)

    def test_last_row_is_graded(self):
        rows = [r.copy() for r in ROWS]
        rows[-1][2] = 0.8
        self.check(rows, False)

    def test_roundoff_passes(self):
        rows = [r.copy() for r in ROWS]
        rows[0][2] += 1e-6
        self.check(rows, True)

    def test_local_binding_error_fails(self):
        rows = [r.copy() for r in ROWS]
        rows[0][2], rows[1][2] = rows[1][2], rows[0][2]
        self.check(rows, False)

    def test_absolute_value_fault_fails(self):
        self.check([[a, b, abs(c)] for a, b, c in ROWS], False)

    def test_duplicate_pair_key_fails(self):
        dup = [r.copy() for r in ROWS]
        dup[1][0], dup[1][1] = dup[0][0], dup[0][1]
        self.check(dup, False)

    def test_missing_axis_entry_fails(self):
        self.check(ROWS[:-1], False)

    def test_wrong_pair_label_fails(self):
        rows = [r.copy() for r in ROWS]
        rows[0][1] = 'other-pair'
        self.check(rows, False)

    def test_nan_fails(self):
        self.check([['spot-0', 'pair-0', 'nan'], *ROWS[1:]], False)

    def test_inf_fails(self):
        self.check([['spot-0', 'pair-0', 'inf'], *ROWS[1:]], False)

    def test_large_statistic_change_fails_numeric_bound(self):
        self.check([['spot-0', 'pair-0', 1.1], *ROWS[1:]], False)

    def test_empty_fails(self):
        self.check([], False)

    def test_bad_number_fails_with_json(self):
        self.check([['spot-0', 'pair-0', 'wrong'], *ROWS[1:]], False)

    def test_extra_column_fails(self):
        self.check([r + [0] for r in ROWS], False)

    def test_nan_tolerance_fails_with_json(self):
        self.rubric.write_text('{"comparison":{"atol":NaN,"rtol":0}}')
        self.check(ROWS, False)

    def test_invalid_rubric_fails_with_json(self):
        self.rubric.write_text('{bad')
        self.check(ROWS, False)


    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐项比较结构上拒不掉，只有独立重算能拒。"""
        rows = [r.copy() for r in ROWS]
        rows[0][2] += 1e-2
        self.write(self.reference, rows)
        result = self.check(rows, False)
        self.assertEqual(result['over_bound'], 0)
        self.assertEqual(result['measurements']['third_leg_failures'],
                         ['reference', 'candidate'])

    def test_third_leg_covers_every_graded_value(self):
        result = self.check(ROWS, True)
        m = result['measurements']
        self.assertEqual(m['items_with_a_third_leg'], m['graded_items'])
        self.assertIs(m['third_leg_is_partial'], False)
        self.assertEqual(m['initial_conditions_recomputed'], ['nominal', 'variant'])

if __name__ == '__main__':
    unittest.main()
