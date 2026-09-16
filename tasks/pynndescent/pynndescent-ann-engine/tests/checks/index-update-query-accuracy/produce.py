"""Extend an index with update() and query it, four ways.

Both upstream nodes build on the first 600 training rows, call update() with the
remaining 202, then query the 200 held-out rows and score against the FULL 802-row
training set. One node does that without prepare(), the other calls prepare()
before and after the update and disables compression, exactly as upstream does.

Both pass random_state=None, so the graph is random per run and the check grades
each run against geometry rather than against the reference.

Note: test_update_w_prepare_query_accuracy is defined twice upstream, at
test_pynndescent_.py:543 and :572, with byte-identical bodies. The second shadows
the first, so it is one node, run once here.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent import NNDescent

K = 10
EPSILON = 0.2
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    nn = inputs['nn']
    query_ids, train_ids = inputs['query_ids'], inputs['train_ids']
    initial_rows = int(inputs['initial_rows'][0])
    fresh_rows = int(inputs['fresh_rows'][0])
if nn.shape != (1002, 5) or query_ids.size != 200 or train_ids.size != 802:
    raise ValueError('expected the frozen 1002x5 nn_data fixture and its 200/802 split')
if initial_rows + fresh_rows != train_ids.size:
    raise ValueError('the initial index plus the update batch must be the whole training set')
queries = nn[:200]
initial = nn[200:200 + initial_rows]
fresh = nn[200 + initial_rows:]
if fresh.shape[0] != fresh_rows:
    raise ValueError('the update batch does not have the row count the IC declares')
import_seconds = time.monotonic() - started

members = {'query_ids': query_ids}
timings = {}
started = time.monotonic()
for name, metric, with_prepare in [
    ('no_prepare_euclidean', 'euclidean', False),
    ('no_prepare_cosine', 'cosine', False),
    ('w_prepare_euclidean', 'euclidean', True),
    ('w_prepare_cosine', 'cosine', True),
]:
    began = time.monotonic()
    if with_prepare:
        index = NNDescent(initial, metric=metric, n_neighbors=10, random_state=None, compressed=False)
        index.prepare()
        index.update(xs_fresh=fresh)
        index.prepare()
    else:
        index = NNDescent(initial, metric=metric, n_neighbors=10, random_state=None)
        index.update(xs_fresh=fresh)
    neighbors, distances = index.query(queries, k=K, epsilon=EPSILON)
    timings[name] = time.monotonic() - began
    neighbors = np.asarray(neighbors)
    if neighbors.shape != (200, K):
        raise ValueError(f'{name}: expected ten neighbours for each of the 200 queries')
    members[f'{name}_neighbor_ids'] = train_ids[neighbors.astype(np.intp)]
    members[f'{name}_distances'] = np.asarray(distances)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'queries.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_seconds={seconds:.9f}')
print(f'all_four_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
