"""Emit the upstream rankdata average ranks for three fully tied blocks."""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import rankdata

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    sizes = inputs['sizes']
if sizes.ndim != 1 or sizes.size == 0 or np.any(sizes <= 0):
    raise ValueError('expected the frozen positive block sizes')
import_seconds = time.monotonic() - started
started = time.monotonic()
# test_rank.py:92: the data is ones(n, int), generated rather than stored.
blocks = []
for size in sizes:
    blocks.append(np.asarray(rankdata(np.ones(int(size), dtype=np.int64)), dtype=np.float64))
    print(f'block_{int(size)}_seconds={time.monotonic() - started:.9f}')
with (Path(os.environ['OUT_DIR']) / 'ranks.npz').open('xb') as stream:
    np.savez(stream, sizes=sizes, ranks=np.concatenate(blocks))
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'ranking_and_output_including_lazy_jit_seconds={time.monotonic() - started:.9f}')
print(f'source_import={pynndescent.__file__}')
