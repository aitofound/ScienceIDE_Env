"""Prepare a hub-tree index for each of five data types and query it with its own points.

Each of the five upstream nodes builds an index, calls prepare(), then queries the
FIRST FIFTY training rows with k=1 and counts how many find themselves. Both the
identity found and the distance reported are emitted, so the validator can check
the count against the upstream floor AND check that the reported distance really
is the distance to the point that was named.

Unlike index._neighbor_graph, index.query() returns CORRECTED distances, so these
go out in ordinary metric space.
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

QUERIES = 50
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
if query_ids.size != QUERIES or train_ids.size != 500:
    raise ValueError('expected fifty query identities and five hundred training identities')
import_seconds = time.monotonic() - started

members = {'query_ids': query_ids}
timings = {}
started = time.monotonic()
for name, data, metric in [
    ('dense_euclidean', dense, 'euclidean'),
    ('dense_angular', angular, 'cosine'),
    ('sparse_euclidean', sp, 'euclidean'),
    ('sparse_angular', sp_norm, 'cosine'),
    ('bitpacked', bits, 'bit_jaccard'),
]:
    began = time.monotonic()
    index = NNDescent(data, metric=metric, n_neighbors=15, random_state=42)
    index.prepare()
    neighbors, distances = index.query(data[:QUERIES], k=1)
    timings[name] = time.monotonic() - began
    neighbors = np.asarray(neighbors)
    if neighbors.shape != (QUERIES, 1):
        raise ValueError(f'{name}: expected one neighbour for each of the fifty queries')
    members[f'{name}_neighbor_ids'] = train_ids[neighbors.astype(np.intp)]
    members[f'{name}_distances'] = np.asarray(distances)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'selfquery.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_build_prepare_query_seconds={seconds:.9f}')
print(f'all_five_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
