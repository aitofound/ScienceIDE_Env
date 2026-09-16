#!/usr/bin/env python3
"""topology-and-coupling 判分器的自测。

写法纪律：**先写结构与断言、常量留空**（下面的 `PROBE` 一开始全是 None），
跑一次一次性探针把它们回填，回填过程记录在
`~/.sciaccel_pipeline/cassiopeia/evidence/topology-and-coupling-*/scratch/`。
这样断言的形状不是照着结果编的。
"""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('tc_validate', HERE / 'validate.py')
V = importlib.util.module_from_spec(spec)
spec.loader.exec_module(V)

CONFIG = json.loads((HERE / 'ic' / 'nominal' / 'inputs.json').read_text())
VARIANT = json.loads((HERE / 'ic' / 'variant' / 'inputs.json').read_text())

# 一次性探针回填的常量。留空跑一遍 → 全红；填上 → 全绿。
PROBE = {
    'choose_nCk_10_2': 45,
    'coalescent_100_2_60': 0.2432488146773861,
    'expansion_ones': {'min_clade_20': 19, 'min_clade_2': 12,
                       'min_depth_3': 16, 'copy_tree': 12},
    'expansion_non_ones_min_clade_2': {'1': 0.3, '2': 0.8, '3': 0.047619047619047616,
                                       '7': 0.5, '8': 0.6, '9': 0.6, '16': 0.6},
    'cophenetic': {'perfect': 1.0, 'weights_W': 1.0, 'default': 0.8193520510633675},
    'node_count': 19,
    # 生产与独立复算在 weights_W 上差 3 ULP（生产 0.9999999999999997）——
    # 这个差**证明第三条腿是独立的**，不是把生产抄了一遍。
    'production_weights_W': 0.9999999999999997,
}
TOLERANCE = {'atol': 1e-12, 'rtol': 0.0}


def synthetic(config=None, mutate=None):
    """从独立复算构造一份"正确"的产物；mutate 用来做各类负例。"""
    config = config or CONFIG
    expected = V.expected_tables(config)
    arrays = {
        'choose.ids': np.asarray(sorted(expected['choose']), dtype=str),
        'choose.values': np.asarray([expected['choose'][k]
                                     for k in sorted(expected['choose'])], dtype=np.int64),
        'coalescent.ids': np.asarray(sorted(expected['coalescent']), dtype=str),
        'coalescent.values': np.asarray([expected['coalescent'][k]
                                         for k in sorted(expected['coalescent'])],
                                        dtype=np.float64),
        'cophenetic.ids': np.asarray(sorted(expected['cophenetic']), dtype=str),
        'cophenetic.correlation': np.asarray([expected['cophenetic'][k]
                                              for k in sorted(expected['cophenetic'])],
                                             dtype=np.float64),
        'cophenetic.significance': np.asarray([0.0 for _ in expected['cophenetic']],
                                              dtype=np.float64),
        'errors.ids': np.asarray(sorted(expected['errors']), dtype=str),
        'errors.exception': np.asarray([expected['errors'][k]
                                        for k in sorted(expected['errors'])], dtype=str),
        'ic.name': np.asarray([config['ic_name']], dtype=str),
    }
    for case, table in expected['expansion'].items():
        nodes = sorted(table)
        arrays[f'expansion.{case}.nodes'] = np.asarray(nodes, dtype=str)
        arrays[f'expansion.{case}.pvalues'] = np.asarray([table[n] for n in nodes],
                                                         dtype=np.float64)
    if mutate:
        mutate(arrays)
    return arrays


def judge(reference_arrays, candidate_arrays, tolerance=None):
    tolerance = tolerance or TOLERANCE
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        for name, arrays in (('reference', reference_arrays), ('candidate', candidate_arrays)):
            (work / name).mkdir()
            np.savez(work / name / 'results.npz', **arrays)
        rubric = {'comparison': {'atol': tolerance['atol'], 'rtol': tolerance['rtol'],
                                 'initial_conditions': ['nominal', 'variant']}}
        (work / 'rubric.json').write_text(json.dumps(rubric))
        out = work / 'result.json'
        V.main(['--reference', str(work / 'reference'), '--candidate', str(work / 'candidate'),
                '--rubric', str(work / 'rubric.json'), '--out', str(out)])
        return json.loads(out.read_text())


