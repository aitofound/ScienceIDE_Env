"""Answer the same query through both API paths, as test_transformer_equivalence does.

test_pynndescent_.py:234-258. One NNDescent on nn_data[:400] with n_neighbors=16
queried for k=15 at epsilon=0.15, and one PyNNDescentTransformer fitted on the same
rows with n_neighbors=15 and search_epsilon=0.15. Both random_state=42 and both with
compression off. Upstream shifts n_neighbors by one on the NNDescent side to match
sklearn's KNeighborsTransformer definition; that shift is reproduced here.

Both answers are emitted whole. The transformer side is written as its CSR triple
after sorted_indices(), which is the form upstream compares.
"""
import os
from pathlib import Path
import sys
import time
import warnings

started = time.monotonic()
import numpy as np
import pynndescent
from pynndescent import NNDescent, PyNNDescentTransformer

source = Path(os.environ['PYNNDESCENT_BUILD_DIR']).resolve()
if not Path(pynndescent.__file__).resolve().is_relative_to(source):
    raise RuntimeError('PyNNDescent was not imported from the scratch source copy')
with np.load(Path(os.environ['CHECK_DIR']) / 'ic' / sys.argv[1] / 'inputs.npz', allow_pickle=False) as inputs:
    base = np.asarray(inputs['nn'], dtype=np.float64)
    train_rows = int(inputs['train_rows'][0])
    test_rows = int(inputs['test_rows'][0])
    n_neighbors = int(inputs['n_neighbors'][0])
    epsilon = float(inputs['search_epsilon'][0])
    random_state = int(inputs['random_state'][0])
if test_rows > train_rows or n_neighbors >= train_rows:
    raise ValueError('the frozen split cannot support this neighbour count')
train, test = base[:train_rows], base[:test_rows]
import_seconds = time.monotonic() - started

# test_pynndescent_.py:241-243 - the +1 conforms to sklearn's transformer definition.
began = time.monotonic()
index = NNDescent(data=train, n_neighbors=n_neighbors + 1, random_state=random_state, compressed=False)
indices, distances = index.query(test, k=n_neighbors, epsilon=epsilon)
query_seconds = time.monotonic() - began

began = time.monotonic()
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter('always')
    transformer = PyNNDescentTransformer(n_neighbors=n_neighbors, search_epsilon=epsilon,
                                         random_state=random_state).fit(train, compress_index=False)
    graph = transformer.transform(test).sorted_indices()
transformer_seconds = time.monotonic() - began

indices = np.asarray(indices)
distances = np.asarray(distances)
if indices.shape != (test_rows, n_neighbors) or distances.shape != indices.shape:
    raise ValueError('the query did not return the requested neighbours for every test row')
if graph.shape != (test_rows, train_rows):
    raise ValueError('the transformer graph does not have one row per test point and one column per training point')

members = {
    'query_neighbor_ids': indices.astype(np.int64),
    'query_distances': distances,
    'transformer_indptr': np.asarray(graph.indptr, dtype=np.int64),
    'transformer_indices': np.asarray(graph.indices, dtype=np.int64),
    'transformer_data': np.asarray(graph.data),
}
with (Path(os.environ['OUT_DIR']) / 'equivalence.npz').open('xb') as stream:
    np.savez(stream, **members)
print(f'import_and_input_seconds={import_seconds:.9f}')
print(f'query_path_seconds={query_seconds:.9f}')
print(f'transformer_path_seconds={transformer_seconds:.9f}')
print(f'graph_nnz={int(graph.nnz)} entries_per_row={sorted(set(np.diff(graph.indptr).tolist()))}')
for message in [str(item.message) for item in caught]:
    print(f'transformer_warning={message}')
print(f'members={len(members)}')
print(f'source_import={pynndescent.__file__}')
