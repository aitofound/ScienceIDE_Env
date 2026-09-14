#!/usr/bin/env python3
"""本 check 的人工科学值/身份及失败协议自测；仅 stdlib/numpy。"""
import contextlib
import importlib.util
import io
import json
import lzma
import struct
import zipfile
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

HERE = Path(__file__).resolve().parent
MEASUREMENTS = [
    {'call_id': 'a', 'observable': 'stochastic_missing_probability', 'domain': 'probability'},
    {'call_id': 'a', 'observable': 'heritable_missing_rate', 'domain': 'rate'},
    {'call_id': 'b', 'observable': 'mutation_rate', 'domain': 'rate'},
]


def fixture():
    return {'call_ids': np.array(['a', 'a', 'b']),
            'observables': np.array(['stochastic_missing_probability', 'heritable_missing_rate', 'mutation_rate']),
            'values': np.array([0.1, 0.25, 0.5], dtype=np.float64)}


def archive_bytes(codec, damage=None):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=codec) as archive:
        for key, value in fixture().items():
            member = io.BytesIO()
            np.save(member, value, allow_pickle=False)
            archive.writestr(key + '.npy', member.getvalue())
    payload = bytearray(stream.getvalue())
    if damage == 'EOF':
        return b''
    if damage == 'BadZip':
        return bytes(payload[:-22])
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        items, central = archive.infolist(), archive.start_dir
    if damage == 'encrypted':
        for item in items:
            offset = item.header_offset + 6
            struct.pack_into('<H', payload, offset, struct.unpack_from('<H', payload, offset)[0] | 1)
        while payload[central:central + 4] == b'PK\x01\x02':
            offset = central + 8
            struct.pack_into('<H', payload, offset, struct.unpack_from('<H', payload, offset)[0] | 1)
            central += 46 + sum(struct.unpack_from('<HHH', payload, central + 28))
    elif damage == 'compressed':
        item = items[0]
        start = item.header_offset + 30 + sum(struct.unpack_from('<HH', payload, item.header_offset + 26))
        end = start + item.compress_size
        if codec == zipfile.ZIP_LZMA:
            start += 4 + struct.unpack_from('<H', payload, start + 2)[0]
        payload[start:end] = b'\xff' * (end - start)
    return bytes(payload)


class UnexpectedDecoderFailure(Exception):
    pass