class TestIndependentRecomputation(unittest.TestCase):
    """第三条腿本身是否算对——这些断言不看生产，只看闭式。"""

    def setUp(self):
        self.expected = V.expected_tables(CONFIG)

    def test_nCk_matches_probe(self):
        self.assertEqual(self.expected['choose']['nCk_10_2'], PROBE['choose_nCk_10_2'])

    def test_nCk_rejects_k_greater_than_n(self):
        with self.assertRaises(ValueError):
            V.nCk(5, 7)

    def test_coalescent_matches_probe(self):
        self.assertEqual(self.expected['coalescent']['coalescent_100_2_60'],
                         PROBE['coalescent_100_2_60'])

    def test_expansion_saturation_counts(self):
        """受判面高度饱和，这条把饱和度钉成一个会红的数字，而不是散文。"""
        for case, ones in PROBE['expansion_ones'].items():
            table = self.expected['expansion'][case]
            self.assertEqual(sum(1 for v in table.values() if v == 1.0), ones, case)
            self.assertEqual(len(table), PROBE['node_count'], case)

    def test_expansion_non_one_values(self):
        table = self.expected['expansion']['min_clade_2']
        self.assertEqual({n: v for n, v in table.items() if v != 1.0},
                         PROBE['expansion_non_ones_min_clade_2'])

    def test_copy_tree_table_equals_min_clade_2(self):
        """copy_tree 与 min_clade_2 的 pvalue 表逐位相同——它多出来的信息只在异常场景里。"""
        self.assertEqual(self.expected['expansion']['copy_tree'],
                         self.expected['expansion']['min_clade_2'])

    def test_cophenetic_matches_probe(self):
        for name, value in PROBE['cophenetic'].items():
            self.assertAlmostEqual(self.expected['cophenetic'][name], value, delta=1e-15)

    def test_min_depth_3_is_a_subset_of_min_clade_2(self):
        """warrant「min_depth 只会把浅层的 pvalue 抬回 1.0」的 assert。"""
        loose = self.expected['expansion']['min_clade_2']
        tight = self.expected['expansion']['min_depth_3']
        for node, value in tight.items():
            self.assertTrue(value == 1.0 or value == loose[node], node)


class TestVerdictShape(unittest.TestCase):
    def test_identical_sides_pass_with_zero_distance(self):
        verdict = judge(synthetic(), synthetic())
        self.assertTrue(verdict['passed'], verdict['reason'])
        self.assertEqual(verdict['distance'], 0.0)
        self.assertEqual(verdict['policy'], 'pointwise')

    def test_reference_against_itself_passes(self):
        arrays = synthetic()
        verdict = judge(arrays, arrays)
        self.assertTrue(verdict['passed'])

    def test_three_legs_are_reported_for_recomputable_quantities(self):
        verdict = judge(synthetic(), synthetic())
        legs = verdict['measurements']['coalescent/coalescent_100_2_60']
        self.assertEqual(set(legs), {'candidate_vs_reference', 'reference_vs_recomputation',
                                     'candidate_vs_recomputation'})

    def test_significance_has_only_two_legs_and_says_so(self):
        """已披露的盲区，用一条测试钉住它，免得下次被当成已覆盖。"""
        verdict = judge(synthetic(), synthetic())
        legs = verdict['measurements']['cophenetic/default/significance']
        self.assertEqual(set(legs), {'candidate_vs_reference'})

    def test_both_sides_wrong_the_same_way_is_caught_by_the_third_leg(self):
        def bump(arrays):
            arrays['coalescent.values'] = arrays['coalescent.values'] + 0.5
        verdict = judge(synthetic(mutate=bump), synthetic(mutate=bump))
        self.assertFalse(verdict['passed'])
        self.assertIn('reference_vs_recomputation', verdict['reason'])

    def test_both_sides_wrong_on_significance_is_NOT_caught(self):
        """反向对照：没有第三条腿的那一格，双侧同错必然通过。这条断言的是那个事实。"""
        def bump(arrays):
            arrays['cophenetic.significance'] = arrays['cophenetic.significance'] + 0.25
        verdict = judge(synthetic(mutate=bump), synthetic(mutate=bump))
        self.assertTrue(verdict['passed'], verdict['reason'])


