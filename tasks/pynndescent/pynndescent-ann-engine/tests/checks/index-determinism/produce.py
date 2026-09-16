"""Build the same index twice with the same random_state and emit both results.

The upstream node's claim is reproducibility: two independently constructed
NNDescent indices given the same random_state must answer the same query
identically. Both answers are emitted so the validator can check that directly,
rather than being handed a boolean the producer computed for itself.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent import NNDescent

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    train_ids, query_ids = inputs['train_ids'], inputs['query_ids']
    train, query = inputs['train'], inputs['query']
if train.shape != (1000, 50) or query.shape != (1000, 50) or train.dtype != np.float64:
    raise ValueError('expected the frozen 1000x50 float64 training and query matrices')
if train_ids.shape != (1000,) or query_ids.shape != (1000,):
    raise ValueError('expected one identity per training and query row')
import_seconds = time.monotonic() - started

started = time.monotonic()
first = NNDescent(train, random_state=np.random.RandomState(42))
neighbors, distances = first.query(query)
first_seconds = time.monotonic() - started

started = time.monotonic()
# A second, independently constructed index with the same random_state. Not a
# second query against the first index: the node is about reproducing the BUILD.
second = NNDescent(train, random_state=np.random.RandomState(42))
repeat_neighbors, repeat_distances = second.query(query)
second_seconds = time.monotonic() - started

if neighbors.shape != (1000, 10):
    raise ValueError('expected ten neighbours for each of the 1000 queries')
with (Path(os.environ['OUT_DIR']) / 'neighbors.npz').open('xb') as stream:
    np.savez(stream, query_ids=query_ids,
             neighbor_ids=train_ids[neighbors.astype(np.intp)],
             distances=distances,
             repeat_neighbor_ids=train_ids[repeat_neighbors.astype(np.intp)],
             repeat_distances=repeat_distances)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'first_build_and_query_including_lazy_jit_seconds={first_seconds:.9f}')
print(f'second_build_and_query_seconds={second_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
