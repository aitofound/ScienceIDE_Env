"""开发自测：普通计算异常生成新失败记录，取消和真实输出 I/O 不被吞掉。"""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

CHECK = Path(__file__).resolve().parent


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('validator_protocol_under_test', CHECK / 'validate.py')
        self.validator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.validator)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.rubric = self.root / 'rubric.json'
        self.rubric.write_text(json.dumps({'comparison': {}, '备注': '科学协议'}, ensure_ascii=False), encoding='utf-8')
        self.out = self.root / 'result.json'
        self.argv = ['validate.py', '--reference', str(self.root / 'reference'), '--candidate', str(self.root / 'candidate'), '--rubric', str(self.rubric), '--out', str(self.out)]

    def invoke(self, error):
        with patch.object(sys, 'argv', self.argv), patch.object(self.validator, 'compare', side_effect=error):
            return self.validator.main()

    def test_unexpected_ordinary_exception_becomes_failed_json(self):
        self.assertEqual(self.invoke(RuntimeError('未预期的生产解码错误')), 0)
        result = json.loads(self.out.read_text(encoding='utf-8'))
        self.assertIs(result['passed'], False)
        self.assertIsNone(result['distance'])

    def test_error_with_surrogate_writes_ascii_safe_json(self):
        self.assertEqual(self.invoke(ValueError('非法文件名\udcff')), 0)
        payload = self.out.read_bytes()
        self.assertTrue(payload.isascii())
        self.assertIs(json.loads(payload)['passed'], False)

    def test_failed_record_replaces_stale_success_and_partial_details(self):
        self.out.write_text('{"passed":true,"distance":0,"columns":{"partial":"passed"}}', encoding='utf-8')
        self.assertEqual(self.invoke(ValueError('候选读取失败')), 0)
        result = json.loads(self.out.read_text(encoding='utf-8'))
        self.assertIs(result['passed'], False)
        self.assertNotIn('columns', result)
        self.assertIsNone(result['bound_fraction'])

    def test_nonfinite_partial_success_is_replaced_on_serialization_failure(self):
        partial = {'passed': True, 'reason': '不应保留', 'distance': float('inf'), 'columns': {'partial': 'passed'}}
        with patch.object(sys, 'argv', self.argv), patch.object(self.validator, 'compare', return_value=partial):
            self.assertEqual(self.validator.main(), 0)
        result = json.loads(self.out.read_text(encoding='utf-8'))
        self.assertIs(result['passed'], False)
        self.assertNotIn('columns', result)
        self.assertIsNone(result['distance'])

    def test_nonserializable_partial_success_is_replaced(self):
        partial = {'passed': True, 'reason': '不应保留', 'distance': {1, 2}}
        with patch.object(sys, 'argv', self.argv), patch.object(self.validator, 'compare', return_value=partial):
            self.assertEqual(self.validator.main(), 0)
        self.assertIs(json.loads(self.out.read_text(encoding='utf-8'))['passed'], False)

    def test_keyboard_interrupt_is_not_swallowed(self):
        with self.assertRaises(KeyboardInterrupt):
            self.invoke(KeyboardInterrupt())
        self.assertFalse(self.out.exists())

    def test_system_exit_is_not_swallowed(self):
        with self.assertRaises(SystemExit):
            self.invoke(SystemExit(9))
        self.assertFalse(self.out.exists())

    def test_output_io_failure_is_not_reported_as_scientific_failure(self):
        self.out.mkdir()
        result = {'passed': True, 'reason': '数值通过', 'distance': 0.0, 'bound_fraction': 0.0}
        with patch.object(sys, 'argv', self.argv), patch.object(self.validator, 'compare', return_value=result):
            with self.assertRaises(IsADirectoryError):
                self.validator.main()


if __name__ == '__main__':
    unittest.main()