class TestScientificMismatch(unittest.TestCase):
    def test_single_pvalue_off_by_more_than_bound_fails(self):
        def bump(arrays):
            arrays['expansion.min_clade_2.pvalues'][0] += 1e-9
        verdict = judge(synthetic(), synthetic(mutate=bump))
        self.assertFalse(verdict['passed'])
        self.assertIn('expansion/min_clade_2', verdict['reason'])

    def test_pvalue_off_by_less_than_bound_passes(self):
        def bump(arrays):
            arrays['expansion.min_clade_2.pvalues'][0] += 1e-15
        verdict = judge(synthetic(), synthetic(mutate=bump))
        self.assertTrue(verdict['passed'], verdict['reason'])

    def test_choose_is_exact_and_one_off_fails(self):
        def bump(arrays):
            arrays['choose.values'] = arrays['choose.values'] + 1
        verdict = judge(synthetic(), synthetic(mutate=bump))
        self.assertFalse(verdict['passed'])
        self.assertIn('choose/', verdict['reason'])

    def test_wrong_exception_type_fails(self):
        def swap(arrays):
            arrays['errors.exception'] = np.asarray(['ValueError'] * len(arrays['errors.ids']),
                                                    dtype=str)
        verdict = judge(synthetic(), synthetic(mutate=swap))
        self.assertFalse(verdict['passed'])
        self.assertIn('errors/', verdict['reason'])

    def test_no_exception_raised_fails(self):
        def swap(arrays):
            arrays['errors.exception'] = np.asarray(['no-exception'] * len(arrays['errors.ids']),
                                                    dtype=str)
        verdict = judge(synthetic(), synthetic(mutate=swap))
        self.assertFalse(verdict['passed'])

    def test_cophenetic_correlation_shift_fails(self):
        def bump(arrays):
            arrays['cophenetic.correlation'][-1] += 1e-6
        verdict = judge(synthetic(), synthetic(mutate=bump))
        self.assertFalse(verdict['passed'])
        self.assertIn('cophenetic/', verdict['reason'])

    def test_significance_out_of_range_fails(self):
        def bad(arrays):
            arrays['cophenetic.significance'][0] = 1.5
        verdict = judge(synthetic(), synthetic(mutate=bad))
        self.assertFalse(verdict['passed'])
        self.assertIn('out_of_range', verdict['reason'])


class TestContractFailure(unittest.TestCase):
    """(a) 类：固定名册。缺/多/重复是合同失败——异常 + error_type + distance=None。"""

    def assertContract(self, verdict):
        self.assertFalse(verdict['passed'])
        self.assertIsNone(verdict['distance'])
        self.assertIn('error_type', verdict)

    def test_missing_field_is_contract_failure(self):
        def drop(arrays):
            arrays.pop('choose.values')
        self.assertContract(judge(synthetic(), synthetic(mutate=drop)))

    def test_extra_field_is_contract_failure(self):
        def add(arrays):
            arrays['unexpected'] = np.asarray([1], dtype=np.int64)
        self.assertContract(judge(synthetic(), synthetic(mutate=add)))

    def test_missing_node_is_contract_failure(self):
        def drop(arrays):
            arrays['expansion.min_clade_2.nodes'] = arrays['expansion.min_clade_2.nodes'][:-1]
            arrays['expansion.min_clade_2.pvalues'] = arrays['expansion.min_clade_2.pvalues'][:-1]
        self.assertContract(judge(synthetic(), synthetic(mutate=drop)))

    def test_duplicate_node_is_contract_failure(self):
        def duplicate(arrays):
            nodes = arrays['expansion.min_clade_2.nodes'].tolist()
            nodes[-1] = nodes[0]
            arrays['expansion.min_clade_2.nodes'] = np.asarray(nodes, dtype=str)
        self.assertContract(judge(synthetic(), synthetic(mutate=duplicate)))

    def test_wrong_dtype_is_contract_failure(self):
        def widen(arrays):
            arrays['coalescent.values'] = arrays['coalescent.values'].astype(np.float32)
        self.assertContract(judge(synthetic(), synthetic(mutate=widen)))

    def test_nan_is_contract_failure(self):
        def poison(arrays):
            arrays['cophenetic.correlation'][0] = np.nan
        self.assertContract(judge(synthetic(), synthetic(mutate=poison)))

    def test_reference_side_damage_is_also_contract_failure(self):
        def drop(arrays):
            arrays.pop('errors.ids')
        self.assertContract(judge(synthetic(mutate=drop), synthetic()))

    def test_zero_tolerance_is_rejected(self):
        verdict = judge(synthetic(), synthetic(), {'atol': 0.0, 'rtol': 0.0})
        self.assertContract(verdict)


