"""Run the eight update shapes across three metrics and emit every evaluation.

test_pynndescent_.py:608-641 with conftest.py:88-103. For each case the node builds
an index on xs_orig with n_neighbors=40 and random_state=1234, prepares it, queries
once, applies the update, queries again, and - when rows were changed in place -
queries a third time with the changed rows themselves.

The aliasing in the upstream body is reproduced verbatim and deliberately. When
xs_fresh is None, `xs = xs_orig` and the in-place assignment mutates the array the
queries also point at; when xs_fresh is given, xs and queries2 are two separate
vstack copies and only xs receives the update. Tidying that up would be running a
different test.
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
N_NEIGHBORS = 40
SEED = 1234
METRICS = ('manhattan', 'euclidean', 'cosine')
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    frozen = {name: np.asarray(inputs[name], dtype=np.float64) for name in
              ('xs_orig', 'xs_fresh', 'xs_fresh_small', 'xs_for_complete_update')}
if frozen['xs_orig'].shape != (1000, 5) or frozen['xs_fresh_small'].shape != (100, 5):
    raise ValueError('expected the frozen update_data arrays')
import_seconds = time.monotonic() - started


def case_arrays(case):
    """conftest.py:93-102."""
    orig = frozen['xs_orig'].copy()
    table = [
        (None, None, None),
        (frozen['xs_fresh'], None, None),
        (None, frozen['xs_for_complete_update'], list(range(orig.shape[0]))),
        (None, -frozen['xs_orig'][0:50:2], list(range(0, 50, 2))),
        (None, -frozen['xs_orig'][0:500:2], list(range(0, 500, 2))),
        (frozen['xs_fresh'], frozen['xs_for_complete_update'], list(range(orig.shape[0]))),
        (frozen['xs_fresh_small'], -frozen['xs_orig'][0:50:2], list(range(0, 50, 2))),
        (frozen['xs_fresh'], -frozen['xs_orig'][0:500:2], list(range(0, 500, 2))),
    ]
    fresh, updated, indices = table[case]
    return orig, fresh, updated, indices


members = {}
timings = {}
started = time.monotonic()
for case in range(8):
    for metric in METRICS:
        began = time.monotonic()
        orig, fresh, updated, indices = case_arrays(case)
        index = NNDescent(orig, metric=metric, n_neighbors=N_NEIGHBORS, random_state=SEED)
        index.prepare()
        queries1 = orig
        stages = [index.query(queries1, k=K)]
        index.update(xs_fresh=fresh, xs_updated=updated, updated_indices=indices)
        # test_pynndescent_.py:632-641, aliasing included.
        if fresh is not None:
            xs = np.vstack((orig, fresh))
            queries2 = np.vstack((queries1, fresh))
        else:
            xs = orig
            queries2 = queries1
        if indices is not None:
            xs[indices] = updated
        stages.append(index.query(queries2, k=K))
        if indices is not None:
            stages.append(index.query(updated, k=K))
        timings[f'{metric}_{case}'] = time.monotonic() - began
        for stage, (neighbors, distances) in enumerate(stages):
            neighbors = np.asarray(neighbors)
            if neighbors.shape[1] != K:
                raise ValueError(f'{metric}_{case}_{stage}: expected {K} neighbours per query')
            members[f'{metric}_{case}_{stage}_neighbor_ids'] = neighbors.astype(np.int64)
            members[f'{metric}_{case}_{stage}_distances'] = np.asarray(distances)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'queries.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in sorted(timings.items()):
    print(f'{name}_seconds={seconds:.9f}')
print(f'all_twentyfour_configurations_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
