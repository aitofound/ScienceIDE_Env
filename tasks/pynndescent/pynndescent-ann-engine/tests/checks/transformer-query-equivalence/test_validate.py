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
SPEC = importlib.util.spec_from_file_location('transformer_equivalence_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# agreement_atol and agreement_rtol are numpy's allclose defaults, which is literally
# what the upstream assertion uses at test_pynndescent_.py:258.
BOUND = {'distance_atol': 2e-06, 'distance_rtol': 2e-06,
         'agreement_atol': 1e-08, 'agreement_rtol': 1e-05,
         'tie_atol': 2e-07, 'tie_rtol': 2e-06}
ROWS = 30
TRAIN = 12
TEST = 5
K = 4


def artificial_base():
    values = ((np.arange(ROWS * 3) * 7919) % 251) / 251.0
    return values.reshape(ROWS, 3)


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own base array with its own split and its own k, all
    # different from the real 1002 / 400 / 200 / 15, so a validator that hard-coded
    # any of them cannot pass. It never reads the real graded IC, the pinned source
    # or any private directory.
    inputs = {'nn': artificial_base(),
              'train_rows': np.array([TRAIN], dtype=np.int64),
              'test_rows': np.array([TEST], dtype=np.int64),
              'n_neighbors': np.array([K], dtype=np.int64),
              'search_epsilon': np.array([0.15], dtype=np.float64),
              'random_state': np.array([42], dtype=np.int64)}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)


def exact_output(**overrides):
    """What both paths produce when they agree and are right."""
    inputs = POLICY.load_inputs()
    order = np.argsort(inputs['exact'], axis=1, kind='stable')[:, :inputs['k']]
    distances = np.take_along_axis(inputs['exact'], order, axis=1)
    by_id = np.argsort(order, axis=1, kind='stable')
    members = {
        'query_neighbor_ids': order.astype(np.int64),
        'query_distances': distances.astype(np.float32),
        'transformer_indptr': (np.arange(inputs['test_rows'] + 1) * inputs['k']).astype(np.int64),
        'transformer_indices': np.take_along_axis(order, by_id, axis=1).ravel().astype(np.int64),
        'transformer_data': np.take_along_axis(distances, by_id, axis=1).ravel().astype(np.float32),
    }
    members.update(overrides)
    return members


def grade(tmp_path, reference, candidate, name='pair', bound=None):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing transformer-equivalence pass policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'equivalence.npz', **reference)
    np.savez(cand / 'equivalence.npz', **candidate)
    return POLICY.evaluate(ref, cand, bound or BOUND)


def test_the_artificial_inputs_are_not_the_real_ones():
    inputs = POLICY.load_inputs()
    assert (inputs['train_rows'], inputs['test_rows'], inputs['k']) == (TRAIN, TEST, K)
    assert (inputs['train_rows'], inputs['test_rows'], inputs['k']) != (400, 200, 15)
    assert inputs['exact'].shape == (TEST, TRAIN)
    assert inputs['test_rows'] < inputs['train_rows'], 'upstream queries a subset of what it trained on'


def test_this_node_states_no_recall_floor():
    # test_transformer_equivalence asserts equivalence, not accuracy. No floor was
    # invented for it: the validator reports the recall it measures and gates only
    # on cross-path agreement and on the distances being true.
    assert not hasattr(POLICY, 'MIN_RECALL'), 'a recall floor was introduced without an upstream source'


def test_an_exact_answer_passes(tmp_path):
    data = exact_output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert result['candidate']['recall'] == pytest.approx(1.0)


def test_the_two_paths_must_name_the_same_neighbours(tmp_path):
    # The whole point of the node. Swapping one transformer identity for another
    # legitimate training point must be caught even though both are plausible.
    ref = exact_output()
    candidate = {name: values.copy() for name, values in ref.items()}
    row = candidate['transformer_indices'][:K]
    replacement = next(i for i in range(TRAIN) if i not in row)
    candidate['transformer_indices'][K - 1] = replacement
    candidate['transformer_indices'][:K] = np.sort(candidate['transformer_indices'][:K])
    result = grade(tmp_path, ref, candidate, name='iddisagree')
    assert not result['passed'] and 'same' in result['reason']


def test_the_two_paths_must_report_the_same_distances(tmp_path):
    ref = exact_output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['transformer_data'][0] += np.float32(0.01)
    result = grade(tmp_path, ref, candidate, name='distdisagree')
    assert not result['passed']


