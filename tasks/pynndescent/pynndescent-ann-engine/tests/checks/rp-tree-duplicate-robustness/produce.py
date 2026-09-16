"""Build the cosine training graph on three pathological datasets and emit it.

All three upstream nodes read the same thing: index._neighbor_graph, the graph
NN-descent builds over the training set itself. Its distances for the cosine
metric are ALTERNATIVE cosine values, not corrected ones; they are emitted as the
index stores them and the validator undoes the transform before comparing with
geometry.
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
N_TREES = 20
SEED = 189212
DATASETS = ('hang', 'near', 'dedup')
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    data = {name: inputs[name] for name in DATASETS}
    ids = {name: inputs[f'{name}_ids'] for name in DATASETS}
if data['hang'].shape != (4400, 50) or data['near'].shape != (32, 2) or data['dedup'].shape != (1000, 50):
    raise ValueError('expected the frozen cosine_hang, cosine_near_duplicates and deduplicated subsets')
import_seconds = time.monotonic() - started

members = {}
timings = {}
started = time.monotonic()
for name in DATASETS:
    began = time.monotonic()
    # test_pynndescent_.py:301-308, 322-329 and 355-362: same call in all three.
    neighbors, distances = NNDescent(
        data[name], 'cosine', {}, K,
        random_state=np.random.RandomState(SEED), n_trees=N_TREES,
    )._neighbor_graph
    timings[name] = time.monotonic() - began
    if neighbors.shape != (data[name].shape[0], K):
        raise ValueError(f'{name}: expected ten neighbours for every row')
    members[f'{name}_ids'] = ids[name]
    members[f'{name}_neighbor_ids'] = ids[name][neighbors.astype(np.intp)]
    members[f'{name}_distances'] = distances
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'graphs.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_graph_seconds={seconds:.9f}')
print(f'all_three_graphs_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
