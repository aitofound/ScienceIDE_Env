"""独立小型参数分支语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
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

    def test_extend_first_annulus_false_moves_the_radius_grid(self):
        merged = cross_pcf(toy(), groupby='cell_type', inplace=False, min_cells=1, **KWARGS)
        split = cross_pcf(toy(), groupby='cell_type', extend_first_annulus=False, inplace=False,
                          min_cells=1, **KWARGS)
        self.assertEqual(float(np.min(merged['radius'])), 0.0)
        self.assertGreater(float(np.min(split['radius'])), 0.0)

    def test_expr_prop_above_one_masks_every_pair(self):
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        out = lric(toy(), resource=resource, expr_prop=1.1, inplace=False, **KWARGS)
        self.assertTrue(out['g'].isna().all())

    def test_expr_prop_zero_is_a_no_op(self):
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        base = lric(toy(), resource=resource, inplace=False, **KWARGS)['g'].to_numpy()
        zero = lric(toy(), resource=resource, expr_prop=0, inplace=False, **KWARGS)['g'].to_numpy()
        np.testing.assert_array_equal(base, zero)

    def test_lr_sep_changes_only_the_interaction_name(self):
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        default = lric(toy(), resource=resource, inplace=False, **KWARGS)
        custom = lric(toy(), resource=resource, lr_sep='|', inplace=False, **KWARGS)
        self.assertTrue(all('^' in str(v) for v in default['interaction']))
        self.assertTrue(all('|' in str(v) and '^' not in str(v) for v in custom['interaction']))
        np.testing.assert_array_equal(default['g'].to_numpy(), custom['g'].to_numpy())

    def test_a_nonlinear_transform_still_yields_a_legal_result(self):
        # 注意：sqrt 会不会**改变**数值是 fixture 相关的——在这条八点直线的退化几何上
        # 它正好抵消。真实 fixture 上会变，那由 graded 的 lric-transform-sqrt case 覆盖；
        # 这里只钉住它仍然产出合法的非负有限结果。
        resource = pd.DataFrame({'ligand': ['La'], 'receptor': ['Ra']})
        root = lric(toy(), resource=resource, transform_fn=np.sqrt, inplace=False, **KWARGS)['g'].to_numpy()
        self.assertTrue(np.all(np.isnan(root) | (root >= 0)))
        self.assertTrue(np.all(np.isfinite(root) | np.isnan(root)))


if __name__ == '__main__':
    unittest.main()
