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
SPEC = importlib.util.spec_from_file_location('alternative_corrections_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
# round_trip_atol and round_trip_rtol are numpy's isclose defaults, which is literally
# what the upstream assertion at test_distances.py:332 calls.
BOUND = {'atol': 1e-06, 'rtol': 1e-06, 'round_trip_atol': 1e-08, 'round_trip_rtol': 1e-05}
NAMES = ('euclidean', 'l2', 'cosine', 'dot', 'inner_product', 'true_angular', 'hellinger', 'jaccard')
PAIRS = 7
WIDTH = 11
SPEARMAN = 13


def artificial_arrays():
    """Our own pairs and rank inputs, at sizes different from the real 100x30 and
    100, with the same sparsification the node applies."""
    left = np.empty((len(NAMES), PAIRS, WIDTH), dtype=np.float32)
    right = np.empty_like(left)
    for index in range(len(NAMES)):
        for pair in range(PAIRS):
            base = ((np.arange(WIDTH) * 17 + index * 5 + pair * 3) % 23) / 23.0
            other = ((np.arange(WIDTH) * 29 + index * 7 + pair * 11) % 23) / 23.0
            base[base < 0.25] = 0.0
            other[other < 0.25] = 0.0
            left[index, pair] = base
            right[index, pair] = other
    x = ((np.arange(SPEARMAN) * 7) % 13).astype(np.float64) / 3.0
    y = ((np.arange(SPEARMAN) * 5 + 2) % 13).astype(np.float64) / 5.0
    return left, right, x, y


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # The selftest builds its own pairs and rank inputs. It never reads the real
    # graded IC, the pinned source or any private directory.
    left, right, x, y = artificial_arrays()
    inputs = {'names': np.array([name.encode('ascii') for name in NAMES]),
              'pairs_left': left, 'pairs_right': right, 'spearman_x': x, 'spearman_y': y}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)


def exact_output(**overrides):
    inputs = POLICY.load_inputs()
    members = {}
    for name in inputs['names']:
        truth = inputs['truth'][name]
        members[f'{name}_true'] = truth.copy()
        members[f'{name}_alternative'] = POLICY.invert_correction(name, truth)
    members['spearmanr_value'] = np.array([inputs['spearman']], dtype=np.float64)
    members.update(overrides)
    return members


def grade(tmp_path, reference, candidate, name='pair', bound=None):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing alternative-correction pass policy'
    base = tmp_path / name
    ref, cand = base / 'reference', base / 'candidate'
    ref.mkdir(parents=True)
    cand.mkdir(parents=True)
    np.savez(ref / 'alternatives.npz', **reference)
    np.savez(cand / 'alternatives.npz', **candidate)
    return POLICY.evaluate(ref, cand, bound or BOUND)


def test_the_artificial_inputs_are_not_the_real_ones():
    inputs = POLICY.load_inputs()
    assert inputs['left'].shape == (len(NAMES), PAIRS, WIDTH) != (8, 100, 30)
    assert inputs['x'].size == SPEARMAN != 100
    assert tuple(inputs['names']) == NAMES


def test_the_registry_order_is_the_upstream_one():
    # distances.py:2170-2188. Nothing added, nothing dropped, order preserved.
    assert tuple(POLICY.load_inputs()['names']) == (
        'euclidean', 'l2', 'cosine', 'dot', 'inner_product', 'true_angular', 'hellinger', 'jaccard')


def test_each_alternative_gets_its_own_pairs():
    inputs = POLICY.load_inputs()
    assert not np.array_equal(inputs['left'][0], inputs['left'][1]), (
        'reusing one set of pairs for every alternative would change what the node exercises')


def test_dot_and_inner_product_are_negative_here():
    # A nonnegativity guard would reject the reference. Measured on the real data
    # too: the ranges are -8.9 to -2.7 and -9.9 to -3.7.
    truth = POLICY.load_inputs()['truth']
    assert truth['inner_product'].min() < 0.0
    assert truth['dot'].min() < truth['cosine'].min()