def test_agreement_is_not_merely_ordering(tmp_path):
    # The transformer row is sorted by identity while the query row is sorted by
    # distance. A validator that compared them without reordering would be wrong,
    # and one that sorted BOTH by value would accept a genuinely mismatched pairing.
    ref = exact_output()
    inputs = POLICY.load_inputs()
    assert not np.array_equal(ref['query_neighbor_ids'].ravel(), ref['transformer_indices']), (
        'the two orderings coincide on this data, so the reordering is untested')
    candidate = {name: values.copy() for name, values in ref.items()}
    first = slice(0, K)
    candidate['transformer_data'][first] = candidate['transformer_data'][first][::-1]
    if np.allclose(candidate['transformer_data'][first], ref['transformer_data'][first]):
        pytest.skip('the first row distances are symmetric, so this permutation is invisible')
    assert not grade(tmp_path, ref, candidate, name='pairing')['passed']


def test_a_distance_that_is_not_the_distance_to_the_named_point_fails(tmp_path):
    # Both paths agreeing is not enough: they must agree on the TRUE value, so an
    # implementation that is consistently wrong is still rejected.
    ref = exact_output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['query_distances'] += np.float32(1.0)
    candidate['transformer_data'] += np.float32(1.0)
    result = grade(tmp_path, ref, candidate, name='consistentlywrong')
    assert not result['passed'] and 'distance to the neighbour it names' in result['reason']


@pytest.mark.parametrize('fault', ['indptr_not_uniform', 'indptr_wrong_total', 'unsorted_indices',
                                   'duplicate_indices', 'index_out_of_range', 'query_duplicate',
                                   'query_out_of_range', 'nan_distance', 'negative_distance',
                                   'missing_member', 'extra_member', 'wrong_shape', 'float_ids'])
def test_rejects_malformed_output(tmp_path, fault):
    ref = exact_output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'indptr_not_uniform':
        candidate['transformer_indptr'][1] += 1
        candidate['transformer_indptr'][2] -= 1
    elif fault == 'indptr_wrong_total':
        candidate['transformer_indptr'][-1] -= 1
    elif fault == 'unsorted_indices':
        candidate['transformer_indices'][:K] = candidate['transformer_indices'][:K][::-1]
    elif fault == 'duplicate_indices':
        candidate['transformer_indices'][1] = candidate['transformer_indices'][0]
    elif fault == 'index_out_of_range':
        candidate['transformer_indices'][0] = TRAIN
    elif fault == 'query_duplicate':
        candidate['query_neighbor_ids'][0, 1] = candidate['query_neighbor_ids'][0, 0]
    elif fault == 'query_out_of_range':
        candidate['query_neighbor_ids'][0, 0] = -1
    elif fault == 'nan_distance':
        candidate['query_distances'][0, 0] = np.float32(np.nan)
    elif fault == 'negative_distance':
        candidate['query_distances'][0, 0] = np.float32(-1.0)
    elif fault == 'missing_member':
        del candidate['transformer_data']
    elif fault == 'extra_member':
        candidate['seconds'] = np.zeros(1, dtype=np.float64)
    elif fault == 'wrong_shape':
        candidate['query_neighbor_ids'] = candidate['query_neighbor_ids'][:, :-1]
    else:
        candidate['transformer_indices'] = candidate['transformer_indices'].astype(np.float64)
    assert not grade(tmp_path, ref, candidate, name=fault)['passed']


def test_a_tiny_disagreement_inside_allclose_is_accepted(tmp_path):
    # The upstream assertion is np.allclose, and the two real paths agree to 0.0.
    # A float32 rounding-scale difference must not fail the pair.
    ref = exact_output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['transformer_data'] = (candidate['transformer_data'].astype(np.float64)
                                     * (1.0 + 1e-9)).astype(np.float32)
    assert grade(tmp_path, ref, candidate, name='tiny')['passed']


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = exact_output()
    broken = {name: values.copy() for name, values in ref.items()}
    broken['transformer_data'][0] += np.float32(0.5)
    result = grade(tmp_path, broken, exact_output(), name='badref')
    assert not result['passed'] and result['reason'].startswith('reference')


def test_query_slot_permutations_are_accepted_but_row_order_is_graded(tmp_path):
    ref = exact_output()
    permuted = {name: values.copy() for name, values in ref.items()}
    permuted['query_neighbor_ids'] = permuted['query_neighbor_ids'][:, ::-1].copy()
    permuted['query_distances'] = permuted['query_distances'][:, ::-1].copy()
    assert grade(tmp_path, ref, permuted, name='slots')['passed']
    rows = {name: values.copy() for name, values in ref.items()}
    rows['query_neighbor_ids'] = rows['query_neighbor_ids'][::-1].copy()
    rows['query_distances'] = rows['query_distances'][::-1].copy()
    assert not grade(tmp_path, ref, rows, name='rowperm')['passed']


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = exact_output()
    candidate = {name: (values.astype(dtype) if name in ('query_distances', 'transformer_data')
                        else values.copy()) for name, values in ref.items()}
    assert grade(tmp_path, ref, candidate, name=dtype.replace('<', 'lt').replace('>', 'gt'))['passed']


