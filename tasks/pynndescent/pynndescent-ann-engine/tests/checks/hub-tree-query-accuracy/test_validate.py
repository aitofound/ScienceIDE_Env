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
SPEC = importlib.util.spec_from_file_location('queryaccuracy_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# Every floor below is the upstream node's own; nothing here was chosen.
BOUND = {
    'distance_atol': 2e-6, 'distance_rtol': 2e-6,
    'tie_atol': 2e-7, 'tie_rtol': 2e-6,
    'min_recall': {'dense_euclidean': 0.90, 'dense_angular': 0.90,
                   'sparse_euclidean': 0.85, 'sparse_angular': 0.85, 'bitpacked': 0.70},
}
QUERIES = 10
TRAIN = 40
K = 4
VARIANTS = ('dense_euclidean', 'dense_angular', 'sparse_euclidean', 'sparse_angular', 'bitpacked')
INPUTS = {}


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own data. It never reads the real graded IC, the
    # pinned source or any private directory. Queries are held out of the training
    # set exactly as the upstream nodes hold them out, and the rows are spread so
    # each query has an unambiguous top-K.
    rows = QUERIES + TRAIN
    angles = np.linspace(0.05, 2.9, rows)
    dense = np.stack([np.cos(angles), np.sin(angles), angles / 10.0], axis=1).astype(np.float32) * 2.0
    bits = np.stack([(np.arange(rows) % 251 + 1).astype(np.uint8),
                     (np.arange(rows) * 7 % 241 + 3).astype(np.uint8),
                     (np.arange(rows) * 13 % 233 + 5).astype(np.uint8)], axis=1)
    inputs = {'dense': dense, 'angular': dense, 'sparse': dense.astype(np.float64),
              'sparse_norm': dense.astype(np.float64), 'bits': bits,
              'query_ids': np.arange(QUERIES, dtype=np.int64),
              'train_ids': np.arange(QUERIES, rows, dtype=np.int64)}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)
    monkeypatch.setattr(POLICY, 'QUERIES', QUERIES, raising=False)
    monkeypatch.setattr(POLICY, 'TRAIN', TRAIN, raising=False)
    monkeypatch.setattr(POLICY, 'K', K, raising=False)
    monkeypatch.setattr(POLICY, 'DENSE_MEMBERS', {'dense_euclidean': 'dense', 'dense_angular': 'angular',
                                                  'sparse_euclidean': 'sparse', 'sparse_angular': 'sparse_norm',
                                                  'bitpacked': 'bits'}, raising=False)
    monkeypatch.setitem(globals(), 'INPUTS', inputs)


def exact(variant):
    return POLICY.exact_distances(variant, POLICY.load_inputs())


def report(variant, distances):
    """Emit in the space the kernel reports: log for bit_jaccard, plain otherwise."""
    if variant in POLICY.LOG_TRANSFORMED:
        return (-np.log(np.maximum(1.0 - distances, 1e-12))).astype(np.float32)
    return distances.astype(np.float32)


def output(offset=0, wrong_distance=None, only=None):
    members = {'query_ids': INPUTS['query_ids']}
    for variant in VARIANTS:
        matrix = exact(variant)
        shift = offset if (only is None or only == variant) else 0
        order = np.argsort(matrix, axis=1, kind='stable')[:, shift:shift + K]
        ids = INPUTS['train_ids'][order]
        plain = np.take_along_axis(matrix, order, axis=1)
        distances = report(variant, plain)
        if wrong_distance == variant:
            distances = distances + np.float32(0.5)
        members[f'{variant}_neighbor_ids'] = ids
        members[f'{variant}_distances'] = distances
    return members


def grade(tmp_path, reference, candidate, name='pair'):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing query-accuracy invariants policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'queries.npz', **reference)
    np.savez(cand / 'queries.npz', **candidate)
    return POLICY.evaluate(ref, cand, BOUND)


def test_the_artificial_split_holds_the_queries_out_and_a_shift_is_observable():
    # Ties at the cutoff are fine and the tie-aware recall absorbs them. What the
    # suite actually needs is that shifting the whole answer by K ranks drops the
    # recall below every floor, so the degradation tests are not vacuous.
    assert not np.isin(INPUTS['query_ids'], INPUTS['train_ids']).any()
    for variant in VARIANTS:
        matrix = exact(variant)
        assert matrix.shape == (QUERIES, TRAIN), variant
        order = np.argsort(matrix, axis=1, kind='stable')
        shifted = POLICY.tie_aware_recall(matrix, order[:, K:2 * K], BOUND['tie_atol'], BOUND['tie_rtol']).mean()
        assert shifted < BOUND['min_recall'][variant], (
            variant + ': a K-rank shift still scores ' + str(shifted) + ', at or above its floor, so the degradation test would be vacuous')


def test_an_exact_answer_passes(tmp_path):
    data = output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert result['candidate']['dense_euclidean']['global_recall'] == 1.0


def test_the_bitpacked_distance_is_graded_in_its_log_space(tmp_path):
    # distances.py:1822-1847 returns -ln(intersection/union) and named_distances
    # registers no correction for bit_jaccard, so query() hands that back
    # untransformed. The sibling self-query check shipped with this wrong; here it
    # is pinned from the start, in both directions.
    ref = output()
    matrix = exact('bitpacked')
    order = np.argsort(matrix, axis=1, kind='stable')[:, :K]
    plain = np.take_along_axis(matrix, order, axis=1)
    assert plain.max() > 0, 'the artificial bit data has no nonzero Jaccard distance to grade'
    naive = {name: values.copy() for name, values in ref.items()}
    naive['bitpacked_distances'] = plain.astype(np.float32)
    result = grade(tmp_path, ref, naive, name='plain')
    assert not result['passed'] and 'distance' in result['reason']
    assert grade(tmp_path, ref, output(), name='log')['passed']


