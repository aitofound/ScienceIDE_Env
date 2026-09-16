import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import zipfile

import numpy as np
import pytest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('degenerate_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# 0.95 everywhere is the upstream nodes' own floor; nothing here was chosen.
BOUND = {'distance_atol': 2e-6, 'distance_rtol': 2e-6, 'tie_atol': 2e-7, 'tie_rtol': 2e-6, 'min_recall': 0.95}
K = 4
INPUTS = {}


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own three base arrays. It never reads the real
    # graded IC, the pinned source or any private directory. The rows are spread
    # along a curve so every query has an unambiguous top-K in all three metrics.
    steps = np.linspace(0.0, 3.0, 30)
    nn = np.stack([steps, steps ** 2 / 6.0, np.sin(steps)], axis=1)
    small = np.stack([np.linspace(5.0, 40.0, 2 * K), np.linspace(40.0, 5.0, 2 * K), np.linspace(1.0, 9.0, 2 * K)], axis=1)
    sparse_dense = np.stack([np.linspace(0.1, 4.0, 16), np.linspace(4.0, 0.1, 16), np.linspace(0.5, 2.5, 16)], axis=1)
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', nn=nn, small=small, sparse=sparse_dense)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    layout = {
        'tree_init_false_euclidean': ('nn', 'euclidean', 10, 30),
        'tree_init_false_cosine': ('nn', 'cosine', 10, 30),
        'one_dimensional_euclidean': ('nn1', 'euclidean', 10, 30),
        'one_dimensional_manhattan': ('nn1', 'manhattan', 10, 30),
        # Deliberately mirrors the real fixture, where the dense no-split
        # configuration has exactly K training rows: see the degeneracy test.
        'no_split_dense_euclidean': ('small', 'euclidean', K, 2 * K),
        'no_split_dense_cosine': ('small', 'cosine', K, 2 * K),
        'no_split_sparse_euclidean': ('sparse', 'euclidean', 8, 16),
        'no_split_sparse_cosine': ('sparse', 'cosine', 8, 16),
    }
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)
    monkeypatch.setattr(POLICY, 'K', K, raising=False)
    monkeypatch.setattr(POLICY, 'LAYOUT', layout, raising=False)
    monkeypatch.setitem(globals(), 'INPUTS', {'layout': layout})


def configs():
    return tuple(INPUTS['layout'])


def exact(name):
    return POLICY.exact_distances(name, POLICY.load_inputs())


def output(offset=0, wrong_distance=None, only=None):
    frozen = POLICY.load_inputs()
    members = {}
    for name in configs():
        matrix = exact(name)
        train_ids, query_ids = frozen['ids'][name]
        shift = offset if (only is None or only == name) else 0
        order = np.argsort(matrix, axis=1, kind='stable')[:, shift:shift + K]
        distances = np.take_along_axis(matrix, order, axis=1).astype(np.float32)
        if wrong_distance == name:
            distances = distances + np.float32(0.5)
        members[f'{name}_query_ids'] = query_ids
        members[f'{name}_neighbor_ids'] = train_ids[order]
        members[f'{name}_distances'] = distances
    return members


def grade(tmp_path, reference, candidate, name='pair'):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing degenerate-configuration invariants policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'queries.npz', **reference)
    np.savez(cand / 'queries.npz', **candidate)
    return POLICY.evaluate(ref, cand, BOUND)


SHIFTABLE = ('tree_init_false_euclidean', 'tree_init_false_cosine',
             'one_dimensional_euclidean', 'one_dimensional_manhattan',
             'no_split_sparse_euclidean', 'no_split_sparse_cosine')
DEGENERATE = ('no_split_dense_euclidean', 'no_split_dense_cosine')


def test_every_shiftable_configuration_can_observe_a_shift():
    frozen = POLICY.load_inputs()
    for name in SHIFTABLE:
        matrix = exact(name)
        train_ids, query_ids = frozen['ids'][name]
        assert matrix.shape == (query_ids.size, train_ids.size), name
        assert matrix.shape[1] >= 2 * K, f'{name}: too few training rows for a K-rank shift to be observable'
        order = np.argsort(matrix, axis=1, kind='stable')
        shifted = POLICY.tie_aware_recall(matrix, order[:, K:2 * K], BOUND['tie_atol'], BOUND['tie_rtol']).mean()
        assert shifted < BOUND['min_recall'], f'{name}: a K-rank shift still scores {shifted}, so the degradation test would be vacuous'


