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
SPEC = importlib.util.spec_from_file_location('dense_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
BOUND = {'atol': 1e-6, 'rtol': 2e-6}
INPUTS = {}


@pytest.fixture(autouse=True)
def artificial_inputs(monkeypatch, tmp_path):
    # 人工自测独立生成操作数，不读取真实 graded nominal 或任何私有目录。
    rng = np.random.RandomState(719)
    inputs = {'sample_ids': np.arange(40, 52, dtype=np.int64), 'points': np.vstack([rng.normal(size=(10, 20)), np.zeros((2, 20))]).astype(np.float32)}
    trusted = tmp_path / 'trusted-check'
    (trusted / 'ic/nominal').mkdir(parents=True)
    np.savez(trusted / 'ic/nominal/inputs.npz', **inputs)
    (trusted / 'validate.py').write_bytes((HERE / 'validate.py').read_bytes())
    (trusted / 'rubric.json').write_text(json.dumps({'comparison': BOUND}), encoding='utf-8')
    monkeypatch.setattr(POLICY, 'HERE', trusted, raising=False)
    monkeypatch.setitem(globals(), 'INPUTS', inputs)


def output():
    points = INPUTS['points'].astype(np.float64)
    ids = INPUTS['sample_ids']
    distances = np.sum(np.abs(points[:, None, :] - points[None, :, :]), axis=2)
    return {'sample_ids': ids, 'distances': distances}


def grade(tmp_path, reference, candidate):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing ID-aware distance-matrix policy'
    ref = tmp_path / 'reference'
    cand = tmp_path / 'candidate'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'distances.npz', **reference)
    np.savez(cand / 'distances.npz', **candidate)
    return POLICY.evaluate(ref, cand, BOUND)


def test_identical_physical_matrix_passes(tmp_path):
    data = output()
    result = grade(tmp_path, data, data)
    assert result['passed'] and result['distance'] == 0


def test_both_axes_follow_sample_identity(tmp_path):
    ref = output()
    order = np.arange(12)[::-1]
    candidate = {'sample_ids': ref['sample_ids'][order], 'distances': ref['distances'][np.ix_(order, order)]}
    result = grade(tmp_path, ref, candidate)
    assert result['passed'] and result['distance'] == 0


@pytest.mark.parametrize('fault', ['row_only', 'wrong_id', 'duplicate_id', 'float_ids', 'missing_row', 'nan', 'infinity', 'negative', 'wrong_distance', 'missing_member', 'extra_member'])
def test_rejects_wrong_binding_or_science(tmp_path, fault):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    if fault == 'row_only':
        candidate['sample_ids'] = candidate['sample_ids'][::-1]
        candidate['distances'] = candidate['distances'][::-1]
    elif fault == 'wrong_id':
        candidate['sample_ids'][0] = 9000
    elif fault == 'duplicate_id':
        candidate['sample_ids'][0] = candidate['sample_ids'][1]
    elif fault == 'float_ids':
        candidate['sample_ids'] = candidate['sample_ids'].astype(float)
    elif fault == 'missing_row':
        candidate['sample_ids'] = candidate['sample_ids'][:-1]
        candidate['distances'] = candidate['distances'][:-1, :-1]
    elif fault in ('nan', 'infinity', 'negative', 'wrong_distance'):
        candidate['distances'][0, 1] = {'nan': np.nan, 'infinity': np.inf, 'negative': -1, 'wrong_distance': 300}[fault]
    elif fault == 'missing_member':
        del candidate['sample_ids']
    else:
        candidate['success'] = np.array([True])
    assert not grade(tmp_path, ref, candidate)['passed']


def test_identical_but_wrong_matrices_do_not_pass(tmp_path):
    wrong = output()
    wrong['distances'][:] = 0
    assert not grade(tmp_path, wrong, wrong)['passed']


def test_bad_reference_is_rejected(tmp_path):
    ref = output()
    ref['distances'][1, 2] = np.nan
    assert not grade(tmp_path, ref, output())['passed']


def test_small_floating_difference_is_tolerated(tmp_path):
    ref = output()
    candidate = {name: values.copy() for name, values in ref.items()}
    candidate['distances'][0, 1] += 2e-7
    result = grade(tmp_path, ref, candidate)
    assert result['passed'] and result['distance'] > 0


