"""最终CLI边界自测：真实小型归档及可控意外异常，不执行生产科学代码。"""
import importlib.util
import io
import json
import lzma
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

import numpy as np

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("local_contract_tests", HERE / "selftest_validate.py")
fixtures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixtures)

# 子进程运行真实main；仅注入协议故障，不替代科学判定或生成参考答案。
PROBE = r'''
import importlib.util
from pathlib import Path
import sys
validator_path, mode, side, *arguments = sys.argv[1:]
spec = importlib.util.spec_from_file_location("validator_under_test", validator_path)
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)
if mode in ("decoder", "decoder-unicode", "cancel"):
    original_load = validator.np.load
    target = Path(arguments[arguments.index("--" + side) + 1])
    class UnexpectedDecoderError(Exception):
        pass
    def injected_load(path, *args, **kwargs):
        if Path(path).parent == target:
            if mode == "cancel":
                raise KeyboardInterrupt("cancel protocol probe")
            message = "decoder protocol probe: " + str(path)
            if mode == "decoder-unicode":
                message += chr(0xD800)
            raise UnexpectedDecoderError(message)
        return original_load(path, *args, **kwargs)
    validator.np.load = injected_load
elif mode in ("serialize-object", "serialize-nonfinite", "serialize-unicode"):
    original_dumps = validator.json.dumps
    def injected_dumps(value, *args, **kwargs):
        if isinstance(value, dict) and value.get("passed") is True:
            value = dict(value)
            if mode == "serialize-unicode":
                value["reason"] = chr(0xD800)
                kwargs["ensure_ascii"] = False
            else:
                value["distance"] = object() if mode == "serialize-object" else float("nan")
        return original_dumps(value, *args, **kwargs)
    validator.json.dumps = injected_dumps
sys.argv = [validator_path] + arguments
raise SystemExit(validator.main())
'''


