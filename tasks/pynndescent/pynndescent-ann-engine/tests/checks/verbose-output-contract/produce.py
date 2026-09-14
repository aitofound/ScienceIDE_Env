"""Capture what four index constructions print, verbose and quiet.

test_pynndescent_.py:372-437 builds the same configuration four ways and looks at
stdout: with verbose=True the output must mention the configured tree and
iteration counts, with verbose=False there must be none. The raw captured bytes
are emitted so the validator applies the upstream regexes itself rather than being
handed a boolean the producer computed.

The captured text carries a wall-clock timestamp, so it is not reproducible
between runs. The validator never compares it across runs.
"""
import io
import os
from contextlib import redirect_stdout
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent import NNDescent, PyNNDescentTransformer

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    spatial = inputs['spatial']
    n_neighbors = int(inputs['n_neighbors'][0])
    n_trees = int(inputs['n_trees'][0])
    n_iters = int(inputs['n_iters'][0])
    seed = int(inputs['seed'][0])
    sigma = inputs['sigma']
if spatial.shape != (12, 20) or sigma.shape != (20,):
    raise ValueError('expected the frozen 12x20 spatial fixture and its sigma vector')
import_seconds = time.monotonic() - started


def rng():
    return np.random.RandomState(seed)


def index(verbose):
    return lambda: NNDescent(data=spatial, metric='euclidean', metric_kwds={}, n_neighbors=n_neighbors,
                             random_state=rng(), n_trees=n_trees, n_iters=n_iters, verbose=verbose)


def transformer(verbose, metric, kwds):
    return lambda: PyNNDescentTransformer(n_neighbors=n_neighbors, metric=metric, metric_kwds=kwds,
                                          random_state=rng(), n_trees=n_trees, n_iters=n_iters,
                                          verbose=verbose).fit_transform(spatial)


members = {}
timings = {}
started = time.monotonic()
for name, build in [
    ('nndescent_verbose', index(True)),
    ('nndescent_quiet', index(False)),
    ('transformer_verbose', transformer(True, 'euclidean', {})),
    # test_pynndescent_.py:434 is the only node that uses a non-default metric.
    ('transformer_quiet', transformer(False, 'standardised_euclidean', {'sigma': sigma})),
]:
    began = time.monotonic()
    captured = io.StringIO()
    with redirect_stdout(captured):
        build()
    timings[name] = time.monotonic() - began
    members[f'{name}_stdout'] = np.frombuffer(captured.getvalue().encode('utf-8'), dtype=np.uint8)
total_seconds = time.monotonic() - started

with (Path(os.environ['OUT_DIR']) / 'verbose.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    print(f'{name}_seconds={seconds:.9f} bytes={members[f"{name}_stdout"].size}')
print(f'all_four_including_lazy_jit_seconds={total_seconds:.9f}')
print(f'source_import={pynndescent.__file__}')