@pytest.mark.parametrize('damage', ['garbage', 'truncated_zip', 'huge_header', 'duplicate_members', 'object_array'])
def test_invalid_archive_always_returns_failure(tmp_path, damage):
    assert callable(getattr(POLICY, 'evaluate', None)), 'Missing bounded archive validation'
    ref, cand = tmp_path / 'ref', tmp_path / 'cand'
    ref.mkdir()
    cand.mkdir()
    np.savez(ref / 'distances.npz', **output())
    path = cand / 'distances.npz'
    if damage == 'garbage':
        path.write_bytes(b'not an archive')
    elif damage == 'truncated_zip':
        path.write_bytes((ref / 'distances.npz').read_bytes()[:-30])
    elif damage == 'object_array':
        np.savez(path, sample_ids=output()['sample_ids'], distances=np.full((12, 12), None, dtype=object))
    else:
        with zipfile.ZipFile(path, 'w') as archive:
            header = io.BytesIO()
            np.lib.format.write_array_header_1_0(header, {'descr': '<f8', 'fortran_order': False, 'shape': (10**12,)})
            archive.writestr('distances.npy', header.getvalue())
            archive.writestr('sample_ids.npy', b'invalid')
            if damage == 'duplicate_members':
                with pytest.warns(UserWarning, match='Duplicate name'):
                    archive.writestr('sample_ids.npy', b'duplicate')
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
    np.savez(ref / 'distances.npz', **output())
    path = cand / 'distances.npz'
    path.write_bytes(b'\x00' * 1048576 + (ref / 'distances.npz').read_bytes())
    result = POLICY.evaluate(ref, cand, BOUND)
    assert result['passed'] is False


@pytest.mark.parametrize('atol', [-1.0, float('nan'), float('inf')])
def test_invalid_bounds_fail_without_nonfinite_json(tmp_path, atol):
    result = POLICY.evaluate(tmp_path, tmp_path, {'atol': atol, 'rtol': 2e-6})
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)


@pytest.mark.parametrize('dtype', ['<f4', '>f4', '<f8', '>f8'])
@pytest.mark.parametrize('layout', ['C', 'F'])
def test_legal_precision_byte_order_and_layout_pass(tmp_path, dtype, layout):
    ref = output()
    candidate = {'sample_ids': ref['sample_ids'].astype('>i8'), 'distances': np.array(ref['distances'], dtype=dtype, order=layout)}
    assert grade(tmp_path, ref, candidate)['passed']


@pytest.mark.parametrize('fault', ['wrong_norm', 'omitted_coordinate'])
def test_l1_formula_rejects_wrong_reduction(tmp_path, fault):
    ref = output()
    points = INPUTS['points'].astype(float)
    delta = np.abs(points[:, None, :] - points[None, :, :])
    if fault == 'wrong_norm':
        wrong = np.max(delta, axis=2)
    else:
        coordinate = np.unravel_index(np.argmax(delta), delta.shape)[2]
        wrong = np.sum(np.delete(delta, coordinate, axis=2), axis=2)
    assert not grade(tmp_path, ref, {'sample_ids': ref['sample_ids'], 'distances': wrong})['passed']


@pytest.mark.parametrize('side', ['reference', 'candidate', 'both'])
def test_extreme_finite_values_fail_through_complete_cli(tmp_path, side):
    directories = {name: tmp_path / name for name in ['reference', 'candidate']}
    for name, directory in directories.items():
        directory.mkdir()
        data = output()
        if side == name or side == 'both':
            data['distances'][:] = np.finfo(np.float64).max
        np.savez(directory / 'distances.npz', **data)
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
    np.savez_compressed(ref / 'distances.npz', **output())
    damaged = bytearray((ref / 'distances.npz').read_bytes())
    offset = 30 + int.from_bytes(damaged[26:28], 'little') + int.from_bytes(damaged[28:30], 'little')
    damaged[offset] = 7
    (cand / 'distances.npz').write_bytes(damaged)
    result = POLICY.evaluate(ref, cand, BOUND)
    assert result['passed'] is False
    json.dumps(result, allow_nan=False)