def test_the_dense_no_split_configuration_cannot_discriminate_and_that_is_disclosed(tmp_path):
    # In the real fixture the dense no-split node trains on ten rows and asks for
    # ten neighbours, so EVERY training point is returned and the recall is 1.0
    # whatever the implementation does. That is a property of the upstream node,
    # not of this check, and it is reproduced here rather than hidden: the layout
    # gives this configuration exactly K training rows.
    frozen = POLICY.load_inputs()
    for name in DEGENERATE:
        train_ids, _ = frozen['ids'][name]
        assert train_ids.size == K, f'{name}: the selftest layout no longer reproduces the degeneracy'
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    for name in DEGENERATE:
        slots = np.arange(K)[::-1]
        candidate[f'{name}_neighbor_ids'] = candidate[f'{name}_neighbor_ids'][:, slots]
        candidate[f'{name}_distances'] = candidate[f'{name}_distances'][:, slots]
    result = grade(tmp_path, ref, candidate)
    assert result['passed']
    assert all(result['candidate'][name]['k_equals_training_rows'] for name in DEGENERATE)
    assert all(result['candidate'][name]['global_recall'] == 1.0 for name in DEGENERATE)


def test_an_exact_answer_passes(tmp_path):
    data = output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert all(result['candidate'][name]['global_recall'] == 1.0 for name in configs())


@pytest.mark.parametrize('name', SHIFTABLE)
def test_a_shifted_configuration_fails_the_shared_upstream_floor(tmp_path, name):
    ref = output()
    candidate = output(offset=K, only=name)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'recall' in result['reason'] and name in result['reason']


@pytest.mark.parametrize('name', ['tree_init_false_cosine', 'one_dimensional_euclidean', 'no_split_sparse_cosine'])
def test_a_fabricated_distance_fails(tmp_path, name):
    ref = output()
    candidate = output(wrong_distance=name)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'distance' in result['reason'] and name in result['reason']


def test_all_three_metrics_are_actually_distinguished(tmp_path):
    # euclidean, manhattan and cosine must not collapse onto each other, or the
    # per-configuration metric choice would be untested.
    euclidean = exact('one_dimensional_euclidean')
    manhattan = exact('one_dimensional_manhattan')
    cosine = exact('no_split_dense_cosine')
    assert np.allclose(euclidean, manhattan), 'in one dimension euclidean and manhattan coincide, which is expected'
    assert not np.allclose(cosine, exact('no_split_dense_euclidean'))


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = output(wrong_distance='tree_init_false_euclidean')
    result = grade(tmp_path, ref, output())
    assert not result['passed'] and result['reason'].startswith('reference')


@pytest.mark.parametrize('fault', ['unknown_id', 'duplicate_neighbour', 'duplicate_query', 'nan', 'infinity', 'negative', 'missing_member', 'extra_member', 'float_ids'])
def test_rejects_wrong_identity_or_malformed_output(tmp_path, fault):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'unknown_id':
        candidate['tree_init_false_euclidean_neighbor_ids'][0, 0] = 999999
    elif fault == 'duplicate_neighbour':
        candidate['tree_init_false_euclidean_neighbor_ids'][0, 1] = candidate['tree_init_false_euclidean_neighbor_ids'][0, 0]
    elif fault == 'duplicate_query':
        candidate['no_split_dense_cosine_query_ids'][0] = candidate['no_split_dense_cosine_query_ids'][1]
    elif fault == 'nan':
        candidate['one_dimensional_euclidean_distances'][0, 0] = np.nan
    elif fault == 'infinity':
        candidate['one_dimensional_euclidean_distances'][0, 0] = np.inf
    elif fault == 'negative':
        candidate['no_split_sparse_euclidean_distances'][0, 0] = -5.0
    elif fault == 'missing_member':
        del candidate['no_split_sparse_cosine_distances']
    elif fault == 'extra_member':
        candidate['epsilon'] = np.zeros(1)
    else:
        candidate['tree_init_false_cosine_query_ids'] = candidate['tree_init_false_cosine_query_ids'].astype(np.float64)
    assert not grade(tmp_path, ref, candidate)['passed']


