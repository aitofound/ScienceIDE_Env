"""Query four indices the way the four remaining query-accuracy nodes do.

test_pynndescent_.py:150, :167, :186 and :205. Each trains on rows 200: of its
fixture and queries rows :200 at k=10 with its own n_neighbors and epsilon, all with
random_state=None. Unlike the neighbour-graph nodes these call index.query(), which
applies the registered distance correction - except for bit_jaccard, which has none,
so that one alone comes back as -ln(similarity).

The bitpacked input is derived from the same base array, (nn*256).astype(uint8), as
upstream does at :206.
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

K = 10
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    nn = np.asarray(inputs['nn'], dtype=np.float64)
    block = sparse.csr_matrix((np.asarray(inputs['sparse_data'], dtype=np.float64),
                               np.asarray(inputs['sparse_indices'], dtype=np.int32),
                               np.asarray(inputs['sparse_indptr'], dtype=np.int32)),
                              shape=tuple(int(value) for value in inputs['sparse_shape']))
    held_out = int(inputs['held_out_rows'][0])
block.sort_indices()
if held_out <= 0 or nn.shape[0] - held_out <= K or block.shape[0] - held_out <= K:
    raise ValueError('the frozen fixtures cannot support the held-out split this check declares')
packed = (nn * 256).astype(np.uint8)
import_seconds = time.monotonic() - started

configurations = {
    'angular': (nn, 'cosine', 30, 0.32),
    'sparse_euclidean': (block, 'euclidean', 15, 0.24),
    'sparse_angular': (block, 'cosine', 50, 0.36),
    'bitpacked': (packed, 'bit_jaccard', 50, 0.36),
}

members = {}
timings = {}
notices = {}
started = time.monotonic()
for name, (data, metric, n_neighbors, epsilon) in configurations.items():
    began = time.monotonic()
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter('always')
        index = NNDescent(data[held_out:], metric, n_neighbors=n_neighbors, random_state=None)
        ids, distances = index.query(data[:held_out], k=K, epsilon=epsilon)
    timings[name] = time.monotonic() - began
    notices[name] = [str(item.message) for item in caught]
    ids = np.asarray(ids)
    distances = np.asarray(distances)
    if ids.shape != (held_out, K) or distances.shape != ids.shape:
        raise ValueError(f'{name}: the query did not return {K} neighbours for each held-out row')
    members[f'{name}_neighbor_ids'] = ids.astype(np.int64)
    members[f'{name}_distances'] = distances
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'queries.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    ids = members[f'{name}_neighbor_ids']
    print(f'{name}_seconds={seconds:.9f} train_rows={configurations[name][0].shape[0] - held_out}'
          f' unfilled_slots={int(np.count_nonzero(ids < 0))}')
    for message in notices[name]:
        print(f'{name}_warning={message}')
print(f'all_four_configurations_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