def test_the_dense_jaccard_is_a_set_jaccard_on_the_support():
    # distances.py:270-281 treats NON-ZERO values as set membership for continuous
    # vectors. A weighted min-over-max jaccard is a different number, and that is
    # the formula the probe rejected before this validator was written.
    inputs = POLICY.load_inputs()
    left, right = inputs['left'][NAMES.index('jaccard')], inputs['right'][NAMES.index('jaccard')]
    weighted = 1.0 - np.minimum(left, right).sum(1) / np.maximum(np.maximum(left, right).sum(1), 1e-300)
    assert not np.allclose(inputs['truth']['jaccard'], weighted), (
        'the two jaccard readings are indistinguishable on this data, so the distinction is untested')


def test_an_exact_answer_passes(tmp_path):
    data = exact_output()
    result = grade(tmp_path, data, data)
    assert result['passed'], result['reason']
    assert result['distance'] == 0


@pytest.mark.parametrize('name', NAMES)
def test_a_wrong_true_distance_fails(tmp_path, name):
    candidate = exact_output()
    candidate[f'{name}_true'] = candidate[f'{name}_true'].copy()
    candidate[f'{name}_true'][0] += 1.0
    result = grade(tmp_path, exact_output(), candidate, name='t' + name)
    assert not result['passed'] and name in result['reason']


@pytest.mark.parametrize('name', NAMES)
def test_a_correction_that_does_not_round_trip_fails(tmp_path, name):
    # The node's actual claim: correction(alternative) == true. Moving the
    # alternative while leaving the true distance alone must break it.
    candidate = exact_output()
    candidate[f'{name}_alternative'] = candidate[f'{name}_alternative'].copy()
    candidate[f'{name}_alternative'][0] += 0.5
    result = grade(tmp_path, exact_output(), candidate, name='a' + name)
    assert not result['passed'] and name in result['reason'] and 'correction' in result['reason']


def test_an_alternative_reported_as_the_true_distance_fails(tmp_path):
    # The specific confusion this node exists to prevent: emitting the corrected
    # value where the raw alternative belongs.
    ref = exact_output()
    candidate = {name: value.copy() for name, value in ref.items()}
    candidate['cosine_alternative'] = ref['cosine_true'].copy()
    assert not np.allclose(ref['cosine_alternative'], ref['cosine_true'])
    assert not grade(tmp_path, ref, candidate, name='rawcos')['passed']


def test_the_wrong_correction_for_a_shared_alternative_fails(tmp_path):
    # cosine and true_angular share the SAME alternative kernel but different
    # corrections, so an implementation that keyed the correction off the
    # alternative rather than the metric name would swap them.
    inputs = POLICY.load_inputs()
    ref = exact_output()
    swapped = {name: value.copy() for name, value in ref.items()}
    swapped['true_angular_alternative'] = ref['cosine_alternative'].copy()
    if np.allclose(POLICY.apply_correction('true_angular', swapped['true_angular_alternative']),
                   ref['true_angular_true']):
        pytest.skip('the two alternatives coincide on this data')
    assert not grade(tmp_path, ref, swapped, name='swapcorr')['passed']


def test_a_wrong_spearman_value_fails(tmp_path):
    candidate = exact_output(spearmanr_value=np.array([0.5], dtype=np.float64))
    result = grade(tmp_path, exact_output(), candidate, name='spear')
    assert not result['passed'] and 'spearman' in result['reason'].lower()


def test_the_spearman_value_is_one_minus_the_correlation_not_the_correlation(tmp_path):
    inputs = POLICY.load_inputs()
    ref = exact_output()
    correlation = 1.0 - inputs['spearman']
    if abs(correlation - inputs['spearman']) < 1e-9:
        pytest.skip('the correlation is one half here, so the two readings coincide')
    candidate = exact_output(spearmanr_value=np.array([correlation], dtype=np.float64))
    assert not grade(tmp_path, ref, candidate, name='spearsign')['passed']


@pytest.mark.parametrize('fault', ['nan', 'infinite', 'missing_member', 'extra_member',
                                   'wrong_shape', 'integer_array', 'scalar_spearman'])
