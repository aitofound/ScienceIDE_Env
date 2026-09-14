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
SPEC = importlib.util.spec_from_file_location('selfquery_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# Every threshold below is the upstream node's own; nothing here was chosen.
BOUND = {
    'distance_atol': 2e-6, 'distance_rtol': 2e-6,
    'min_self_found': {'dense_euclidean': 45, 'dense_angular': 45,
                       'sparse_euclidean': 40, 'sparse_angular': 40, 'bitpacked': 40},
}
QUERIES = 10
TRAIN = 24
VARIANTS = ('dense_euclidean', 'dense_angular', 'sparse_euclidean', 'sparse_angular', 'bitpacked')
INPUTS = {}


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own data. It never reads the real graded IC, the
    # pinned source or any private directory. Well separated rows make every
    # point's nearest neighbour unambiguously itself, which is what the upstream
    # nodes are about.
    dense = (np.arange(TRAIN, dtype=np.float32)[:, None] * np.array([1.0, 0.25, -0.5], np.float32)[None, :] + 1.0)
    bits = (np.arange(TRAIN, dtype=np.uint8)[:, None] * np.array([1, 3, 7], np.uint8)[None, :] + 1)
    inputs = {'dense': dense, 'angular': dense, 'sparse': dense.astype(np.float64),
              'sparse_norm': dense.astype(np.float64), 'bits': bits,
              'query_ids': np.arange(QUERIES, dtype=np.int64), 'train_ids': np.arange(TRAIN, dtype=np.int64)}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)
    monkeypatch.setattr(POLICY, 'QUERIES', QUERIES, raising=False)
    monkeypatch.setattr(POLICY, 'TRAIN', TRAIN, raising=False)
    monkeypatch.setattr(POLICY, 'DENSE_MEMBERS', {'dense_euclidean': 'dense', 'dense_angular': 'angular',
                                                  'sparse_euclidean': 'sparse', 'sparse_angular': 'sparse_norm',
                                                  'bitpacked': 'bits'}, raising=False)
    monkeypatch.setitem(globals(), 'INPUTS', inputs)


def exact(variant):
    return POLICY.exact_distances(variant, POLICY.load_inputs())


def output(hits=QUERIES, wrong_distance=None):
    members = {'query_ids': INPUTS['query_ids']}
    for variant in VARIANTS:
        matrix = exact(variant)
        ids = np.arange(QUERIES, dtype=np.int64)
        if hits < QUERIES:
            # Send the misses to their genuine second-nearest neighbour, which is
            # what a real miss looks like.
            for row in range(hits, QUERIES):
                ids[row] = int(np.argsort(matrix[row])[1])
        plain = np.take_along_axis(matrix, ids[:, None], axis=1)
        # bit_jaccard reports -ln(intersection/union); everything else reports an
        # ordinary distance. The producer emits whatever the kernel returns.
        distances = (-np.log(np.maximum(1.0 - plain, 1e-12)) if variant in POLICY.LOG_TRANSFORMED else plain).astype(np.float32)
        if wrong_distance == variant:
            distances = distances + 1.0
        members[f'{variant}_neighbor_ids'] = ids[:, None]
        members[f'{variant}_distances'] = distances
    return members


def grade(tmp_path, reference, candidate, name='pair'):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing self-query invariants policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'selfquery.npz', **reference)
    np.savez(cand / 'selfquery.npz', **candidate)
    return POLICY.evaluate(ref, cand, BOUND)


def test_every_artificial_point_is_its_own_nearest_neighbour():
    for variant in VARIANTS:
        matrix = exact(variant)
        assert np.array_equal(np.argmin(matrix, axis=1), np.arange(QUERIES)), variant
        second = np.sort(matrix, axis=1)[:, 1]
        assert second.min() > 0, f'{variant}: a distinct point sits at zero distance, so a miss is unobservable'


def test_a_perfect_self_query_passes(tmp_path):
    data = output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert result['candidate']['dense_euclidean']['self_found'] == QUERIES


@pytest.mark.parametrize('variant', VARIANTS)
def test_too_few_self_hits_fails_at_the_upstream_threshold(tmp_path, variant):
    ref = output()
    scaled = int(np.ceil(BOUND['min_self_found'][variant] * QUERIES / 50.0))
    candidate = {name: values.copy() for name, values in ref.items()}
    short = output(hits=scaled - 1)
    candidate[f'{variant}_neighbor_ids'] = short[f'{variant}_neighbor_ids']
    candidate[f'{variant}_distances'] = short[f'{variant}_distances']
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'upstream floor' in result['reason'] and variant in result['reason']


@pytest.mark.parametrize('variant', VARIANTS)
def test_exactly_the_upstream_threshold_passes(tmp_path, variant):
    # The bound is the upstream node's own >=, so landing exactly on it passes.
    ref = output()
    scaled = int(np.ceil(BOUND['min_self_found'][variant] * QUERIES / 50.0))
    candidate = {name: values.copy() for name, values in ref.items()}
    short = output(hits=scaled)
    candidate[f'{variant}_neighbor_ids'] = short[f'{variant}_neighbor_ids']
    candidate[f'{variant}_distances'] = short[f'{variant}_distances']
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('variant', VARIANTS)
def test_a_fabricated_distance_fails(tmp_path, variant):
    # Self-hits alone are cheap to claim; the reported distance has to be the real
    # distance to the point that was named.
    ref = output()
    candidate = output(wrong_distance=variant)
    result = grade(tmp_path, ref, candidate)
    assert not result['passed'] and 'distance' in result['reason']


