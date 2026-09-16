"""人工全列normalizer/零/tie/scaling测试，不增加graded数据或coverage。"""
import unittest
import numpy as np
from liana.method.sp._bivariate._local_functions import _norm_product


class MathTests(unittest.TestCase):
    def setUp(self):
        self.w = np.array([[1, 2], [3, 0]], dtype=np.float32)
        self.x = np.array([[1], [2]], dtype=np.float32)
        self.y = np.array([[3], [-1]], dtype=np.float32)

    def test_normalize_after_smoothing_across_all_spots(self):
        np.testing.assert_allclose(_norm_product(self.x, self.y, self.w), [[1 / 9], [3 / 5]], atol=1e-7, rtol=0)

    def test_zero_normalizer_uses_one_and_keeps_zero(self):
        np.testing.assert_array_equal(_norm_product(np.zeros_like(self.x), self.y, self.w), [[0], [0]])

    def test_absmax_tie_keeps_sign_without_winner_identity(self):
        x = np.array([[1], [-2]], dtype=np.float32)
        np.testing.assert_allclose(_norm_product(x, self.y, self.w), [[-1 / 9], [1]], atol=1e-7, rtol=0)

    def test_positive_column_rescaling_cancels(self):
        np.testing.assert_allclose(_norm_product(self.x * 3, self.y * 2, self.w), [[1 / 9], [3 / 5]], atol=1e-7, rtol=0)


if __name__ == '__main__':
    unittest.main()