def test_row_and_slot_permutations_are_accepted(tmp_path):
    ref = output()
    candidate = {}
    for name in configs():
        rows = np.arange(ref[f'{name}_query_ids'].size)[::-1]
        slots = np.arange(K)[::-1]
        candidate[f'{name}_query_ids'] = ref[f'{name}_query_ids'][rows]
        candidate[f'{name}_neighbor_ids'] = ref[f'{name}_neighbor_ids'][rows][:, slots]
        candidate[f'{name}_distances'] = ref[f'{name}_distances'][rows][:, slots]
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = output()
    candidate = {}
    for name in configs():
        candidate[f'{name}_query_ids'] = ref[f'{name}_query_ids'].astype('>i8')
        candidate[f'{name}_neighbor_ids'] = ref[f'{name}_neighbor_ids'].astype('>i8')
        candidate[f'{name}_distances'] = ref[f'{name}_distances'].astype(dtype)
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('bad', [{'distance_atol': 0.0}, {'distance_atol': float('nan')}, {'tie_rtol': -1.0}, {'min_recall': 0.0}, {'min_recall': 1.0}])
def test_invalid_bounds_fail_without_nonfinite_json(tmp_path, bad):
    bounds = dict(BOUND)
    bounds.update(bad)
    result = POLICY.evaluate(tmp_path, tmp_path, bounds)
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


@pytest.mark.parametrize('damage', ['garbage', 'truncated_zip', 'huge_header', 'duplicate_members', 'object_array'])
def test_invalid_archive_always_returns_failure(tmp_path, damage):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'queries.npz', **output())
    path = cand / 'queries.npz'
    if damage == 'garbage':
        path.write_bytes(b'not an archive')
    elif damage == 'truncated_zip':
        path.write_bytes((ref / 'queries.npz').read_bytes()[:-30])
    elif damage == 'object_array':
        data = output()
        data['tree_init_false_euclidean_distances'] = np.full(data['tree_init_false_euclidean_distances'].shape, None, dtype=object)
        np.savez(path, **data)
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f8', 'fortran_order': False, 'shape': (10**12,)})
            archive.writestr('tree_init_false_euclidean_distances.npy', header.getvalue())
            archive.writestr('tree_init_false_euclidean_query_ids.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('tree_init_false_euclidean_query_ids.npy', b'duplicate')
    result = POLICY.evaluate(ref, cand, BOUND)
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


def test_cli_writes_failure_json_for_missing_outputs(tmp_path):
    result_path = tmp_path / 'result.json'
    command = [sys.executable, str(POLICY.HERE / 'validate.py'), '--reference', str(tmp_path / 'absent-ref'), '--candidate', str(tmp_path / 'absent-cand'), '--rubric', str(POLICY.HERE / 'rubric.json'), '--out', str(result_path)]
    run = subprocess.run(command, capture_output=True, text=True)
    assert run.returncode == 0, run.stderr
    assert json.loads(result_path.read_text())['passed'] is False


def test_oversized_otherwise_valid_archive_is_rejected(tmp_path):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'queries.npz', **output())
    (cand / 'queries.npz').write_bytes(b'\x00' * POLICY.SIZE_LIMIT + (ref / 'queries.npz').read_bytes())
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False


@pytest.mark.parametrize('side', ['reference', 'candidate', 'both'])
def test_extreme_finite_values_fail_through_complete_cli(tmp_path, side):
    directories = {name: tmp_path / name for name in ['reference', 'candidate']}
    for name, directory in directories.items():
        directory.mkdir()
        data = output()
        if side == name or side == 'both':
            for config in configs():
                data[f'{config}_distances'][:] = np.finfo(np.float32).max
        np.savez(directory / 'queries.npz', **data)
    result = tmp_path / 'extreme-result.json'
    command = [sys.executable, str(POLICY.HERE / 'validate.py'), '--reference', str(directories['reference']), '--candidate', str(directories['candidate']), '--rubric', str(POLICY.HERE / 'rubric.json'), '--out', str(result)]
    process = subprocess.run(command, capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    payload = result.read_bytes()
    assert payload.isascii()
    record = json.loads(payload)
    assert record['passed'] is False
    json.dumps(record, allow_nan=False)


def test_corrupt_deflate_returns_failure_json(tmp_path):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez_compressed(ref / 'queries.npz', **output())
    damaged = bytearray((ref / 'queries.npz').read_bytes())
    offset = 30 + int.from_bytes(damaged[26:28], 'little') + int.from_bytes(damaged[28:30], 'little')
    damaged[offset] = 7
    (cand / 'queries.npz').write_bytes(damaged)
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False
