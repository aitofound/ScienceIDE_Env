"""人工小型presence/zero/weight语义自测；不增加graded数据或coverage。"""
import unittest
import numpy as np
from scipy.sparse import csr_matrix
from liana.method.sp._bivariate._local_functions import _vectorized_jaccard


class MathTests(unittest.TestCase):
    def test_presence_is_positive_not_nonzero(self):
        x = np.array([[-1], [1], [0]], dtype=np.float32)
        y = np.array([[1], [1], [-1]], dtype=np.float32)
        actual = _vectorized_jaccard(x, y, csr_matrix(np.ones((3, 3), dtype=np.float32)))
        np.testing.assert_allclose(actual, np.full((3, 1), 0.5), atol=1e-7, rtol=0)

    def test_empty_union_returns_finite_zero(self):
        x = -np.ones((3, 1), dtype=np.float32)
        y = np.zeros((3, 1), dtype=np.float32)
        actual = _vectorized_jaccard(x, y, csr_matrix(np.ones((3, 3), dtype=np.float32)))
        np.testing.assert_array_equal(actual, np.zeros((3, 1)))

    def test_each_center_has_its_own_weights(self):
        x = np.ones((2, 1), dtype=np.float32)
        y = np.array([[1], [0]], dtype=np.float32)
        weight = csr_matrix(np.array([[3, 1], [1, 3]], dtype=np.float32))
        np.testing.assert_allclose(_vectorized_jaccard(x, y, weight), [[0.75], [0.25]], atol=1e-7, rtol=0)


if __name__ == '__main__':
    unittest.main()
