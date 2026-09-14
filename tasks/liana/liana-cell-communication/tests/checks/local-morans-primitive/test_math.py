"""人工小型两项乘积和/符号/零输入自测；不增加graded数据或coverage。"""
import unittest
import numpy as np
from scipy.sparse import csr_matrix
from liana.method.sp._bivariate._local_functions import _local_morans


class MathTests(unittest.TestCase):
    def test_two_terms_keep_unbounded_signed_values(self):
        y = np.array([[3], [-1]], dtype=np.float32)
        weight = csr_matrix(np.array([[0, 2], [1, 0]], dtype=np.float32))
        for x, expected in [([[1], [2]], [[10], [5]]), ([[1], [-2]], [[-14], [-7]])]:
            with self.subTest(x=x):
                np.testing.assert_array_equal(_local_morans(np.array(x, dtype=np.float32), y, weight), expected)

    def test_swapping_x_and_y_preserves_this_symmetric_formula(self):
        x = np.array([[1], [2]], dtype=np.float32)
        y = np.array([[3], [-1]], dtype=np.float32)
        weight = csr_matrix(np.array([[0, 2], [1, 0]], dtype=np.float32))
        np.testing.assert_array_equal(_local_morans(y, x, weight), [[10], [5]])

    def test_zero_x_zeroes_both_products(self):
        x = np.zeros((2, 1), dtype=np.float32)
        y = np.array([[3], [-1]], dtype=np.float32)
        weight = csr_matrix(np.array([[0, 2], [1, 0]], dtype=np.float32))
        np.testing.assert_array_equal(_local_morans(x, y, weight), np.zeros((2, 1)))


if __name__ == '__main__':
    unittest.main()