class CliBoundaryTests(unittest.TestCase):
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)
        self.reference, self.candidate = self.root / "reference", self.root / "candidate"
        self.reference.mkdir()
        self.candidate.mkdir()
        for root in (self.reference, self.candidate):
            np.savez(root / "assignment.npz", **fixtures.specimen("assignment"))
        case = {"path": "assignment.npz", "kind": "assignment", "edges": [["p", "x"], ["p", "y"]],
                "leaf_states": {"x": "C", "y": "A"}, "states": ["A", "C", "G"]}
        self.rubric = self.root / "rubric.json"
        self.rubric.write_text(json.dumps({"policy": "invariants", "comparison": {"atol": 0, "rtol": 0, "files": [case]}}))
        self.validator = HERE / "validate.py"
        self.extra_arguments = []
        self.output = self.root / "protocol-result.json"
        self.filename = sorted(self.reference.glob("*.npz"))[0].name
        self.original = (self.reference / self.filename).read_bytes()
        self.command = [sys.executable, str(self.validator), "--reference", str(self.reference),
                        "--candidate", str(self.candidate), "--rubric", str(self.rubric),
                        "--out", str(self.output)] + self.extra_arguments

    def invoke(self, mode=None, side="candidate", expected=True, error=None):
        # 预存pass用于证明失败必须写全新结果，不能静默复用旧输出或半成品。
        self.output.write_text('{"passed":true,"stale_success":true}')
        command = self.command if mode is None else [
            sys.executable, "-c", PROBE, str(self.validator), mode, side, *self.command[2:]]
        process = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        def reject_constant(token):
            raise AssertionError("非严格JSON常量: " + token)
        result = json.loads(self.output.read_text(), parse_constant=reject_constant)
        self.assertIs(result["passed"], expected, result)
        self.assertNotIn("stale_success", result)
        if not expected:
            self.assertIsNone(result["distance"], result)
            self.assertIsNone(result["bound_fraction"], result)
            self.assertEqual(result.get("files", {}), {}, result)
            self.assertIn(error, result["reason"])
            self.assertIn(error, process.stderr)
            self.assertIn("Traceback", process.stderr)
        return result

    def lzma_archive(self, damaged):
        with zipfile.ZipFile(io.BytesIO(self.original)) as archive:
            entries = {name: archive.read(name) for name in archive.namelist()}
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_LZMA) as archive:
            for name, payload in entries.items():
                archive.writestr(name, payload)
        raw = bytearray(stream.getvalue())
        with zipfile.ZipFile(stream) as archive:
            info = archive.infolist()[0]
            self.assertEqual(archive.read(info), entries[info.filename])
        if damaged:
            name_length, extra_length = struct.unpack_from("<HH", raw, info.header_offset + 26)
            start = info.header_offset + 30 + name_length + extra_length
            # ZIP-LZMA前四字节是版本和属性长度，仅损坏后面的LZMA属性。
            raw[start + 4] = 255
            with zipfile.ZipFile(io.BytesIO(raw)) as archive:
                self.assertEqual(archive.namelist(), list(entries))
                self.assertEqual(archive.getinfo(info.filename).file_size, len(entries[info.filename]))
                with self.assertRaises(lzma.LZMAError):
                    archive.read(info.filename)
            with np.load(io.BytesIO(raw), allow_pickle=False) as archive:
                self.assertEqual(set(archive.files), {name[:-4] for name in entries})
                with self.assertRaises(lzma.LZMAError):
                    archive[info.filename[:-4]]
        return bytes(raw)

    def test_healthy_lzma_on_either_side_passes(self):
        for side in (self.reference, self.candidate):
            with self.subTest(side=side.name):
                for root in (self.reference, self.candidate):
                    (root / self.filename).write_bytes(self.original)
                (side / self.filename).write_bytes(self.lzma_archive(False))
                self.invoke()

    def test_damaged_lzma_on_either_side_fails_closed(self):
        for side in (self.reference, self.candidate):
            with self.subTest(side=side.name):
                for root in (self.reference, self.candidate):
                    (root / self.filename).write_bytes(self.original)
                (side / self.filename).write_bytes(self.lzma_archive(True))
                self.invoke(expected=False, error="_lzma.LZMAError")

    def test_unexpected_decoder_exception_on_either_side_fails_closed(self):
        for side in ("reference", "candidate"):
            with self.subTest(side=side):
                result = self.invoke("decoder", side, expected=False, error="__main__.UnexpectedDecoderError")
                self.assertIn("decoder protocol probe", result["reason"])
                self.assertIn(str(self.root / side), result["reason"])

    def test_serialization_error_replaces_success_with_safe_failure(self):
        for mode, error in (("serialize-object", "builtins.TypeError"),
                            ("serialize-nonfinite", "builtins.ValueError")):
            with self.subTest(mode=mode):
                result = self.invoke(mode, expected=False, error=error)
                self.assertIn("strict JSON serialization", result["reason"])

    def test_bad_unicode_exception_on_either_side_fails_closed(self):
        for side in ("reference", "candidate"):
            with self.subTest(side=side):
                result = self.invoke("decoder-unicode", side, expected=False,
                                     error="__main__.UnexpectedDecoderError")
                self.assertIn(chr(0xD800), result["reason"])
                self.assertTrue(self.output.read_bytes().isascii())

    def test_non_utf8_result_is_replaced_before_file_is_opened(self):
        result = self.invoke("serialize-unicode", expected=False, error="builtins.UnicodeEncodeError")
        self.assertIn("strict JSON serialization", result["reason"])
        self.assertTrue(self.output.read_bytes().isascii())

    def test_keyboard_interrupt_is_not_converted_to_failure_json(self):
        command = [sys.executable, "-c", PROBE, str(self.validator), "cancel", "candidate", *self.command[2:]]
        process = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(process.returncode, 0)
        self.assertIn("KeyboardInterrupt", process.stderr)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
