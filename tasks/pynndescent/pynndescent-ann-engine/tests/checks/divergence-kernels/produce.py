"""Emit the two Jensen-Shannon matrices and the dense and sparse Wasserstein
matrices at every exponent.

test_distances.py::test_jensen_shannon (:335), ::test_sparse_jensen_shannon (:351)
and ::test_wasserstein_1d (:382). The kernels are called exactly as those nodes call
them: the sparse Jensen-Shannon and the sparse Wasserstein receive a row's index and
data arrays, the dense ones receive whole rows.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
from scipy.sparse import csr_matrix
import pynndescent
import pynndescent.distances as dist
import pynndescent.sparse as spdist

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    js = np.asarray(inputs['js_dense'], dtype=np.float64)
    sparse_js = csr_matrix((np.asarray(inputs['sparse_js_data'], dtype=np.float64),
                            np.asarray(inputs['sparse_js_indices'], dtype=np.int32),
                            np.asarray(inputs['sparse_js_indptr'], dtype=np.int32)),
                           shape=tuple(int(v) for v in inputs['sparse_js_shape']))
    wasserstein_dense = np.asarray(inputs['wasserstein_dense'], dtype=np.float64)
    p_values = [float(v) for v in inputs['p_values']]
sparse_js.sort_indices()
wasserstein_sparse = csr_matrix(wasserstein_dense)
wasserstein_sparse.sort_indices()
rows = js.shape[0]
if sparse_js.shape[0] != rows or wasserstein_dense.shape[0] != rows or not p_values:
    raise ValueError('the frozen blocks do not describe the same sample set')
before = wasserstein_dense.copy()
import_seconds = time.monotonic() - started

members = {}
timings = {}
began = time.monotonic()
members['jensen_shannon_matrix'] = np.array(
    [[dist.jensen_shannon_divergence(js[i], js[j]) for j in range(rows)] for i in range(rows)],
    dtype=np.float64)
timings['jensen_shannon'] = time.monotonic() - began

began = time.monotonic()
members['sparse_jensen_shannon_matrix'] = np.array(
    [[spdist.sparse_jensen_shannon_divergence(sparse_js[i].indices, sparse_js[i].data,
                                              sparse_js[j].indices, sparse_js[j].data)
      for j in range(rows)] for i in range(rows)], dtype=np.float64)
timings['sparse_jensen_shannon'] = time.monotonic() - began

for index, p in enumerate(p_values):
    began = time.monotonic()
    members[f'wasserstein_dense_p{index}_matrix'] = np.array(
        [[dist.wasserstein_1d(wasserstein_dense[i], wasserstein_dense[j], p) for j in range(rows)]
         for i in range(rows)], dtype=np.float64)
    members[f'wasserstein_sparse_p{index}_matrix'] = np.array(
        [[spdist.sparse_wasserstein_1d(wasserstein_sparse[i].indices, wasserstein_sparse[i].data,
                                       wasserstein_sparse[j].indices, wasserstein_sparse[j].data, p)
          for j in range(rows)] for i in range(rows)], dtype=np.float64)
    timings[f'wasserstein_p{index}'] = time.monotonic() - began

if not np.array_equal(before, wasserstein_dense):
    raise RuntimeError('the wasserstein kernel mutated its input, which would make the matrix order-dependent')

with (Path(os.environ['OUT_DIR']) / 'divergences.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    if name.startswith('wasserstein_p'):
        index = int(name.split('p')[-1])
        dense = members[f'wasserstein_dense_p{index}_matrix']
        gap = float(np.max(np.abs(dense - members[f'wasserstein_sparse_p{index}_matrix'])))
        print(f'{name}_seconds={seconds:.9f} p={p_values[index]} range=[{dense.min():.9f},{dense.max():.9f}]'
              f' dense_vs_sparse_max_abs={gap:.6e}')
        continue
    value = members[f'{name}_matrix']
    print(f'{name}_seconds={seconds:.9f} range=[{value.min():.9f},{value.max():.9f}]')
print(f'all_matrices_including_lazy_jit_seconds={sum(timings.values()):.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
