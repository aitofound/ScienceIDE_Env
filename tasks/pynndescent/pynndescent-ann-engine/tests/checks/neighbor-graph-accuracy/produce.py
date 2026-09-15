"""Build the five neighbour graphs the upstream accuracy nodes read.

test_pynndescent_.py:19, :37, :56, :92 and :114. Each constructs one NNDescent and
reads the RAW `_neighbor_graph` attribute - not the `neighbor_graph` property - so
no registered distance correction is applied and the distances come out in the
metric's internal space. The construction arguments are reproduced positionally
exactly as upstream writes them.

The bitpacked configuration cannot fill every slot on this fixture and upstream
warns about it; the warning is captured and reported rather than silenced, and the
unfilled slots are emitted as they come, id -1 with an infinite distance.
"""
import os
from pathlib import Path
import sys
import time
import warnings

started = time.monotonic()
import numpy as np
from scipy import sparse
import pynndescent
from pynndescent import NNDescent

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    nn = np.asarray(inputs['nn'], dtype=np.float64)
    block = sparse.csr_matrix((np.asarray(inputs['sparse_data'], dtype=np.float64),
                               np.asarray(inputs['sparse_indices'], dtype=np.int32),
                               np.asarray(inputs['sparse_indptr'], dtype=np.int32)),
                              shape=tuple(int(value) for value in inputs['sparse_shape']))
    seed = int(inputs['seed'][0])
block.sort_indices()
if nn.shape[1] < 1 or nn.shape[0] <= 10 or block.shape[0] <= 10:
    raise ValueError('the frozen fixtures are too small for these nodes')
# test_pynndescent_.py:57 - the bitpacked node derives its input from the same array.
packed = (nn * 256).astype(np.uint8)
import_seconds = time.monotonic() - started

builders = {
    'euclidean': lambda: NNDescent(nn, 'euclidean', {}, 10, random_state=np.random.RandomState(seed)),
    'angular': lambda: NNDescent(nn, 'cosine', {}, 10, random_state=np.random.RandomState(seed)),
    'bitpacked': lambda: NNDescent(packed, 'bit_jaccard', {}, 10, random_state=np.random.RandomState(seed)),
    'sparse_euclidean': lambda: NNDescent(block, 'euclidean', n_neighbors=20, random_state=None),
    'sparse_angular': lambda: NNDescent(block, 'cosine', {}, 20, random_state=None),
}

members = {}
timings = {}
notices = {}
started = time.monotonic()
for name, build in builders.items():
    began = time.monotonic()
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter('always')
        ids, distances = build()._neighbor_graph
    timings[name] = time.monotonic() - began
    notices[name] = [str(item.message) for item in caught]
    ids = np.asarray(ids)
    distances = np.asarray(distances)
    if ids.shape != distances.shape or ids.ndim != 2:
        raise ValueError(f'{name}: the neighbour graph is not a pair of matching matrices')
    members[f'{name}_neighbor_ids'] = ids.astype(np.int64)
    members[f'{name}_distances'] = distances
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'graph.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    graph = members[f'{name}_neighbor_ids']
    unfilled = int(np.count_nonzero(graph < 0))
    print(f'{name}_seconds={seconds:.9f} shape={graph.shape[0]}x{graph.shape[1]} unfilled_slots={unfilled}')
    for message in notices[name]:
        print(f'{name}_warning={message}')
print(f'all_five_configurations_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
