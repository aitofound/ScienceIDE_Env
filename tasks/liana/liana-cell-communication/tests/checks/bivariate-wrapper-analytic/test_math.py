"""独立小型 bivariate wrapper 语义测试；依赖LIANA，仅开发用途，不算graded覆盖。"""
import unittest

import numpy as np
import pandas as pd
from anndata import AnnData
import scipy.sparse as sp
from scipy.sparse import csr_matrix

from liana.method.sp._bivariate._spatial_bivariate import bivariate


def dense(a):
    return np.asarray(a.todense() if sp.issparse(a) else a, dtype=np.float64)


def toy(n=6, seed=3):
    rng = np.random.default_rng(seed)
    counts = rng.uniform(0.5, 5.0, size=(n, 4)).astype(np.float32)
    adata = AnnData(csr_matrix(counts), var=pd.DataFrame(index=['La', 'Lb', 'Ra', 'Rb']))
    adata.raw = adata
    weight = np.eye(n) + np.eye(n, k=1) + np.eye(n, k=-1)
    adata.obsp['spatial_connectivities'] = csr_matrix(weight)
    return adata


RESOURCE = pd.DataFrame({'ligand': ['La', 'Lb'], 'receptor': ['Ra', 'Rb']})


def run(**kwargs):
    keywords = dict(resource=RESOURCE, use_raw=True, nz_prop=0)
    keywords.update(kwargs)
    return bivariate(toy(), **keywords)


class MathTests(unittest.TestCase):
    def test_n_perms_none_produces_no_pvalues(self):
        out = run(local_name='jaccard', global_name='lee', n_perms=None)
        self.assertNotIn('pvals', list(out.layers.keys()))
        self.assertNotIn('morans_pvals', out.var.columns)

    def test_n_perms_zero_produces_analytical_pvalues(self):
        out = run(local_name='morans', global_name=['morans'], n_perms=0, mask_negatives=True)
        self.assertIn('pvals', list(out.layers.keys()))
        values = dense(out.layers['pvals'])
        self.assertTrue(np.isfinite(values).all())
        self.assertGreaterEqual(values.min(), 0.0)
        self.assertLessEqual(values.max(), 1.0)

    def test_add_categories_emits_a_three_valued_layer(self):
        out = run(local_name='jaccard', global_name='lee', n_perms=None, add_categories=True)
        cats = np.unique(dense(out.layers['cats']))
        self.assertTrue(set(cats.tolist()).issubset({-1, 0, 1}))

    def test_jaccard_local_scores_stay_in_the_unit_interval(self):
        out = run(local_name='jaccard', global_name='lee', n_perms=None)
        values = dense(out.X)
        self.assertGreaterEqual(values.min(), 0.0)
        self.assertLessEqual(values.max(), 1.0)

    def test_output_shape_is_spots_by_interactions(self):
        out = run(local_name='morans', global_name=['morans'], n_perms=0)
        self.assertEqual(out.shape, (6, len(RESOURCE)))

    def test_var_carries_mean_and_std_per_interaction(self):
        out = run(local_name='morans', global_name=['morans'], n_perms=0)
        self.assertTrue({'mean', 'std'}.issubset(out.var.columns))
        self.assertEqual(len(out.var), len(RESOURCE))

    def test_the_caller_object_is_not_stripped(self):
        adata = toy()
        annotations = (set(adata.obsm), set(adata.uns), set(adata.obsp))
        bivariate(adata, resource=RESOURCE, use_raw=True, nz_prop=0,
                  local_name='morans', global_name=['morans'], n_perms=0)
        self.assertEqual((set(adata.obsm), set(adata.uns), set(adata.obsp)), annotations)

    def test_mask_negatives_changes_the_analytical_tail(self):
        masked = run(local_name='morans', global_name=['morans'], n_perms=0, mask_negatives=True)
        unmasked = run(local_name='morans', global_name=['morans'], n_perms=0, mask_negatives=False)
        self.assertFalse(np.array_equal(dense(masked.layers['pvals']), dense(unmasked.layers['pvals'])))


if __name__ == '__main__':
    unittest.main()
