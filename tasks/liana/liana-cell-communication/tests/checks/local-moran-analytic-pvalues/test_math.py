"""独立小型 local Moran 解析概率测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
from scipy.sparse import csr_matrix

from liana.method.sp._bivariate._local_functions import LocalFunction

MORANS = LocalFunction._get_instance('morans')


def call(x, y, truth, weight, mask_negatives=True):
    return np.asarray(MORANS._zscore_pvals(x_mat=x, y_mat=y, local_truth=truth,
                                           weight=csr_matrix(weight), mask_negatives=mask_negatives))


class MathTests(unittest.TestCase):
    def test_hand_computed_identity_weight_two_spots(self):
        # n=2, W=I: weight_sq=[1,1], dim=2(n-1)^2/n^2=0.5, MLE sigma=1 scaled by n/(n-1)=2,
        # sigma_prod=4, core=2, var=1*2+2=4, std=2; z=[0,2] -> sf(z).
        x = np.array([[0.0], [2.0]])
        values = call(x, x.copy(), np.array([[0.0], [4.0]]), np.eye(2))
        np.testing.assert_allclose(values, [[0.5], [0.022750131948179195]], atol=1e-14, rtol=0)

    def test_unmasked_branch_is_one_sided_absolute_without_doubling(self):
        # local 的 mask_negatives=False 分支是 sf(|z|)，没有global那样的 *2。
        x = np.array([[0.0], [2.0]])
        values = call(x, x.copy(), np.array([[-4.0], [4.0]]), np.eye(2), mask_negatives=False)
        np.testing.assert_allclose(values, [[0.022750131948179195], [0.022750131948179195]], atol=1e-14, rtol=0)

    def test_masked_branch_keeps_the_negative_tail_above_one_half(self):
        x = np.array([[0.0], [2.0]])
        values = call(x, x.copy(), np.array([[-4.0], [4.0]]), np.eye(2))
        self.assertGreater(values[0, 0], 0.5)
        self.assertLess(values[1, 0], 0.5)

    def test_columns_are_separable_so_block_layout_cannot_change_values(self):
        rng = np.random.default_rng(7)
        x, y, truth = (rng.normal(size=(6, 4)) for _ in range(3))
        weight = rng.uniform(size=(6, 6))
        whole = call(x, y, truth, weight)
        parts = np.concatenate([call(x[:, j:j + 2], y[:, j:j + 2], truth[:, j:j + 2], weight) for j in (0, 2)], axis=1)
        np.testing.assert_array_equal(whole, parts)

    def test_variance_uses_squared_row_sums_so_sign_flips_do_not_matter(self):
        rng = np.random.default_rng(11)
        x, y, truth = (rng.normal(size=(5, 3)) for _ in range(3))
        weight = rng.normal(size=(5, 5))
        np.testing.assert_array_equal(call(x, y, truth, weight), call(x, y, truth, -weight))


if __name__ == '__main__':
    unittest.main()
