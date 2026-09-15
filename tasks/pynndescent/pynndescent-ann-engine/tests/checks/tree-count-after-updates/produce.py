"""Record how many trees an index keeps, at construction and after five updates.

test_pynndescent_.py:644-661 builds an index over the single point [[1.0]] with a
given n_trees, then updates it five times with single points, checking two integer
attributes at every stage. There is no fixture, no seed and no approximation here.

Both attributes are recorded at every stage, including before the first update,
because the non-obvious half of the contract is that n_trees_after_update already
holds the post-update value while n_trees still holds what was requested.
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
    initial_point = inputs['initial_point']
    update_points = inputs['update_points']
    tree_counts = inputs['tree_counts'].astype(np.int64)
    n_neighbors = int(inputs['n_neighbors'][0])
if initial_point.shape != (1, 1) or update_points.shape != (5, 1) or tree_counts.ndim != 1:
    raise ValueError('expected the frozen single-point fixture, five update steps and the tree-count list')
import_seconds = time.monotonic() - started

stages = update_points.shape[0] + 1
n_trees = np.empty((tree_counts.size, stages), dtype=np.int64)
n_trees_after_update = np.empty_like(n_trees)
timings = {}
started = time.monotonic()
for row, requested in enumerate(tree_counts):
    began = time.monotonic()
    index = NNDescent(initial_point, n_neighbors=n_neighbors, n_trees=int(requested))
    n_trees[row, 0] = int(index.n_trees)
    n_trees_after_update[row, 0] = int(index.n_trees_after_update)
    for step in range(update_points.shape[0]):
        index.update(xs_fresh=update_points[step:step + 1])
        n_trees[row, step + 1] = int(index.n_trees)
        n_trees_after_update[row, step + 1] = int(index.n_trees_after_update)
    timings[int(requested)] = time.monotonic() - began
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'tree_counts.npz').open('xb') as stream:
    np.savez(stream, tree_counts=tree_counts, n_trees=n_trees, n_trees_after_update=n_trees_after_update)
print(f'import_and_input_seconds={import_seconds:.9f}')
for requested, seconds in timings.items():
    print(f'n_trees_{requested}_seconds={seconds:.9f}')
print(f'all_configurations_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
