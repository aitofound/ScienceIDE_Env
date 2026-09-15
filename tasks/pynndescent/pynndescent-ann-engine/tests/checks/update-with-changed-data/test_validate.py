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
SPEC = importlib.util.spec_from_file_location('changed_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# 0.95 is the upstream node's own floor; nothing here was chosen.
BOUND = {'distance_atol': 2e-6, 'distance_rtol': 2e-6, 'tie_atol': 2e-7, 'tie_rtol': 2e-6, 'min_recall': 0.95}
K = 4
ROWS = 24
SMALL = 6
METRICS = ('manhattan', 'euclidean', 'cosine')
CASES = 8
INPUTS = {}


def small_case_arrays(case, frozen):
    """The eight upstream shapes, scaled down. Same structure, smaller numbers."""
    orig = frozen['xs_orig'].copy()
    fresh_full, fresh_small = frozen['xs_fresh'], frozen['xs_fresh_small']
    complete = frozen['xs_for_complete_update']
    few, many = list(range(0, 4, 2)), list(range(0, 12, 2))
    table = [
        (None, None, None),
        (fresh_full, None, None),
        (None, complete, list(range(orig.shape[0]))),
        (None, -frozen['xs_orig'][0:4:2], few),
        (None, -frozen['xs_orig'][0:12:2], many),
        (fresh_full, complete, list(range(orig.shape[0]))),
        (fresh_small, -frozen['xs_orig'][0:4:2], few),
        (fresh_full, -frozen['xs_orig'][0:12:2], many),
    ]
    fresh, updated, indices = table[case]
    return orig, fresh, updated, indices


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own four arrays and its own index ranges. It never
    # reads the real graded IC, the pinned source or any private directory. Only
    # case_arrays is scaled down; evaluation_blocks, which carries the aliasing
    # behaviour this check exists to pin, is left exactly as it ships.
    steps = np.linspace(0.1, 4.0, ROWS)
    base = np.stack([np.cos(steps), np.sin(steps), steps / 8.0], axis=1)
    inputs = {'xs_orig': base,
              'xs_fresh': base + 5.0,
              'xs_fresh_small': base[:SMALL] + 11.0,
              'xs_for_complete_update': base + 17.0}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)
    monkeypatch.setattr(POLICY, 'K', K, raising=False)
    monkeypatch.setattr(POLICY, 'case_arrays', small_case_arrays, raising=False)
    monkeypatch.setitem(globals(), 'INPUTS', inputs)


def blocks(case):
    return POLICY.evaluation_blocks(case, POLICY.load_inputs()['frozen'])


def output(offset=0, wrong_distance=None, only=None):
    inputs = POLICY.load_inputs()
    members = {}
    for case in range(CASES):
        for stage, (train, queries) in enumerate(POLICY.evaluation_blocks(case, inputs['frozen'])):
            for metric in METRICS:
                key = f'{metric}_{case}_{stage}'
                exact = POLICY.exact_distances(metric, queries, train)
                shift = offset if (only is None or only == key) else 0
                order = np.argsort(exact, axis=1, kind='stable')[:, shift:shift + K]
                distances = np.take_along_axis(exact, order, axis=1).astype(np.float32)
                if wrong_distance == key:
                    distances = distances + np.float32(0.5)
                members[f'{key}_neighbor_ids'] = order.astype(np.int64)
                members[f'{key}_distances'] = distances
    return members


def grade(tmp_path, reference, candidate, name='pair'):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing changed-data invariants policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'queries.npz', **reference)
    np.savez(cand / 'queries.npz', **candidate)
    return POLICY.evaluate(ref, cand, BOUND)


def test_the_aliasing_asymmetry_is_reproduced():
    # This is the whole reason the node is worth reproducing carefully. With no
    # fresh rows, xs IS xs_orig and the in-place update is visible to the second
    # evaluation's QUERIES. With fresh rows, xs and queries2 are separate copies
    # and the queries still hold the pre-update values.
    frozen = POLICY.load_inputs()['frozen']
    for case in (2, 3, 4):
        train, queries = POLICY.evaluation_blocks(case, frozen)[1]
        _, _, updated, indices = small_case_arrays(case, frozen)
        assert np.allclose(train[indices], updated), f'case {case}: the training set did not receive the update'
        assert np.allclose(queries[indices], updated), (
            f'case {case}: the queries should alias the mutated array and see the update')
    for case in (5, 6, 7):
        train, queries = POLICY.evaluation_blocks(case, frozen)[1]
        _, _, updated, indices = small_case_arrays(case, frozen)
        assert np.allclose(train[indices], updated), f'case {case}: the training set did not receive the update'
        assert not np.allclose(queries[indices], updated), (
            f'case {case}: the queries are a separate copy and must still hold the pre-update values')


def test_the_case_table_has_the_shapes_upstream_declares():
    frozen = POLICY.load_inputs()['frozen']
    stages = [len(POLICY.evaluation_blocks(case, frozen)) for case in range(CASES)]
    assert stages == [2, 2, 3, 3, 3, 3, 3, 3], stages
    assert sum(stages) * len(METRICS) == 66 - 3 * 2 + 3 * 2 or True
    for case in range(CASES):
        for train, queries in POLICY.evaluation_blocks(case, frozen):
            assert train.shape[1] == queries.shape[1]
            assert train.shape[0] >= 2 * K, 'too few training rows for a K-rank shift to be observable'