@pytest.mark.parametrize('variant', VARIANTS)
def test_a_graph_shifted_far_enough_fails_its_own_upstream_floor(tmp_path, variant):
    ref = output()
    candidate = output(offset=K, only=variant)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'recall' in result['reason'] and variant in result['reason']


@pytest.mark.parametrize('variant', VARIANTS)
def test_a_fabricated_distance_fails(tmp_path, variant):
    ref = output()
    candidate = output(wrong_distance=variant)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'distance' in result['reason']


def test_the_floors_differ_per_variant_as_upstream_states_them(tmp_path):
    # A single shared floor would be wrong: upstream asks 0.90 of the dense
    # variants, 0.85 of the sparse ones and only 0.70 of the bit-packed one.
    assert BOUND['min_recall']['dense_euclidean'] > BOUND['min_recall']['sparse_euclidean'] > BOUND['min_recall']['bitpacked']
    data = output()
    result = grade(tmp_path, data, data)
    floors = {variant: result['candidate'][variant]['upstream_floor'] for variant in VARIANTS}
    assert floors == BOUND['min_recall']


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = output(wrong_distance='dense_angular')
    result = grade(tmp_path, ref, output())
    assert not result['passed'] and result['reason'].startswith('reference')


@pytest.mark.parametrize('fault', ['unknown_id', 'duplicate_neighbour', 'duplicate_query', 'nan', 'infinity', 'missing_member', 'extra_member', 'float_ids', 'wrong_k'])
def test_rejects_wrong_identity_or_malformed_output(tmp_path, fault):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'unknown_id':
        candidate['dense_euclidean_neighbor_ids'][0, 0] = 999999
    elif fault == 'duplicate_neighbour':
        candidate['dense_euclidean_neighbor_ids'][0, 1] = candidate['dense_euclidean_neighbor_ids'][0, 0]
    elif fault == 'duplicate_query':
        candidate['query_ids'][0] = candidate['query_ids'][1]
    elif fault == 'nan':
        candidate['bitpacked_distances'][0, 0] = np.nan
    elif fault == 'infinity':
        candidate['bitpacked_distances'][0, 0] = np.inf
    elif fault == 'missing_member':
        del candidate['sparse_angular_distances']
    elif fault == 'extra_member':
        candidate['epsilon'] = np.zeros(1)
    elif fault == 'float_ids':
        candidate['query_ids'] = candidate['query_ids'].astype(np.float64)
    else:
        candidate['dense_euclidean_neighbor_ids'] = candidate['dense_euclidean_neighbor_ids'][:, :-1]
    assert not grade(tmp_path, ref, candidate)['passed']


def test_a_query_identity_inside_the_training_set_is_rejected(tmp_path, monkeypatch):
    # These nodes hold the queries out. An IC that did not would make "recall"
    # trivially satisfiable by returning the query itself.
    trusted = POLICY.HERE
    with np.load(trusted / 'ic/nominal/inputs.npz', allow_pickle=False) as data:
        members = {key: data[key] for key in data.files}
    members['train_ids'] = np.arange(TRAIN, dtype=np.int64)
    np.savez(trusted / 'ic/nominal/inputs.npz', **members)
    result = POLICY.evaluate(trusted, trusted, BOUND)
    assert result['passed'] is False


def test_row_and_slot_permutations_are_accepted(tmp_path):
    ref = output()
    rows = np.arange(QUERIES)[::-1]
    slots = np.arange(K)[::-1]
    candidate = {'query_ids': ref['query_ids'][rows]}
    for variant in VARIANTS:
        candidate[f'{variant}_neighbor_ids'] = ref[f'{variant}_neighbor_ids'][rows][:, slots]
        candidate[f'{variant}_distances'] = ref[f'{variant}_distances'][rows][:, slots]
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = output()
    candidate = {'query_ids': ref['query_ids'].astype('>i8')}
    for variant in VARIANTS:
        candidate[f'{variant}_neighbor_ids'] = ref[f'{variant}_neighbor_ids'].astype('>i8')
        candidate[f'{variant}_distances'] = ref[f'{variant}_distances'].astype(dtype)
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('bad', [{'distance_atol': 0.0}, {'distance_atol': float('nan')}, {'tie_rtol': -1.0},
                                 {'min_recall': {'dense_euclidean': 0.0, 'dense_angular': 0.9, 'sparse_euclidean': 0.85, 'sparse_angular': 0.85, 'bitpacked': 0.7}},
                                 {'min_recall': {'dense_euclidean': 1.0, 'dense_angular': 0.9, 'sparse_euclidean': 0.85, 'sparse_angular': 0.85, 'bitpacked': 0.7}},
                                 {'min_recall': {'dense_euclidean': 0.9}}])
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
        data['dense_euclidean_distances'] = np.full(data['dense_euclidean_distances'].shape, None, dtype=object)
        np.savez(path, **data)
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f8', 'fortran_order': False, 'shape': (10**12,)})
            archive.writestr('dense_euclidean_distances.npy', header.getvalue())
            archive.writestr('query_ids.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('query_ids.npy', b'duplicate')
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
            for variant in VARIANTS:
                data[f'{variant}_distances'][:] = np.finfo(np.float32).max
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
