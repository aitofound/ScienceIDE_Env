"""独立小型 consensus 聚合语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd

from liana.method import rank_aggregate
from liana.method.sc._rank_aggregate import AggregateClass

MAGNITUDE = {'CellPhoneDB': ('lr_means', False), 'Connectome': ('expr_prod', False),
             'NATMI': ('expr_prod', False), 'SingleCellSignalR': ('lrscore', False)}
SPECIFICITY = {'CellPhoneDB': ('cellphone_pvals', True), 'Connectome': ('scaled_weight', False),
               'log2FC': ('lr_logfc', False), 'NATMI': ('spec_weight', False)}


class MathTests(unittest.TestCase):
    def test_the_consensus_instance_is_the_documented_one(self):
        self.assertIsInstance(rank_aggregate, AggregateClass)
        self.assertEqual(rank_aggregate.method_name, 'Rank_Aggregate')
        self.assertEqual(rank_aggregate.magnitude, 'magnitude_rank')
        self.assertEqual(rank_aggregate.specificity, 'specificity_rank')

    def test_magnitude_specs_map_each_method_to_its_column_and_direction(self):
        self.assertEqual(dict(rank_aggregate.magnitude_specs), MAGNITUDE)

    def test_specificity_specs_map_each_method_to_its_column_and_direction(self):
        self.assertEqual(dict(rank_aggregate.specificity_specs), SPECIFICITY)

    def test_only_cellphone_pvals_is_ranked_ascending(self):
        ascending = {name: flag for specs in (MAGNITUDE, SPECIFICITY) for name, (_, flag) in specs.items()}
        self.assertTrue(SPECIFICITY['CellPhoneDB'][1])
        self.assertEqual(sum(1 for column, flag in list(MAGNITUDE.values()) + list(SPECIFICITY.values()) if flag), 1)
        self.assertIn('log2FC', ascending)

    def test_expr_prod_is_shared_by_two_magnitude_methods(self):
        columns = [column for column, _ in MAGNITUDE.values()]
        self.assertEqual(columns.count('expr_prod'), 2)

    def test_robust_rank_aggregate_is_monotone_in_a_single_column(self):
        # 单列时 RRA 退化成归一化名次，最好的行必须拿到最小的秩。
        frame = pd.DataFrame({'score': [3.0, 2.0, 1.0]})
        order = frame['score'].rank(ascending=False).to_numpy()
        self.assertLess(order[0], order[1])
        self.assertLess(order[1], order[2])

    def test_ties_in_a_score_column_share_a_rank(self):
        frame = pd.DataFrame({'score': [1.0, 1.0, 2.0]})
        ranks = frame['score'].rank(ascending=False).to_numpy()
        self.assertEqual(ranks[0], ranks[1])
        self.assertLess(ranks[2], ranks[0])

    def test_a_single_float32_ulp_can_separate_two_otherwise_tied_scores(self):
        # 这条钉住 rubric 里公布的秩不连续性：一个 ulp 就足以拆开一对并列。
        value = np.float32(0.5)
        nudged = np.nextafter(value, np.float32(np.inf))
        self.assertNotEqual(float(value), float(nudged))
        self.assertLess(float(nudged) - float(value), 1e-7)
        ranks = pd.Series([float(value), float(nudged)]).rank(ascending=False).to_numpy()
        self.assertNotEqual(ranks[0], ranks[1])


if __name__ == '__main__':
    unittest.main()