@pytest.mark.parametrize('bad', [{'distance_atol': 0.0}, {'distance_atol': float('nan')},
                                 {'agreement_atol': -1.0}, {'tie_rtol': -1.0},
                                 {'agreement_rtol': float('inf')}])
def test_invalid_bounds_fail_without_nonfinite_json(tmp_path, bad):
    result = POLICY.evaluate(tmp_path, tmp_path, dict(BOUND, **bad))
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


@pytest.mark.parametrize('damage', ['garbage', 'truncated_zip', 'huge_header', 'duplicate_members', 'object_array'])
def test_invalid_archive_always_returns_failure(tmp_path, damage):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'equivalence.npz', **exact_output())
    path = cand / 'equivalence.npz'
    if damage == 'garbage':
        path.write_bytes(b'not an archive at all')
    elif damage == 'truncated_zip':
        path.write_bytes((ref / 'equivalence.npz').read_bytes()[:-30])
    elif damage == 'object_array':
        data = exact_output()
        data['transformer_data'] = np.full(data['transformer_data'].shape, None, dtype=object)
        np.savez(path, **data)
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f4', 'fortran_order': False, 'shape': (10 ** 12,)})
            archive.writestr('transformer_data.npy', header.getvalue())
            archive.writestr('query_distances.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('query_distances.npy', b'duplicate')
    result = POLICY.evaluate(ref, cand, BOUND)
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


def test_oversized_otherwise_valid_archive_is_rejected(tmp_path):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'equivalence.npz', **exact_output())
    (cand / 'equivalence.npz').write_bytes(b'\x00' * POLICY.SIZE_LIMIT + (ref / 'equivalence.npz').read_bytes())
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False


def test_cli_writes_failure_json_for_missing_outputs(tmp_path):
    result_path = tmp_path / 'result.json'
    command = [sys.executable, str(POLICY.HERE / 'validate.py'),
               '--reference', str(tmp_path / 'absent-ref'), '--candidate', str(tmp_path / 'absent-cand'),
               '--rubric', str(POLICY.HERE / 'rubric.json'), '--out', str(result_path)]
    run = subprocess.run(command, capture_output=True, text=True)
    assert run.returncode == 0, run.stderr
    assert json.loads(result_path.read_text())['passed'] is False


@pytest.mark.parametrize('side', ['reference', 'candidate', 'both'])
def test_extreme_finite_values_fail_through_complete_cli(tmp_path, side):
    directories = {name: tmp_path / name for name in ['reference', 'candidate']}
    for name, directory in directories.items():
        directory.mkdir()
        data = exact_output()
        if side == name or side == 'both':
            data['query_distances'] = np.full(data['query_distances'].shape,
                                              np.finfo(np.float32).max, dtype=np.float32)
        np.savez(directory / 'equivalence.npz', **data)
    result = tmp_path / 'extreme-result.json'
    command = [sys.executable, str(POLICY.HERE / 'validate.py'),
               '--reference', str(directories['reference']), '--candidate', str(directories['candidate']),
               '--rubric', str(POLICY.HERE / 'rubric.json'), '--out', str(result)]
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
    np.savez_compressed(ref / 'equivalence.npz', **exact_output())
    damaged = bytearray((ref / 'equivalence.npz').read_bytes())
    offset = 30 + int.from_bytes(damaged[26:28], 'little') + int.from_bytes(damaged[28:30], 'little')
    damaged[offset] = 7
    (cand / 'equivalence.npz').write_bytes(damaged)
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False


def test_the_report_carries_the_measured_recall_without_gating_on_it(tmp_path):
    inputs = POLICY.load_inputs()
    order = np.argsort(inputs['exact'], axis=1, kind='stable')
    poor = order[:, inputs['k']:2 * inputs['k']]
    distances = np.take_along_axis(inputs['exact'], poor, axis=1)
    by_id = np.argsort(poor, axis=1, kind='stable')
    candidate = exact_output(
        query_neighbor_ids=poor.astype(np.int64),
        query_distances=distances.astype(np.float32),
        transformer_indices=np.take_along_axis(poor, by_id, axis=1).ravel().astype(np.int64),
        transformer_data=np.take_along_axis(distances, by_id, axis=1).ravel().astype(np.float32))
    result = grade(tmp_path, exact_output(), candidate, name='poor')
    assert result['passed'], 'this node states no accuracy floor, so a poor but consistent answer passes'
    assert result['candidate']['recall'] < 0.5
    assert 'no accuracy floor' in result['reason']