class ParameterContract(unittest.TestCase):
    def verdict(self, reference, candidate, atol=1e-12, rtol=1e-10):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for side, arrays in [('reference', reference), ('candidate', candidate)]:
                (root / side).mkdir()
                if isinstance(arrays, bytes):
                    (root / side / 'results.npz').write_bytes(arrays)
                else:
                    np.savez(root / side / 'results.npz', **arrays)
            rubric = {'comparison': {'atol': atol, 'rtol': rtol, 'measurements': MEASUREMENTS}}
            (root / 'rubric.json').write_text(json.dumps(rubric))
            result = subprocess.run([sys.executable, str(HERE / 'validate.py'),
                '--reference', str(root / 'reference'), '--candidate', str(root / 'candidate'),
                '--rubric', str(root / 'rubric.json'), '--out', str(root / 'result.json')],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            wire = (root / 'result.json').read_bytes()
            wire.decode('ascii')
            return json.loads(wire)

    def test_full_identity_payload_permutation_is_legal(self):
        reference = fixture()
        candidate = {key: value[[2, 1, 0]] for key, value in reference.items()}
        result = self.verdict(reference, candidate)
        self.assertTrue(result['passed'], result)
        self.assertEqual(result['distance'], 0)

    def test_producer_preserves_method_reset_call_order_and_complete_tuple(self):
        spec = importlib.util.spec_from_file_location('parameter_producer_test', HERE / 'produce.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        created, observed = [], []
        class Tree:
            def __init__(self):
                self.parameters = {}
        def factory(specification):
            tree = Tree()
            created.append(tree)
            return {'t': tree}
        def missing(tree, **kwargs):
            observed.append((tree, dict(tree.parameters), kwargs))
            return (0.1, kwargs.get('heritable_missing_rate', tree.parameters.get('heritable_missing_rate')))
        def mutation(tree, **kwargs):
            observed.append((tree, dict(tree.parameters), kwargs))
            return 0.3
        first = {'call_id': 'first', 'api': 'estimate_missing_data_rates', 'tree': 't',
                 'observables': ['stochastic_missing_probability', 'heritable_missing_rate'],
                 'kwargs': {'heritable_missing_rate': 0.25}, 'updates_before': []}
        second = dict(first, call_id='second', kwargs={}, updates_before=[
            {'tree': 't', 'parameter': 'heritable_missing_rate', 'value': 0.5}])
        third = {'call_id': 'third', 'api': 'estimate_mutation_rate', 'tree': 't',
                 'observables': ['mutation_rate'], 'kwargs': {'continuous': True}, 'updates_before': []}
        inputs = {'trees': {}, 'methods': [{'selector': 'one', 'calls': [first, second]},
                                        {'selector': 'two', 'calls': [third]}]}
        arrays, diagnostics = module.collect(inputs, factory, {'estimate_missing_data_rates': missing,
                                                             'estimate_mutation_rate': mutation})
        self.assertEqual(len(created), 2)
        self.assertIs(observed[0][0], observed[1][0])
        self.assertIsNot(observed[0][0], observed[2][0])
        self.assertEqual([entry[1] for entry in observed], [{}, {'heritable_missing_rate': 0.5}, {}])
        self.assertEqual(arrays['values'].tolist(), [0.1, 0.25, 0.1, 0.5, 0.3])
        self.assertEqual(arrays['call_ids'].tolist(), ['first', 'first', 'second', 'second', 'third'])
        self.assertEqual(len(diagnostics['calls']), 3)

    def test_two_ulp_noise_passes_but_material_parameter_change_fails(self):
        candidate = fixture()
        candidate['values'][0] = np.nextafter(np.nextafter(0.1, np.inf), np.inf)
        result = self.verdict(fixture(), candidate)
        self.assertTrue(result['passed'])
        self.assertGreater(result['distance'], 0)
        candidate['values'][0] += 1e-4
        self.assertFalse(self.verdict(fixture(), candidate)['passed'])

    def test_missing_extra_and_duplicate_components_fail(self):
        for indexes in [[0, 2], [0, 0, 2], [0, 1, 2, 2]]:
            candidate = {key: value[indexes] for key, value in fixture().items()}
            self.assertFalse(self.verdict(fixture(), candidate)['passed'])
        candidate = fixture()
        candidate['call_ids'] = np.array(['a', 'a', 'unknown'])
        self.assertFalse(self.verdict(fixture(), candidate)['passed'])

    def test_probability_range_and_known_tuple_component_are_enforced(self):
        for changed in [-0.1, 1.1]:
            candidate = fixture()
            candidate['values'][0] = changed
            self.assertFalse(self.verdict(fixture(), candidate)['passed'])
        candidate = fixture()
        candidate['values'][1] += 0.01
        self.assertFalse(self.verdict(fixture(), candidate)['passed'])

    def test_shape_dtype_naninf_and_field_schema_fail_on_both_sides(self):
        changes = [('values', np.array([0.1, 0.25, 0.5], dtype=np.float32)),
                   ('values', np.array([0, 1, 1], dtype=np.int64)),
                   ('values', np.array([[0.1, 0.25, 0.5]])),
                   ('values', np.array([0.1, 0.25], dtype=np.float64)),
                   ('values', np.array([0.1, np.nan, 0.5])),
                   ('values', np.array([0.1, np.inf, 0.5])),
                   ('values', np.array([0.1, -np.inf, 0.5])),
                   ('values', np.array([0.1, 0.25, 0.5], dtype=object)),
                   ('call_ids', np.array([b'a', b'a', b'b']))]
        for key, value in changes:
            for side in ['reference', 'candidate']:
                with self.subTest(key=key, value=str(value), dtype=str(value.dtype), side=side):
                    broken = fixture()
                    broken[key] = value
                    args = (broken, fixture()) if side == 'reference' else (fixture(), broken)
                    self.assertFalse(self.verdict(*args)['passed'])
        for action in ['extra', 'missing']:
            broken = fixture()
            if action == 'extra':
                broken['extra'] = np.array([1])
            else:
                del broken['observables']
            self.assertFalse(self.verdict(fixture(), broken)['passed'])

    def test_zero_bound_and_extreme_error_keep_strict_json(self):
        self.assertTrue(self.verdict(fixture(), fixture(), atol=0, rtol=0)['passed'])
        candidate = fixture()
        candidate['values'][2] += 0.01
        result = self.verdict(fixture(), candidate, atol=0, rtol=0)
        self.assertFalse(result['passed'])
        self.assertIsNone(result['bound_fraction'])
        candidate['values'][2] = 1e308
        self.assertFalse(self.verdict(fixture(), candidate)['passed'])

    def test_healthy_codecs_pass_on_both_sides(self):
        for codec in [zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED, zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA]:
            for side in ['reference', 'candidate']:
                with self.subTest(codec=codec, side=side):
                    healthy = archive_bytes(codec)
                    args = (healthy, fixture()) if side == 'reference' else (fixture(), healthy)
                    self.assertTrue(self.verdict(*args)['passed'])

    def test_corrupt_archives_fail_closed_on_both_sides(self):
        for codec, damage in [(zipfile.ZIP_STORED, 'EOF'), (zipfile.ZIP_STORED, 'BadZip'),
                              (zipfile.ZIP_DEFLATED, 'encrypted'), (zipfile.ZIP_DEFLATED, 'compressed'),
                              (zipfile.ZIP_BZIP2, 'compressed'), (zipfile.ZIP_LZMA, 'compressed')]:
            broken = archive_bytes(codec, damage)
            if codec == zipfile.ZIP_LZMA:
                with zipfile.ZipFile(io.BytesIO(broken)) as archive:
                    with self.assertRaises(lzma.LZMAError):
                        archive.read(archive.namelist()[0])
            for side in ['reference', 'candidate']:
                with self.subTest(codec=codec, damage=damage, side=side):
                    args = (broken, fixture()) if side == 'reference' else (fixture(), broken)
                    result = self.verdict(*args)
                    self.assertFalse(result['passed'])
                    self.assertIsNone(result['distance'])
                    self.assertIsNone(result['bound_fraction'])
                    self.assertIn(side, result['context'])

    def protocol_fault(self, target, failure, *, cancel=False):
        spec = importlib.util.spec_from_file_location('parameter_validator_protocol', HERE / 'validate.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for side in ['reference', 'candidate']:
                (root / side).mkdir()
                np.savez(root / side / 'results.npz', **fixture())
            (root / 'rubric.json').write_text(json.dumps({'comparison': {'atol': 1e-12, 'rtol': 1e-10,
                                                                       'measurements': MEASUREMENTS}}))
            output = root / 'result.json'
            output.write_text('旧结果不能被失败编码截断')
            argv = ['validate.py', '--reference', str(root / 'reference'), '--candidate', str(root / 'candidate'),
                    '--rubric', str(root / 'rubric.json'), '--out', str(output)]
            options = {'side_effect': failure} if isinstance(failure, BaseException) else {'return_value': failure}
            stderr = io.StringIO()
            with patch.object(module, target, **options), patch.object(sys, 'argv', argv), contextlib.redirect_stderr(stderr):
                if cancel:
                    with self.assertRaises(type(failure)):
                        module.main()
                    self.assertEqual(output.read_text(), '旧结果不能被失败编码截断')
                    return
                self.assertEqual(module.main(), 0)
            wire = output.read_bytes()
            wire.decode('ascii')
            result = json.loads(wire)
            self.assertFalse(result['passed'])
            self.assertIsNone(result['distance'])
            self.assertIsNone(result['bound_fraction'])
            self.assertNotIn('measurements', result)
            self.assertIn('.', result['error_type'])
            self.assertIn('Traceback', stderr.getvalue())
            return result

    def test_unexpected_decoder_and_bad_unicode_are_safe(self):
        for error in [UnexpectedDecoderFailure('unexpected'), ValueError('坏字符' + chr(0xD800))]:
            result = self.protocol_fault('load', error)
            self.assertIn('reference', result['context'])

    def test_serializer_error_cannot_keep_a_partial_pass(self):
        for value in [object(), float('nan')]:
            result = self.protocol_fault('compare', {'passed': True, 'distance': 0.0,
                                                    'bound_fraction': 0.0, 'measurements': value})
            self.assertIn('JSON', result['context'])

    def test_baseexception_is_not_swallowed(self):
        for error in [KeyboardInterrupt(), SystemExit(7)]:
            self.protocol_fault('load', error, cancel=True)

    def test_swapping_tuple_components_without_labels_is_not_legal(self):
        candidate = fixture()
        candidate['values'] = candidate['values'][[1, 0, 2]]
        self.assertFalse(self.verdict(fixture(), candidate)['passed'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
