"""独立小型给定统计量→解析概率测试；不算经验null或额外graded覆盖。"""
import unittest
import numpy as np
from scipy.sparse import csr_matrix
from liana.method.sp._bivariate._global_functions import _global_r


class MathTests(unittest.TestCase):
    def test_one_sided_sf_with_three_pairs_but_two_weight_spots(self):
        w = csr_matrix(np.eye(2, dtype=np.float64))
        values = _global_r._zscore_pvals(weight=w, global_stat=np.array([0.0, 1.0, -1.0]), mask_negatives=True)
        np.testing.assert_allclose(values, [0.5, 0.15865525393145707, 0.8413447460685429], atol=1e-14, rtol=0)

    def test_two_sided_branch_is_development_only(self):
        w = csr_matrix(np.eye(2, dtype=np.float64))
        values = _global_r._zscore_pvals(weight=w, global_stat=np.array([0.0, 1.0, -1.0]), mask_negatives=False)
        np.testing.assert_allclose(values, [1.0, 0.31731050786291415, 0.31731050786291415], atol=1e-14, rtol=0)

    def test_directed_weight_square_is_not_transpose_product(self):
        w = csr_matrix(np.array([[0.0, 1.0], [2.0, 0.0]]))
        # 手算variance=(20-16+9)/4=13/4，z分别为0和1。
        values = _global_r._zscore_pvals(weight=w, global_stat=np.array([0.0, np.sqrt(13) / 2]), mask_negatives=True)
        np.testing.assert_allclose(values, [0.5, 0.15865525393145707], atol=1e-14, rtol=0)

    def test_representable_extreme_tail_zero_and_one_are_valid(self):
        values = _global_r._zscore_pvals(weight=csr_matrix(np.eye(2)), global_stat=np.array([100.0, -100.0]), mask_negatives=True)
        np.testing.assert_array_equal(values, [0.0, 1.0])


if __name__ == '__main__':
    unittest.main()
