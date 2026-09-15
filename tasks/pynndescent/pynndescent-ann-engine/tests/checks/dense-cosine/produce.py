"""Emit the upstream cosine-distance kernel's complete pairwise matrix."""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import cosine

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    points, sample_ids = inputs['points'], inputs['sample_ids']
if points.shape != (12, 20) or points.dtype != np.float32 or sample_ids.shape != (12,):
    raise ValueError('expected the complete official 12x20 float32 fixture')
import_seconds = time.monotonic() - started
started = time.monotonic()
distances = np.array([[cosine(left, right) for right in points] for left in points])
with (Path(os.environ['OUT_DIR']) / 'distances.npz').open('xb') as stream:
    np.savez(stream, sample_ids=sample_ids, distances=distances)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'matrix_and_output_including_lazy_jit_seconds={time.monotonic() - started:.9f}')
print(f'source_import={pynndescent.__file__}')
