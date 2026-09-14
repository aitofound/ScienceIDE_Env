"""独立小型 inflow 参数分支语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd
import scipy.sparse as sp
from anndata import AnnData
from scipy.sparse import csr_matrix

from liana.method import inflow
from liana.utils.transform import zi_minmax

RESOURCE = pd.DataFrame({'ligand': ['La', 'Lb'], 'receptor': ['Ra', 'Rb']})


def toy(n=8, seed=5):
    rng = np.random.default_rng(seed)
    counts = rng.uniform(0.5, 4.0, size=(n, 4)).astype(np.float32)
    adata = AnnData(csr_matrix(counts), var=pd.DataFrame(index=['La', 'Lb', 'Ra', 'Rb']))
    adata.raw = adata
    adata.obs['cell_type'] = pd.Categorical(['A'] * (n // 2) + ['B'] * (n - n // 2))
    adata.obsm['spatial'] = np.stack([np.arange(n, dtype=float) * 10.0, np.zeros(n)], axis=1)
    weight = np.eye(n) + np.eye(n, k=1) + np.eye(n, k=-1)
    adata.obsp['spatial_connectivities'] = csr_matrix(weight)
    return adata


def run(**kwargs):
    keywords = dict(groupby='cell_type', resource=RESOURCE, use_raw=True, nz_prop=0)
    keywords.update(kwargs)
    return inflow(toy(), **keywords)


class MathTests(unittest.TestCase):
    def test_columns_are_celltype_ligand_receptor_triples(self):
        out = run()
        for name in out.var_names:
            parts = str(name).split('^')
            self.assertEqual(len(parts), 3)
            self.assertIn(parts[0], {'A', 'B'})

    def test_output_is_sparse_and_nonnegative(self):
        out = run()
        self.assertTrue(sp.issparse(out.X))
        self.assertGreaterEqual(float(np.asarray(out.X.todense()).min()), 0.0)

    def test_one_row_per_input_spot(self):
        out = run()
        self.assertEqual(out.shape[0], 8)
        self.assertEqual(list(out.obs_names), list(toy().obs_names))

    def test_the_caller_annotations_are_carried_over(self):
        out = run()
        self.assertIn('spatial', out.obsm)
        self.assertIn('spatial_connectivities', out.obsp)
        np.testing.assert_array_equal(np.asarray(out.obsm['spatial']), toy().obsm['spatial'])

    def test_var_carries_the_five_summary_columns(self):
        out = run()
        self.assertTrue({'mean', 'variance', 'std', 'cv', 'nonzero_fraction'}.issubset(out.var.columns))

    def test_summary_mean_matches_the_column_mean(self):
        out = run()
        dense = np.asarray(out.X.todense(), dtype=np.float64)
        np.testing.assert_allclose(out.var['mean'].to_numpy(), dense.mean(axis=0), atol=1e-12, rtol=0)

    def test_summary_std_is_the_square_root_of_variance(self):
        out = run()
        np.testing.assert_allclose(out.var['std'].to_numpy(),
                                   np.sqrt(out.var['variance'].to_numpy()), atol=1e-12, rtol=0)

    def test_zi_minmax_transform_bounds_the_output(self):
        out = run(x_transform=zi_minmax, y_transform=zi_minmax, use_raw=False)
        dense = np.asarray(out.X.todense(), dtype=np.float64)
        self.assertGreaterEqual(dense.min(), 0.0)
        self.assertLessEqual(dense.max(), 1.0)

    def test_a_stricter_nz_prop_keeps_no_more_columns(self):
        loose = run(nz_prop=0)
        strict = run(nz_prop=0.99)
        self.assertLessEqual(strict.shape[1], loose.shape[1])

    def test_a_one_hot_obsm_matches_the_groupby_path(self):
        adata = toy()
        adata.obsm['ct_onehot'] = pd.get_dummies(adata.obs['cell_type'])
        left = inflow(adata.copy(), resource=RESOURCE, use_raw=True, nz_prop=0, groupby='cell_type')
        right = inflow(adata.copy(), resource=RESOURCE, use_raw=True, nz_prop=0, obsm_key='ct_onehot')
        self.assertEqual(left.shape, right.shape)
        np.testing.assert_allclose(np.asarray(left.X.todense()), np.asarray(right.X.todense()),
                                   atol=1e-5, rtol=0)

    def test_transform_kwargs_reach_the_transform(self):
        seen = {}

        def transform(mat, clip_max=1.0):
            seen['clip_max'] = clip_max
            return mat

        run(x_transform=transform, x_transform_kwargs={'clip_max': 0.25}, use_raw=False)
        self.assertEqual(seen['clip_max'], 0.25)

    def test_giving_neither_groupby_nor_obsm_key_is_rejected(self):
        with self.assertRaises(ValueError):
            inflow(toy(), resource=RESOURCE, use_raw=True, nz_prop=0)


if __name__ == '__main__':
    unittest.main()
