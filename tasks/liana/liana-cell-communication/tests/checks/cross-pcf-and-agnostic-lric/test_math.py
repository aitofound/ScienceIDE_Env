"""独立小型 cross_pcf / agnostic lric 语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd
from anndata import AnnData
from scipy.sparse import csr_matrix

from liana.method.sp._LRIC import cross_pcf, lric

KWARGS = {'max_radius': 40, 'radius_step': 20, 'verbose': False}


def toy():
    # 两个细胞类型各三个点，摆在一条直线上，距离都是整数
    coords = np.array([[0.0, 0.0], [10.0, 0.0], [20.0, 0.0], [30.0, 0.0], [40.0, 0.0], [50.0, 0.0]])
    counts = np.full((6, 4), 2.0, dtype=np.float32)
    adata = AnnData(csr_matrix(counts), var=pd.DataFrame(index=['La', 'Lb', 'Ra', 'Rb']))
    adata.raw = adata
    adata.obsm['spatial'] = coords
    adata.obs['cell_type'] = pd.Categorical(['A', 'A', 'A', 'B', 'B', 'B'])
    return adata


class MathTests(unittest.TestCase):
    def test_cross_pcf_is_symmetric_so_each_unordered_pair_appears_once(self):
        result = cross_pcf(toy(), groupby='cell_type', inplace=False, min_cells=1, **KWARGS)
        radii = sorted({float(r) for r in result['radius']})
        self.assertEqual(len(result), len(radii))  # 一个无序对 x 每个半径

    def test_cross_pcf_g_is_nonnegative_or_undefined(self):
        values = cross_pcf(toy(), groupby='cell_type', inplace=False, min_cells=1, **KWARGS)['g'].to_numpy()
        self.assertTrue(np.all(np.isnan(values) | (values >= 0)))

    def test_cross_pcf_does_not_read_expression(self):
        a, b = toy(), toy()
        raw = b.raw.X.toarray()
        raw[0, 0] = raw[0, 0] * 3.0
        b.raw = AnnData(csr_matrix(raw), var=b.raw.var.copy())
        left = cross_pcf(a, groupby='cell_type', inplace=False, min_cells=1, **KWARGS)['g'].to_numpy()
        right = cross_pcf(b, groupby='cell_type', inplace=False, min_cells=1, **KWARGS)['g'].to_numpy()
        np.testing.assert_array_equal(np.nan_to_num(left, nan=-1), np.nan_to_num(right, nan=-1))

    def test_min_cells_can_drop_a_cell_type(self):
        loose = cross_pcf(toy(), groupby='cell_type', inplace=False, min_cells=1, **KWARGS)
        strict = cross_pcf(toy(), groupby='cell_type', inplace=False, min_cells=200, **KWARGS)
        self.assertGreater(len(loose), len(strict))

    def test_agnostic_lric_has_no_cell_type_axis(self):
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        result = lric(toy(), resource=resource, inplace=False, **KWARGS)
        self.assertNotIn('source', result.columns)
        self.assertIn('ligand_complex', result.columns)
        self.assertIn('interaction', result.columns)

    def test_agnostic_lric_interaction_is_the_join_of_the_complexes(self):
        resource = pd.DataFrame({'ligand': ['La', 'Lb'], 'receptor': ['Ra', 'Rb']})
        result = lric(toy(), resource=resource, inplace=False, **KWARGS)
        for row in result.itertuples(index=False):
            self.assertEqual(str(row.interaction).split('^')[0], str(row.ligand_complex))
            self.assertEqual(str(row.interaction).split('^')[1], str(row.receptor_complex))

    def test_agnostic_lric_g_is_nonnegative(self):
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        values = lric(toy(), resource=resource, inplace=False, **KWARGS)['g'].to_numpy()
        self.assertTrue(np.all(values >= 0))

    def test_annulus_steps_widens_the_bins(self):
        one = cross_pcf(toy(), groupby='cell_type', annulus_steps=1, inplace=False, min_cells=1, **KWARGS)
        two = cross_pcf(toy(), groupby='cell_type', annulus_steps=2, inplace=False, min_cells=1, **KWARGS)
        self.assertEqual(len(one), len(two))
        self.assertFalse(np.array_equal(np.nan_to_num(one['g'].to_numpy(), nan=-1),
                                        np.nan_to_num(two['g'].to_numpy(), nan=-1)))


if __name__ == '__main__':
    unittest.main()