class TestDeclaredInitialCondition(unittest.TestCase):
    """两侧必须声明同一个 IC；声明的 IC 决定第三条腿用哪份固定输入。"""

    def test_sides_declaring_different_ics_is_legal_and_judged_per_side(self):
        """两侧声明不同 IC 是**合法**的，不是合同失败。

        `sab.py task selfcheck` 的计分形态就是 `reference=oracle-nominal` 对
        `candidate=oracle-variant`，跨 IC 是常态。本用例原先断言「不同 IC 必须失败」，
        对应 validate.py 里一处已修掉的缺陷（它当时直接
        `raise ValueError('两侧声明了不同的初始条件')`）——那样写会让本 check 在自己的
        selfcheck 里必然失败。修法是**逐侧**判定：每一侧各自对照它所声明那个 IC 的期望。
        这条用例随之改写成断言修好之后的行为。
        """
        def relabel(arrays):
            arrays['ic.name'] = np.asarray(['variant'], dtype=str)
        verdict = judge(synthetic(), synthetic(mutate=relabel))
        self.assertNotIn('error_type', verdict,
                         '跨 IC 不应再走异常路径')
        self.assertIsNotNone(verdict['distance'],
                             '跨 IC 时仍应给出科学距离，而不是 None')
        # 实测：合成夹具在两个 IC 下都满足独立重算，所以跨 IC **通过**，
        # distance 是 2.2e-16 的机器噪声。这正是修好之后该有的行为。
        self.assertTrue(verdict['passed'], verdict.get('reason'))
        self.assertNotIn('初始条件', verdict.get('reason', ''),
                         '不应再出现「两侧声明了不同的初始条件」这条已删掉的合同规则')

    def test_undeclared_ic_is_contract_failure(self):
        def relabel(arrays):
            arrays['ic.name'] = np.asarray(['made-up'], dtype=str)
        verdict = judge(synthetic(mutate=relabel), synthetic(mutate=relabel))
        self.assertFalse(verdict['passed'])
        self.assertIsNone(verdict['distance'])

    def test_variant_artifacts_are_graded_against_the_variant_recomputation(self):
        """若判分器固定用 nominal 复算，这条会红——它钉住的正是那个错误。"""
        arrays = synthetic(config=VARIANT)
        verdict = judge(arrays, arrays)
        self.assertTrue(verdict['passed'], verdict['reason'])
        self.assertEqual(verdict['distance'], 0.0)

    def test_mislabelled_ic_is_NOT_caught_at_this_bound(self):
        """把 variant 的数值贴上 nominal 的标签——**判分器抓不住**，这条断言那个事实。

        实测 2-ULP IC 扰动在受判量上只走到 `cophenetic/perfect` 的 2.22e-16，
        其余受判量逐位不变；而 bound 是 1e-12。**所以 IC 标签在数值上不可验证**，
        它只是用来选对复算输入的声明，不是一道防线。写下来免得被当成已覆盖。
        """
        arrays = synthetic(config=VARIANT)
        arrays['ic.name'] = np.asarray(['nominal'], dtype=str)
        verdict = judge(arrays, arrays)
        self.assertTrue(verdict['passed'], verdict['reason'])
        self.assertLess(verdict['distance'], 1e-15)


class TestNodeOrderIsNotGraded(unittest.TestCase):
    def test_permuted_node_order_still_passes(self):
        """上游 assertListEqual 式的顺序约束比科学更宽，本 check 主动放松。"""
        def permute(arrays):
            order = np.argsort(arrays['expansion.min_clade_2.nodes'])[::-1]
            arrays['expansion.min_clade_2.nodes'] = arrays['expansion.min_clade_2.nodes'][order]
            arrays['expansion.min_clade_2.pvalues'] = arrays['expansion.min_clade_2.pvalues'][order]
        verdict = judge(synthetic(), synthetic(mutate=permute))
        self.assertTrue(verdict['passed'], verdict['reason'])


class TestVariantIsAMeaningfulPerturbation(unittest.TestCase):
    def test_variant_moves_only_the_matrix_backed_quantities(self):
        """variant 只扰动两张浮点矩阵；纯拓扑派生的量必须逐位不变。

        这同时是"扰动确实打进去了"的两端确认——若 perfect/weights_W 也不动，
        说明 variant 没有进入任何受判量。
        """
        nominal = V.expected_tables(CONFIG)
        variant = V.expected_tables(VARIANT)
        self.assertEqual(nominal['choose'], variant['choose'])
        self.assertEqual(nominal['coalescent'], variant['coalescent'])
        self.assertEqual(nominal['expansion'], variant['expansion'])
        self.assertEqual(nominal['cophenetic']['default'], variant['cophenetic']['default'])
        moved = [name for name in ('perfect', 'weights_W')
                 if nominal['cophenetic'][name] != variant['cophenetic'][name]]
        self.assertNotEqual(moved, [], '2-ULP 扰动没有到达任何受判量')


class TestCliBoundary(unittest.TestCase):
    def test_cli_writes_strict_json_and_exits_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            for name in ('reference', 'candidate'):
                (work / name).mkdir()
                np.savez(work / name / 'results.npz', **synthetic())
            (work / 'rubric.json').write_text(json.dumps(
                {'comparison': {'atol': 1e-12, 'rtol': 0.0,
                                'initial_conditions': ['nominal', 'variant']}}))
            out = work / 'r.json'
            proc = subprocess.run([sys.executable, str(HERE / 'validate.py'),
                                   '--reference', str(work / 'reference'),
                                   '--candidate', str(work / 'candidate'),
                                   '--rubric', str(work / 'rubric.json'),
                                   '--out', str(out)], capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr[-400:])
            payload = out.read_bytes()
            self.assertTrue(payload.endswith(b'\n'))
            payload.decode('ascii')
            self.assertTrue(json.loads(payload)['passed'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
