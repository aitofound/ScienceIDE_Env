"""Emit the upstream weighted-Minkowski kernel's complete pairwise matrix at p=3."""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import weighted_minkowski

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    points, sample_ids, weights = inputs['points'], inputs['sample_ids'], inputs['weights']
if points.shape != (12, 20) or points.dtype != np.float32 or sample_ids.shape != (12,):
    raise ValueError('expected the complete official 12x20 float32 fixture')
if weights.shape != (20,) or weights.dtype != np.float64:
    raise ValueError('expected the frozen float64 weight vector of length 20')
import_seconds = time.monotonic() - started
started = time.monotonic()
# w and p are both passed explicitly, exactly as test_distances.py:254 calls the
# kernel: p=3 here is the upstream node's choice, not the kernel's default.
distances = np.array([[weighted_minkowski(left, right, weights, 3) for right in points] for left in points])
with (Path(os.environ['OUT_DIR']) / 'distances.npz').open('xb') as stream:
    np.savez(stream, sample_ids=sample_ids, distances=distances)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'matrix_and_output_including_lazy_jit_seconds={time.monotonic() - started:.9f}')
print(f'source_import={pynndescent.__file__}')
