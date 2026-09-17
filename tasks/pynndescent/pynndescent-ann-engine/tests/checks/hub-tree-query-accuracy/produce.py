"""Prepare a hub-tree index per data type and query a held-out set of 100 points.

Each of the five upstream nodes trains on rows 100: and queries rows :100 with
k=10 and its own epsilon, then measures recall against exact ground truth. Both
the identities and the distances are emitted so the validator can recompute the
recall itself and check that each reported distance really belongs to the
neighbour it is attached to.

Distances go out exactly as the kernel returns them. For bit_jaccard that is
-ln(intersection/union), not an ordinary Jaccard distance: distances.py:1822-1847
is already the transformed quantity and named_distances registers no correction
for it. The validator undoes the transform.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import scipy.sparse as sparse
import pynndescent
from pynndescent import NNDescent

QUERIES = 100
K = 10
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    frozen = {name: inputs[name] for name in inputs.files}
dense, angular, bits = frozen['dense'], frozen['angular'], frozen['bits']
query_ids, train_ids = frozen['query_ids'], frozen['train_ids']
sp = sparse.csr_matrix((frozen['sparse_data'], frozen['sparse_indices'], frozen['sparse_indptr']), shape=(500, 50))
sp_norm = sparse.csr_matrix((frozen['sparse_norm_data'], frozen['sparse_norm_indices'], frozen['sparse_norm_indptr']), shape=(500, 50))
if dense.shape != (500, 20) or bits.shape != (500, 20) or sp.shape != (500, 50):
    raise ValueError('expected the frozen hub-tree fixtures')
if query_ids.size != QUERIES or train_ids.size != 400:
    raise ValueError('expected one hundred held-out queries and four hundred training rows')
import_seconds = time.monotonic() - started

members = {'query_ids': query_ids}
timings = {}
started = time.monotonic()
for name, data, metric, epsilon in [
    ('dense_euclidean', dense, 'euclidean', 0.2),
    ('dense_angular', angular, 'cosine', 0.2),
    ('sparse_euclidean', sp, 'euclidean', 0.2),
    ('sparse_angular', sp_norm, 'cosine', 0.2),
    ('bitpacked', bits, 'bit_jaccard', 0.3),
]:
    began = time.monotonic()
    index = NNDescent(data[QUERIES:], metric=metric, n_neighbors=15, random_state=42)
    index.prepare()
    neighbors, distances = index.query(data[:QUERIES], k=K, epsilon=epsilon)
    timings[name] = time.monotonic() - began
    neighbors = np.asarray(neighbors)
    if neighbors.shape != (QUERIES, K):
        raise ValueError(f'{name}: expected ten neighbours for each of the hundred queries')
    members[f'{name}_neighbor_ids'] = train_ids[neighbors.astype(np.intp)]
    members[f'{name}_distances'] = np.asarray(distances)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'queries.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_build_prepare_query_seconds={seconds:.9f}')
print(f'all_five_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
