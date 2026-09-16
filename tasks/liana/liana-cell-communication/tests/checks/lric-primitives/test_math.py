"""独立小型 LRIC helper 语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
from scipy.sparse import csr_matrix
from scipy.spatial import cKDTree

from liana.method.sp._LRIC import (
    _edge_group_bounds,
    _linear_transform,
    _make_radii,
    _support_edge_list,
    _to_dense,
    _type_mean_weights,
)


class MathTests(unittest.TestCase):
    def test_linear_transform_scales_each_column_to_its_own_max(self):
        np.testing.assert_allclose(_linear_transform(np.array([[2.0, 4.0], [2.0, 4.0]])),
                                   np.ones((2, 2)), atol=1e-12, rtol=0)

    def test_linear_transform_never_returns_negatives(self):
        self.assertTrue((_linear_transform(np.array([[0.0, 1.0], [1.0, 1.0]])) >= 0).all())

    def test_linear_transform_of_all_zeros_stays_zero(self):
        self.assertTrue((_linear_transform(np.zeros((3, 2))) == 0).all())

    def test_to_dense_returns_a_float32_ndarray(self):
        data = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float32)
        out = _to_dense(csr_matrix(data))
        self.assertIsInstance(out, np.ndarray)
        np.testing.assert_allclose(out, data, atol=1e-12, rtol=0)
        self.assertEqual(_to_dense(data.astype(np.float64)).dtype, np.float32)

    def test_make_radii_merges_the_contact_band_by_default(self):
        inner, outer = _make_radii(max_radius=100, radius_step=20)
        np.testing.assert_allclose(inner, [0, 40, 60, 80, 100], atol=1e-12, rtol=0)
        np.testing.assert_allclose(outer, [40, 60, 80, 100, 120], atol=1e-12, rtol=0)

    def test_make_radii_can_keep_the_first_annulus_at_one_step(self):
        inner, _ = _make_radii(max_radius=100, radius_step=20, extend_first_annulus=False)
        np.testing.assert_allclose(inner, [20, 40, 60, 80, 100], atol=1e-12, rtol=0)

    def test_annulus_steps_widens_the_outer_edges_but_not_the_inner_ones(self):
        inner, outer = _make_radii(max_radius=100, radius_step=20)
        inner2, outer2 = _make_radii(max_radius=100, radius_step=20, annulus_steps=2)
        np.testing.assert_allclose(inner2, inner, atol=1e-12, rtol=0)
        np.testing.assert_allclose(outer2, np.asarray(outer) + 20, atol=1e-12, rtol=0)

    def test_support_edge_list_excludes_self_pairs_and_bins_half_open(self):
        coords = np.array([[0.0, 0.0], [10.0, 0.0], [20.0, 0.0]])
        I, J, bins = _support_edge_list(cKDTree(coords), np.array([0.0, 15.0]), np.array([15.0, 25.0]))
        self.assertTrue((np.asarray(I) != np.asarray(J)).all())
        seen = dict(zip(zip(I.tolist(), J.tolist()), bins.tolist()))
        self.assertEqual(set(seen), {(0, 1), (1, 0), (1, 2), (2, 1), (0, 2), (2, 0)})
        self.assertEqual(seen[(0, 1)], 0)
        self.assertEqual(seen[(0, 2)], 1)

    def test_edge_group_bounds_gives_cumulative_offsets_including_empty_groups(self):
        np.testing.assert_array_equal(
            _edge_group_bounds(np.array([0, 0, 1, 1, 1, 3]), n_groups=4), [0, 2, 5, 5, 6])

    def test_type_mean_weights_averages_within_each_label(self):
        W = np.array([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [7.0, 8.0]])
        np.testing.assert_allclose(_type_mean_weights(W, np.array(['A', 'A', 'B', 'B']), ['A', 'B']),
                                   [[2.0, 3.0], [6.0, 7.0]], atol=1e-12, rtol=0)


if __name__ == '__main__':
    unittest.main()
