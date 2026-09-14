"""Emit the upstream rankdata ranks for every frozen case under all five methods."""
import os
from pathlib import Path
import sys
import time

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent.distances import rankdata

METHODS = ('average', 'min', 'max', 'dense', 'ordinal')
source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    lengths = inputs['case_lengths']
    names = sorted(name for name in inputs.files if name.startswith('case_') and name != 'case_lengths')
    # Each case keeps the dtype it was frozen with. Casting them to one common
    # type would merge 2**60 and 2**60+1, which is the point of two of the cases.
    cases = [inputs[name] for name in names]
if len(cases) != lengths.size or any(values.size != length for values, length in zip(cases, lengths)):
    raise ValueError('the frozen case layout does not match case_lengths')
import_seconds = time.monotonic() - started
started = time.monotonic()
ranks = {f'ranks_{method}': np.concatenate([np.asarray(rankdata(values, method=method), dtype=np.float64)
                                            for values in cases] + [np.empty(0)])
         for method in METHODS}
with (Path(os.environ['OUT_DIR']) / 'ranks.npz').open('xb') as stream:
    np.savez(stream, case_lengths=lengths, **ranks)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'ranking_and_output_including_lazy_jit_seconds={time.monotonic() - started:.9f}')
print(f'source_import={pynndescent.__file__}')