def test_rejects_malformed_output(tmp_path, fault):
    ref = exact_output()
    candidate = {name: value.copy() for name, value in ref.items()}
    if fault == 'nan':
        candidate['hellinger_true'][0] = np.nan
    elif fault == 'infinite':
        candidate['l2_alternative'][0] = np.inf
    elif fault == 'missing_member':
        del candidate['jaccard_true']
    elif fault == 'extra_member':
        candidate['seconds'] = np.zeros(1)
    elif fault == 'wrong_shape':
        candidate['cosine_true'] = candidate['cosine_true'][:-1]
    elif fault == 'integer_array':
        candidate['dot_true'] = candidate['dot_true'].astype(np.int64)
    else:
        candidate['spearmanr_value'] = np.zeros((2,), dtype=np.float64)
    assert not grade(tmp_path, ref, candidate, name=fault)['passed']


def test_a_bad_reference_fails_the_pair(tmp_path):
    ref = exact_output()
    broken = {name: value.copy() for name, value in ref.items()}
    broken['euclidean_true'][0] += 1.0
    result = grade(tmp_path, broken, exact_output(), name='badref')
    assert not result['passed'] and result['reason'].startswith('reference')


# float32 is accepted. It was MEASURED on the real reference that rounding to
# float32 leaves the worst bound fraction unchanged at 0.1604, so forbidding it
# would be strictness with nothing behind it - the same check that removed the
# equivalent restriction from sparse-distance-family.
@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
def test_legal_precision_and_byte_order_pass(tmp_path, dtype):
    ref = exact_output()
    candidate = {name: value.astype(dtype) for name, value in ref.items()}
    assert grade(tmp_path, ref, candidate, name=dtype.replace('<', 'lt').replace('>', 'gt'))['passed']


@pytest.mark.parametrize('bad', [{'atol': 0.0}, {'atol': float('nan')}, {'rtol': -1.0},
                                 {'round_trip_atol': 0.0}, {'round_trip_rtol': -1.0}])
def test_invalid_bounds_fail_without_nonfinite_json(tmp_path, bad):
    result = POLICY.evaluate(tmp_path, tmp_path, dict(BOUND, **bad))
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


@pytest.mark.parametrize('damage', ['garbage', 'truncated_zip', 'huge_header', 'duplicate_members', 'object_array'])
def test_invalid_archive_always_returns_failure(tmp_path, damage):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'alternatives.npz', **exact_output())
    path = cand / 'alternatives.npz'
    if damage == 'garbage':
        path.write_bytes(b'not an archive at all')
    elif damage == 'truncated_zip':
        path.write_bytes((ref / 'alternatives.npz').read_bytes()[:-30])
    elif damage == 'object_array':
        data = exact_output()
        data['cosine_true'] = np.full(data['cosine_true'].shape, None, dtype=object)
        np.savez(path, **data)
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f8', 'fortran_order': False, 'shape': (10 ** 12,)})
            archive.writestr('cosine_true.npy', header.getvalue())
            archive.writestr('cosine_alternative.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('cosine_alternative.npy', b'duplicate')
    result = POLICY.evaluate(ref, cand, BOUND)
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


def test_oversized_otherwise_valid_archive_is_rejected(tmp_path):
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'alternatives.npz', **exact_output())
    (cand / 'alternatives.npz').write_bytes(b'\x00' * POLICY.SIZE_LIMIT + (ref / 'alternatives.npz').read_bytes())
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
            data['hellinger_true'] = np.full(data['hellinger_true'].shape, np.finfo(np.float64).max)
        np.savez(directory / 'alternatives.npz', **data)
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
    np.savez_compressed(ref / 'alternatives.npz', **exact_output())
    damaged = bytearray((ref / 'alternatives.npz').read_bytes())
    offset = 30 + int.from_bytes(damaged[26:28], 'little') + int.from_bytes(damaged[28:30], 'little')
    damaged[offset] = 7
    (cand / 'alternatives.npz').write_bytes(damaged)
    assert POLICY.evaluate(ref, cand, BOUND)['passed'] is False


def test_the_report_names_every_alternative(tmp_path):
    data = exact_output()
    result = grade(tmp_path, data, data, name='report')
    for name in NAMES:
        assert name in result['candidate'], name
    assert 'spearmanr' in result['candidate']
