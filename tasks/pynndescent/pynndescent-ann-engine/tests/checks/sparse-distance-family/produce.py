"""Emit the sixteen complete sparse distance matrices.

test_distances.py::test_sparse_spatial_check over its nine metrics (:94-157) and
::test_sparse_binary_check over its seven (:161-224). Each kernel is called with the
row's index and data arrays exactly as the upstream nodes call it, and the six
kernels listed in sparse_need_n_features additionally receive the column count.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
from scipy.sparse import csr_matrix
import pynndescent
import pynndescent.sparse as spdist

SPATIAL_METRICS = ('euclidean', 'manhattan', 'chebyshev', 'minkowski', 'hamming',
                   'canberra', 'cosine', 'braycurtis', 'correlation')
BINARY_METRICS = ('jaccard', 'matching', 'dice', 'rogerstanimoto',
                  'russellrao', 'sokalmichener', 'sokalsneath')
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    spatial = csr_matrix((np.asarray(inputs['spatial_data'], dtype=np.float32),
                          np.asarray(inputs['spatial_indices'], dtype=np.int32),
                          np.asarray(inputs['spatial_indptr'], dtype=np.int32)),
                         shape=tuple(int(v) for v in inputs['spatial_shape']))
    binary = csr_matrix((np.asarray(inputs['binary_data']).astype(bool),
                         np.asarray(inputs['binary_indices'], dtype=np.int32),
                         np.asarray(inputs['binary_indptr'], dtype=np.int32)),
                        shape=tuple(int(v) for v in inputs['binary_shape']))
spatial.sort_indices()
binary.sort_indices()
missing = [m for m in SPATIAL_METRICS + BINARY_METRICS if m not in spdist.sparse_named_distances]
if missing:
    raise ValueError(f'the pinned source does not register the sparse metrics {missing}')
import_seconds = time.monotonic() - started


def matrix_for(metric, block):
    function = spdist.sparse_named_distances[metric]
    rows = block.shape[0]
    if metric in spdist.sparse_need_n_features:
        return np.array([[function(block[i].indices, block[i].data, block[j].indices,
                                   block[j].data, block.shape[1]) for j in range(rows)]
                         for i in range(rows)], dtype=np.float64)
    return np.array([[function(block[i].indices, block[i].data, block[j].indices, block[j].data)
                      for j in range(rows)] for i in range(rows)], dtype=np.float64)


members = {}
timings = {}
for label, block, metrics in (('spatial', spatial, SPATIAL_METRICS), ('binary', binary, BINARY_METRICS)):
    for metric in metrics:
        began = time.monotonic()
        members[f'{label}_{metric}_matrix'] = matrix_for(metric, block)
        timings[f'{label}_{metric}'] = time.monotonic() - began

with (Path(os.environ['OUT_DIR']) / 'distances.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    matrix = members[f'{name}_matrix']
    print(f'{name}_seconds={seconds:.9f} needs_n_features='
          f'{name.split("_", 1)[1] in spdist.sparse_need_n_features}'
          f' range=[{matrix.min():.9f},{matrix.max():.9f}]')
print(f'all_sixteen_matrices_including_lazy_jit_seconds={sum(timings.values()):.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