def test_a_slightly_negative_reported_distance_is_accepted(tmp_path):
    # The real runs report -0.0 for the bit metric and about -1.2e-7 for cosine.
    # A nonnegativity guard here would reject a correct answer.
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['dense_angular_distances'] = candidate['dense_angular_distances'] - np.float32(1.19e-7)
    candidate['bitpacked_distances'] = candidate['bitpacked_distances'] * np.float32(-1.0)
    assert grade(tmp_path, ref, candidate)['passed']


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = output(wrong_distance='sparse_euclidean')
    result = grade(tmp_path, ref, output())
    assert not result['passed'] and result['reason'].startswith('reference')


@pytest.mark.parametrize('fault', ['unknown_id', 'duplicate_query', 'nan', 'infinity', 'missing_member', 'extra_member', 'float_ids', 'wrong_k'])
def test_rejects_wrong_identity_or_malformed_output(tmp_path, fault):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'unknown_id':
        candidate['dense_euclidean_neighbor_ids'][0, 0] = 999999
    elif fault == 'duplicate_query':
        candidate['query_ids'][0] = candidate['query_ids'][1]
    elif fault == 'nan':
        candidate['bitpacked_distances'][0, 0] = np.nan
    elif fault == 'infinity':
        candidate['bitpacked_distances'][0, 0] = np.inf
    elif fault == 'missing_member':
        del candidate['sparse_angular_distances']
    elif fault == 'extra_member':
        candidate['prepare_seconds'] = np.zeros(1)
    elif fault == 'float_ids':
        candidate['query_ids'] = candidate['query_ids'].astype(np.float64)
    else:
        candidate['dense_euclidean_neighbor_ids'] = np.repeat(candidate['dense_euclidean_neighbor_ids'], 2, axis=1)
    assert not grade(tmp_path, ref, candidate)['passed']


def test_row_permutation_is_accepted(tmp_path):
    ref = output()
    rows = np.arange(QUERIES)[::-1]
    candidate = {'query_ids': ref['query_ids'][rows]}
    for variant in VARIANTS:
        candidate[f'{variant}_neighbor_ids'] = ref[f'{variant}_neighbor_ids'][rows]
        candidate[f'{variant}_distances'] = ref[f'{variant}_distances'][rows]
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = output()
    candidate = {'query_ids': ref['query_ids'].astype('>i8')}
    for variant in VARIANTS:
        candidate[f'{variant}_neighbor_ids'] = ref[f'{variant}_neighbor_ids'].astype('>i8')
        candidate[f'{variant}_distances'] = ref[f'{variant}_distances'].astype(dtype)
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('bad', [{'distance_atol': 0.0}, {'distance_atol': float('nan')}, {'distance_rtol': -1.0},
                                 {'min_self_found': {'dense_euclidean': 0, 'dense_angular': 45, 'sparse_euclidean': 40, 'sparse_angular': 40, 'bitpacked': 40}},
                                 {'min_self_found': {'dense_euclidean': 45}}])
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
    np.savez(ref / 'selfquery.npz', **output())
    path = cand / 'selfquery.npz'
    if damage == 'garbage':
        path.write_bytes(b'not an archive')
    elif damage == 'truncated_zip':
        path.write_bytes((ref / 'selfquery.npz').read_bytes()[:-30])
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
    np.savez(ref / 'selfquery.npz', **output())
    (cand / 'selfquery.npz').write_bytes(b'\x00' * POLICY.SIZE_LIMIT + (ref / 'selfquery.npz').read_bytes())
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
        np.savez(directory / 'selfquery.npz', **data)
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
    np.savez_compressed(ref / 'selfquery.npz', **output())
    damaged = bytearray((ref / 'selfquery.npz').read_bytes())
    offset = 30 + int.from_bytes(damaged[26:28], 'little') + int.from_bytes(damaged[28:30], 'little')
    damaged[offset] = 7
    (cand / 'selfquery.npz').write_bytes(damaged)
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False


def test_the_bitpacked_distance_is_graded_in_its_log_space(tmp_path):
    # distances.py:1822-1847 returns -ln(intersection/union) and named_distances
    # registers no correction for bit_jaccard, so query() hands that value back
    # untransformed. Comparing it against a plain Jaccard distance would reject a
    # correct candidate as soon as it returns any neighbour that is not the query
    # itself, which the upstream floor of 40 of 50 explicitly permits. This test
    # exists because that is exactly the mistake this check shipped with at first.
    ref = output()
    matrix = exact('bitpacked')
    ids = np.arange(QUERIES, dtype=np.int64)
    ids[-1] = int(np.argsort(matrix[QUERIES - 1])[1])
    plain = np.take_along_axis(matrix, ids[:, None], axis=1)
    assert plain.max() > 0, 'the miss did not produce a nonzero Jaccard distance'
    correct = {name: values.copy() for name, values in ref.items()}
    correct['bitpacked_neighbor_ids'] = ids[:, None]
    correct['bitpacked_distances'] = (-np.log(np.maximum(1.0 - plain, 1e-12))).astype(np.float32)
    assert grade(tmp_path, ref, correct, name='log')['passed']

    naive = {name: values.copy() for name, values in ref.items()}
    naive['bitpacked_neighbor_ids'] = ids[:, None]
    naive['bitpacked_distances'] = plain.astype(np.float32)
    result = grade(tmp_path, ref, naive, name='plain')
    assert not result['passed'] and 'distance' in result['reason']
