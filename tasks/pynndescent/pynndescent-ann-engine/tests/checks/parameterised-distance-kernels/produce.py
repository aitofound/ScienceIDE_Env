"""Emit the three parameterised distance matrices.

test_distances.py::test_seuclidean (:226), ::test_mahalanobis (:267) and
::test_haversine (:286). Each kernel is called pair by pair with the extra argument
the upstream node supplies: the standardisation weights, the covariance matrix that
node hands to sklearn in the VI position, and - for the haversine - only the first
two columns of each point.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import standardised_euclidean, mahalanobis, haversine

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    points = np.asarray(inputs['points'])
    weights = np.asarray(inputs['seuclidean_weights'], dtype=np.float64)
    matrix = np.asarray(inputs['mahalanobis_matrix'], dtype=np.float64)
rows, columns = points.shape
if weights.shape != (columns,) or matrix.shape != (columns, columns) or columns < 2:
    raise ValueError('the frozen extra arguments do not match the frozen point set')
import_seconds = time.monotonic() - started

members = {}
timings = {}
for name, build in (
    ('seuclidean', lambda i, j: standardised_euclidean(points[i], points[j], weights)),
    ('mahalanobis', lambda i, j: mahalanobis(points[i], points[j], matrix)),
    # test_distances.py:288 - the haversine reads only the first two columns.
    ('haversine', lambda i, j: haversine(points[i, :2], points[j, :2])),
):
    began = time.monotonic()
    members[f'{name}_matrix'] = np.array([[build(i, j) for j in range(rows)] for i in range(rows)],
                                         dtype=np.float64)
    timings[name] = time.monotonic() - began

with (Path(os.environ['OUT_DIR']) / 'distances.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    value = members[f'{name}_matrix']
    print(f'{name}_seconds={seconds:.9f} range=[{value.min():.9f},{value.max():.9f}]')
print(f'all_three_matrices_including_lazy_jit_seconds={sum(timings.values()):.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
