"""独立小型空间核测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
from anndata import AnnData

from liana.utils.spatial_neighbors import spatial_neighbors


def build(coordinates):
    adata = AnnData(np.zeros((len(coordinates), 2), dtype=np.float32))
    adata.obsm['spatial'] = np.asarray(coordinates, dtype=np.float64)
    return adata


def connectivities(coordinates, **keywords):
    return np.asarray(spatial_neighbors(adata=build(coordinates), inplace=False, **keywords).todense())


LINE = [[0.0, 0.0], [100.0, 0.0], [300.0, 0.0]]


class MathTests(unittest.TestCase):
    def test_gaussian_is_exp_minus_d2_over_two_b2(self):
        values = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=True, max_neighbours=2)
        self.assertAlmostEqual(values[0, 0], 1.0, places=15)
        self.assertAlmostEqual(values[0, 1], np.exp(-(100.0 ** 2) / (2 * 100.0 ** 2)), places=15)

    def test_misty_rbf_drops_the_factor_two_in_the_denominator(self):
        values = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=True, max_neighbours=2, kernel='misty_rbf')
        self.assertAlmostEqual(values[0, 1], np.exp(-(100.0 ** 2) / (100.0 ** 2)), places=15)

    def test_exponential_uses_the_raw_distance(self):
        values = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=True, max_neighbours=2, kernel='exponential')
        self.assertAlmostEqual(values[0, 1], np.exp(-1.0), places=15)

    def test_linear_is_clipped_at_zero_beyond_the_bandwidth(self):
        values = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=True, max_neighbours=2, kernel='linear')
        self.assertAlmostEqual(values[0, 1], 0.0, places=15)
        self.assertEqual(values[0, 2], 0.0)

    def test_cutoff_zeroes_the_value_but_keeps_the_stored_slot(self):
        loose = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=True, max_neighbours=2)
        tight = connectivities(LINE, bandwidth=100, cutoff=0.7, set_diag=True, max_neighbours=2)
        self.assertGreater(loose[0, 1], 0.0)
        self.assertEqual(tight[0, 1], 0.0)
        self.assertEqual(tight[0, 0], 1.0)

    def test_set_diag_false_clears_the_self_weight(self):
        values = connectivities(LINE, bandwidth=100, cutoff=0.0, set_diag=False, max_neighbours=2)
        self.assertEqual(values[0, 0], 0.0)

    def test_standardize_makes_every_nonempty_row_sum_to_one(self):
        values = connectivities(LINE, bandwidth=200, cutoff=0.1, set_diag=True, max_neighbours=2, standardize=True)
        np.testing.assert_allclose(values.sum(axis=1), np.ones(len(LINE)), atol=1e-14, rtol=0)

    def test_unknown_kernel_is_rejected(self):
        with self.assertRaises(AssertionError):
            connectivities(LINE, bandwidth=100, cutoff=0.0, kernel='not-a-kernel')

    def test_missing_bandwidth_is_rejected(self):
        with self.assertRaises(ValueError):
            connectivities(LINE, cutoff=0.0)


if __name__ == '__main__':
    unittest.main()
