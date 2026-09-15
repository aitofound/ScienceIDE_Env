"""独立小型网格铺排测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd
from anndata import AnnData

from liana.utils import expand_coordinates

# 两个样本，各自两点，范围一目了然：sample_0 占 [0,10]x[0,20]，sample_1 占 [5,7]x[5,9]
COORDS = np.array([[0.0, 0.0], [10.0, 20.0], [5.0, 5.0], [7.0, 9.0]])
SAMPLES = ['sample_0', 'sample_0', 'sample_1', 'sample_1']


def build(coords=COORDS, samples=SAMPLES, sample_key='sample', spatial_key='spatial'):
    obs = pd.DataFrame({sample_key: pd.Categorical(samples)})
    return AnnData(X=np.zeros((len(samples), 3)), obs=obs, obsm={spatial_key: coords.copy()})


class MathTests(unittest.TestCase):
    def test_hand_computed_two_column_layout(self):
        # 每样本先平移到原点：sample_0 -> [[0,0],[10,20]]，sample_1 -> [[0,0],[2,4]]
        # extents = [[10,20],[2,4]]，cell = [10,20]*1.1 = [11,22]；sample_1 在 col=1
        out = np.asarray(expand_coordinates(build(), sample_key='sample', n_cols=2).obsm['spatial'])
        np.testing.assert_allclose(out, [[0, 0], [10, 20], [11, 0], [13, 4]], atol=1e-12, rtol=0)

    def test_margin_zero_packs_cells_edge_to_edge(self):
        out = np.asarray(expand_coordinates(build(), sample_key='sample', n_cols=2, margin=0.0).obsm['spatial'])
        np.testing.assert_allclose(out, [[0, 0], [10, 20], [10, 0], [12, 4]], atol=1e-12, rtol=0)

    def test_single_column_stacks_along_the_second_axis(self):
        out = np.asarray(expand_coordinates(build(), sample_key='sample', n_cols=1, margin=0.0).obsm['spatial'])
        np.testing.assert_allclose(out, [[0, 0], [10, 20], [0, 20], [2, 24]], atol=1e-12, rtol=0)

    def test_default_n_cols_is_ceil_sqrt_of_the_sample_count(self):
        auto = expand_coordinates(build(), sample_key='sample').obsm['spatial']
        two = expand_coordinates(build(), sample_key='sample', n_cols=2).obsm['spatial']
        np.testing.assert_array_equal(np.asarray(auto), np.asarray(two))

    def test_within_a_sample_the_map_is_a_pure_translation(self):
        out = np.asarray(expand_coordinates(build(), sample_key='sample', n_cols=2).obsm['spatial'])
        for lo, hi in [(0, 2), (2, 4)]:
            shift = out[lo:hi] - COORDS[lo:hi]
            np.testing.assert_allclose(shift - shift[0], 0, atol=1e-12, rtol=0)

    def test_input_is_copied_and_original_is_preserved(self):
        adata = build()
        before = adata.obsm['spatial'].copy()
        expanded = expand_coordinates(adata, sample_key='sample', n_cols=2)
        np.testing.assert_array_equal(adata.obsm['spatial'], before)
        self.assertNotIn('spatial_original', adata.obsm)
        np.testing.assert_array_equal(np.asarray(expanded.obsm['spatial_original']), before)

    def test_custom_spatial_key_is_honoured(self):
        adata = build(spatial_key='custom')
        expanded = expand_coordinates(adata, sample_key='sample', spatial_key='custom', n_cols=2)
        self.assertIn('custom_original', expanded.obsm)
        self.assertNotIn('spatial', expanded.obsm)

    def test_missing_keys_are_rejected(self):
        with self.assertRaises(ValueError):
            expand_coordinates(build(), sample_key='not_a_column')
        with self.assertRaises(ValueError):
            expand_coordinates(build(), sample_key='sample', spatial_key='not_a_key')


if __name__ == '__main__':
    unittest.main()