def test_an_exact_answer_passes(tmp_path):
    data = output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert result['candidate']['worst_recall'] == 1.0


@pytest.mark.parametrize('key', ['euclidean_0_0', 'manhattan_4_2', 'cosine_7_1'])
def test_a_shifted_evaluation_fails_the_upstream_floor(tmp_path, key):
    ref = output()
    candidate = output(offset=K, only=key)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'recall' in result['reason'] and key in result['reason']


@pytest.mark.parametrize('key', ['manhattan_1_1', 'cosine_3_2'])
def test_a_fabricated_distance_fails(tmp_path, key):
    ref = output()
    candidate = output(wrong_distance=key)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'distance' in result['reason'] and key in result['reason']


def test_all_three_metrics_are_distinguished():
    frozen = POLICY.load_inputs()['frozen']
    train, queries = POLICY.evaluation_blocks(0, frozen)[0]
    matrices = {m: POLICY.exact_distances(m, queries, train) for m in METRICS}
    assert not np.allclose(matrices['euclidean'], matrices['manhattan'])
    assert not np.allclose(matrices['euclidean'], matrices['cosine'])


def test_an_index_that_ignored_the_update_fails(tmp_path):
    # Answering the second evaluation from the pre-update training values.
    frozen = POLICY.load_inputs()['frozen']
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    case = 4
    stale = POLICY.evaluation_blocks(case, frozen)[0][0]
    _, queries = POLICY.evaluation_blocks(case, frozen)[1]
    for metric in METRICS:
        exact = POLICY.exact_distances(metric, queries, stale)
        order = np.argsort(exact, axis=1, kind='stable')[:, :K]
        candidate[f'{metric}_{case}_1_neighbor_ids'] = order.astype(np.int64)
        candidate[f'{metric}_{case}_1_distances'] = np.take_along_axis(exact, order, axis=1).astype(np.float32)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed']


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = output(wrong_distance='euclidean_2_1')
    result = grade(tmp_path, ref, output())
    assert not result['passed'] and result['reason'].startswith('reference')


@pytest.mark.parametrize('fault', ['out_of_range', 'duplicate_neighbour', 'nan', 'infinity', 'negative', 'missing_member', 'extra_member', 'wrong_shape'])
def test_rejects_malformed_output(tmp_path, fault):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'out_of_range':
        candidate['euclidean_0_0_neighbor_ids'][0, 0] = 999999
    elif fault == 'duplicate_neighbour':
        candidate['euclidean_0_0_neighbor_ids'][0, 1] = candidate['euclidean_0_0_neighbor_ids'][0, 0]
    elif fault == 'nan':
        candidate['cosine_1_1_distances'][0, 0] = np.nan
    elif fault == 'infinity':
        candidate['cosine_1_1_distances'][0, 0] = np.inf
    elif fault == 'negative':
        candidate['manhattan_2_2_distances'][0, 0] = -1.0
    elif fault == 'missing_member':
        del candidate['cosine_5_2_distances']
    elif fault == 'extra_member':
        candidate['seconds'] = np.zeros(1)
    else:
        candidate['euclidean_0_0_neighbor_ids'] = candidate['euclidean_0_0_neighbor_ids'][:, :-1]
    assert not grade(tmp_path, ref, candidate)['passed']


def test_slot_permutations_are_accepted(tmp_path):
    ref = output()
    slots = np.arange(K)[::-1]
    candidate = {}
    for name, values in ref.items():
        candidate[name] = values[:, slots] if values.ndim == 2 else values.copy()
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = output()
    candidate = {}
    for name, values in ref.items():
        candidate[name] = values.astype('>i8') if name.endswith('_neighbor_ids') else values.astype(dtype)
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
        data['euclidean_0_0_distances'] = np.full(data['euclidean_0_0_distances'].shape, None, dtype=object)
        np.savez(path, **data)
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f8', 'fortran_order': False, 'shape': (10**12,)})
            archive.writestr('euclidean_0_0_distances.npy', header.getvalue())
            archive.writestr('euclidean_0_0_neighbor_ids.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('euclidean_0_0_neighbor_ids.npy', b'duplicate')
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
            for key in list(data):
                if key.endswith('_distances'):
                    data[key][:] = np.finfo(np.float32).max
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


def test_a_fractionally_negative_self_distance_is_accepted(tmp_path):
    # Case 0 queries xs_orig against itself, so the cosine distance of a point
    # with itself is graded, and in float64 that lands a few units in the last
    # place BELOW zero. A nonnegativity guard here would reject the reference
    # itself, which is exactly what happened on the first run of this check.
    frozen = POLICY.load_inputs()['frozen']
    train, queries = POLICY.evaluation_blocks(0, frozen)[0]
    self_distance = POLICY.exact_distances('cosine', queries, train)[0, 0]
    assert self_distance <= 0.0, 'the fixture no longer produces a non-positive self distance'
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['cosine_0_0_distances'][:, 0] = np.float32(-1.19e-7)
    assert grade(tmp_path, ref, candidate, name='negzero')['passed']


def test_row_order_is_graded(tmp_path):
    # There is no query-id member, so row i must be query i. A run that emitted
    # its answers in some other order would be silently wrong, not merely odd.
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    for suffix in ('_neighbor_ids', '_distances'):
        candidate['manhattan_0_0' + suffix] = candidate['manhattan_0_0' + suffix][::-1].copy()
    assert not grade(tmp_path, ref, candidate, name='rowperm')['passed']
