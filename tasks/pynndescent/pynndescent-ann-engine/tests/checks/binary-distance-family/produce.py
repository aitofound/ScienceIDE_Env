"""Emit the ten complete distance matrices this node's family covers.

test_distances.py::test_binary_check over its eight metrics (:55-90), plus
::test_bit_hamming (:401) and ::test_bit_jaccard (:418). Every matrix is produced by
the pinned kernels themselves, pair by pair, exactly as the upstream nodes call them.

The two bit-packed kernels are emitted RAW, in the spaces they return: bit_hamming a
count of differing bits, bit_jaccard the negative logarithm of the similarity. That
is what upstream divides and exponentiates at :414 and :430 respectively.
"""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import named_distances, bit_hamming, bit_jaccard

METRICS = ('jaccard', 'matching', 'dice', 'rogerstanimoto',
           'russellrao', 'sokalmichener', 'sokalsneath', 'yule')
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    binary = np.asarray(inputs['binary'])
    bits = np.asarray(inputs['bits'])
if binary.dtype != np.bool_ or binary.ndim != 2:
    raise ValueError('expected the frozen boolean fixture')
if bits.dtype != np.uint8 or bits.ndim != 2:
    raise ValueError('expected the frozen bit-packed array')
missing = [metric for metric in METRICS if metric not in named_distances]
if missing:
    raise ValueError(f'the pinned source does not register {missing}')
import_seconds = time.monotonic() - started

members = {}
timings = {}
rows = binary.shape[0]
for metric in METRICS:
    began = time.monotonic()
    function = named_distances[metric]
    members[f'{metric}_matrix'] = np.array(
        [[function(binary[i], binary[j]) for j in range(rows)] for i in range(rows)], dtype=np.float64)
    timings[metric] = time.monotonic() - began

bit_rows = bits.shape[0]
for name, function in (('bit_hamming', bit_hamming), ('bit_jaccard', bit_jaccard)):
    began = time.monotonic()
    members[f'{name}_matrix'] = np.array(
        [[function(bits[i], bits[j]) for j in range(bit_rows)] for i in range(bit_rows)], dtype=np.float64)
    timings[name] = time.monotonic() - began

with (Path(os.environ['OUT_DIR']) / 'distances.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
for name, seconds in timings.items():
    matrix = members[f'{name}_matrix']
    print(f'{name}_seconds={seconds:.9f} shape={matrix.shape[0]}x{matrix.shape[1]}'
          f' range=[{matrix.min():.9f},{matrix.max():.9f}]')
print(f'all_ten_matrices_including_lazy_jit_seconds={sum(timings.values()):.9f}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
