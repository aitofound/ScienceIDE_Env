"""Build one UNSEEDED neighbour graph, as test_random_state_none does.

test_pynndescent_.py:261-277: NNDescent(nn_data, "euclidean", {}, 10,
random_state=None), read through the RAW _neighbor_graph attribute, so no sqrt
correction is applied and the distances come back squared.
"""
import os
from pathlib import Path
import sys
import time
import warnings

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent import NNDescent

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    base = np.asarray(inputs['nn'], dtype=np.float64)
    n_neighbors = int(inputs['n_neighbors'][0])
if base.ndim != 2 or base.shape[0] <= n_neighbors:
    raise ValueError('the frozen base array cannot support a neighbour graph of this width')
import_seconds = time.monotonic() - started

began = time.monotonic()
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter('always')
    ids, distances = NNDescent(base, 'euclidean', {}, n_neighbors, random_state=None)._neighbor_graph
build_seconds = time.monotonic() - began
ids = np.asarray(ids)
distances = np.asarray(distances)
if ids.shape != (base.shape[0], n_neighbors) or distances.shape != ids.shape:
    raise ValueError('the neighbour graph does not have one row per point and the requested width')

members = {'neighbor_ids': ids.astype(np.int64), 'distances': distances}
with (Path(os.environ['OUT_DIR']) / 'graph.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'build_seconds={build_seconds:.9f}')
print(f'unfilled_slots={int(np.count_nonzero(ids < 0))}')
for message in [str(item.message) for item in caught]:
    print(f'build_warning={message}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
