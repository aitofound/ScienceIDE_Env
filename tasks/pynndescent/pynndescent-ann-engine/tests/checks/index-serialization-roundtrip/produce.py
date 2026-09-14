"""Round-trip an index through four serializers and emit the answer either side.

Each of the four upstream nodes builds its own index, queries it, serializes it,
reloads it and queries again. Both answers are emitted for every serializer so
the validator can compare them directly rather than being handed a boolean the
producer computed for itself.

Three of the four nodes pass random_state=None, so the four indices are not
expected to agree with each other and are not required to.
"""
import io
import os
from pathlib import Path
import pickle
import sys
import time

started = time.monotonic()
import joblib
import numpy as np
import pynndescent
from pynndescent import NNDescent, PyNNDescentTransformer

K = 10
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    train_ids, query_ids = inputs['train_ids'], inputs['query_ids']
    train, query = inputs['train'], inputs['query']
if train.shape != (1000, 50) or query.shape != (1000, 50) or train.dtype != np.float64:
    raise ValueError('expected the frozen 1000x50 float64 training and query matrices')
import_seconds = time.monotonic() - started

members = {'query_ids': query_ids}
timings = {}


def dense_query(index):
    ids, distances = index.query(query)
    return np.asarray(ids), np.asarray(distances)


def transformer_query(index):
    matrix = index.transform(query)
    counts = np.diff(matrix.indptr)
    if counts.min() != K or counts.max() != K:
        raise ValueError('the transformer did not return exactly K neighbours per row')
    members['transformer_indptr'] = matrix.indptr.astype(np.int64)
    return matrix.indices.reshape(-1, K), matrix.data.reshape(-1, K)


def roundtrip(name, build, run, dump, load):
    started = time.monotonic()
    index = build()
    before_ids, before_distances = run(index)
    buffer = io.BytesIO()
    dump(index, buffer)
    buffer.seek(0)
    after_ids, after_distances = run(load(buffer))
    timings[name] = time.monotonic() - started
    if before_ids.shape != (1000, K):
        raise ValueError(f'{name}: expected ten neighbours for each of the 1000 queries')
    members[f'{name}_before_ids'] = train_ids[before_ids.astype(np.intp)]
    members[f'{name}_before_distances'] = before_distances
    members[f'{name}_after_ids'] = train_ids[after_ids.astype(np.intp)]
    members[f'{name}_after_distances'] = after_distances


started = time.monotonic()
roundtrip('pickle', lambda: NNDescent(train, 'euclidean', {}, K, random_state=None), dense_query, pickle.dump, pickle.load)
roundtrip('compressed_pickle', lambda: NNDescent(train, 'euclidean', {}, K, random_state=None, compressed=True), dense_query, pickle.dump, pickle.load)
roundtrip('transformer_pickle', lambda: PyNNDescentTransformer(n_neighbors=K).fit(train), transformer_query, pickle.dump, pickle.load)
roundtrip('joblib', lambda: NNDescent(train, 'euclidean', {}, K, random_state=None), dense_query, joblib.dump, joblib.load)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'roundtrip.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_build_query_roundtrip_requery_seconds={seconds:.9f}')
print(f'all_four_roundtrips_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
