"""独立小型 g(r) 曲线摘要测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd

from liana.utils import get_lric_auc, get_lric_divergence


def curves(mapping, radii=(0.0, 1.0, 2.0)):
    rows = []
    for interaction, values in mapping.items():
        for radius, g in zip(radii, values):
            rows.append({'interaction': interaction, 'radius': float(radius), 'g': float(g)})
    return pd.DataFrame(rows)


class MathTests(unittest.TestCase):
    def test_flat_unit_curve_scores_zero(self):
        out = get_lric_auc(liana_res=curves({'a^b': [1.0, 1.0, 1.0]}), min_bins=3)
        self.assertAlmostEqual(float(out['score'].iloc[0]), 0.0, places=12)

    def test_score_is_the_span_normalised_mean_log2(self):
        # log2 g = [1, 1, 2]；梯形面积 = 1*1 + 1.5*1 = 2.5，跨度 2 -> 1.25
        out = get_lric_auc(liana_res=curves({'a^b': [2.0, 2.0, 4.0]}), min_bins=3)
        self.assertAlmostEqual(float(out['score'].iloc[0]), 1.25, places=12)

    def test_peak_radius_is_where_the_absolute_deviation_is_largest(self):
        out = get_lric_auc(liana_res=curves({'a^b': [1.0, 0.5, 4.0]}), min_bins=3)
        self.assertEqual(float(out['peak_radius'].iloc[0]), 2.0)

    def test_default_transform_floors_g_at_five_hundredths(self):
        out = get_lric_auc(liana_res=curves({'a^b': [0.0, 0.0, 0.0]}), min_bins=3)
        self.assertAlmostEqual(float(out['score'].iloc[0]), float(np.log2(0.05)), places=12)

    def test_strict_log2_drops_the_zero_bin_below_min_bins(self):
        frame = curves({'a^b': [0.0, 2.0, 2.0]})
        self.assertEqual(len(get_lric_auc(liana_res=frame, min_bins=3)), 1)
        self.assertEqual(len(get_lric_auc(liana_res=frame, min_bins=3, transform_fn=np.log2)), 0)

    def test_max_dist_restricts_the_integration_window(self):
        frame = curves({'a^b': [2.0, 2.0, 8.0]})
        whole = float(get_lric_auc(liana_res=frame, min_bins=2)['score'].iloc[0])
        window = float(get_lric_auc(liana_res=frame, min_bins=2, max_dist=2.0)['score'].iloc[0])
        self.assertAlmostEqual(window, 1.0, places=12)
        self.assertGreater(whole, window)

    def test_results_are_sorted_most_enriched_first(self):
        out = get_lric_auc(liana_res=curves({'low^x': [1.0, 1.0, 1.0], 'high^x': [4.0, 4.0, 4.0]}), min_bins=3)
        self.assertEqual(list(out['interaction']), ['high^x', 'low^x'])

    def test_divergence_of_a_curve_against_itself_is_exactly_zero(self):
        frame = curves({'a^b': [1.0, 2.0, 4.0]})
        record = get_lric_divergence(liana_res=frame, feature_a={'interaction': 'a^b'},
                                     feature_b={'interaction': 'a^b'}, min_bins=2)
        self.assertEqual(float(record['divergence']), 0.0)
        self.assertEqual(record['direction'], 'equal')

    def test_a_doubled_curve_is_exactly_one_log2_apart(self):
        frame = curves({'a^b': [1.0, 1.0, 1.0], 'c^d': [2.0, 2.0, 2.0]})
        record = get_lric_divergence(liana_res=frame, feature_a={'interaction': 'c^d'},
                                     feature_b={'interaction': 'a^b'}, min_bins=2, transform_fn=np.log2)
        self.assertAlmostEqual(float(record['divergence']), 1.0, places=12)
        self.assertAlmostEqual(float(record['delta_star']), 1.0, places=12)
        self.assertEqual(record['direction'], 'A > B')

    def test_missing_selection_and_unmatched_rows_are_rejected(self):
        frame = curves({'a^b': [1.0, 1.0, 1.0]})
        with self.assertRaises(ValueError):
            get_lric_divergence(liana_res=frame, feature_a={'interaction': 'a^b'})
        with self.assertRaises(ValueError):
            get_lric_divergence(liana_res=frame, feature_a={'interaction': 'nope'},
                                feature_b={'interaction': 'a^b'}, min_bins=2)

    def test_too_few_shared_bins_is_rejected(self):
        frame = curves({'a^b': [1.0, 1.0, 1.0], 'c^d': [2.0, 2.0, 2.0]})
        with self.assertRaises(ValueError):
            get_lric_divergence(liana_res=frame, feature_a={'interaction': 'a^b'},
                                feature_b={'interaction': 'c^d'}, min_bins=99)


if __name__ == '__main__':
    unittest.main()
