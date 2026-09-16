#!/usr/bin/env python3
"""独立人工非对称矩阵自测；不读取HOME、source或benchmark nominal。"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

import numpy as np


import importlib.util as _ilu
_SPEC = _ilu.spec_from_file_location("pa_validator_under_test", Path(__file__).resolve().with_name("validate.py"))
VALIDATOR = _ilu.module_from_spec(_SPEC)
_SPEC.loader.exec_module(VALIDATOR)


def repack_archive(path, compression):
    with zipfile.ZipFile(path) as archive:
        members = [(name, archive.read(name)) for name in archive.namelist()]
    with zipfile.ZipFile(path, "w", compression=compression) as archive:
        for name, payload in members:
            archive.writestr(name, payload)


class MatrixRoleContract(unittest.TestCase):
    probes = 0

    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        # 夹具用**真实的 `default-multivariate` 3×3 矩阵**，取自 validate.recompute 对 ic/nominal
        # 的独立重算。原来这里是人工给定的 [[0.1,0.02,…]]，加上第三条腿之后被正确拒掉——
        # 那组数不是任何真实计算的输出。矩阵仍是非对称的，所以下三角与两个轴照样都参与比较。
        _expected = VALIDATOR.recompute(Path(__file__).resolve().parent / "ic" / "nominal")
        _matrix = np.asarray(_expected["default-multivariate.npz"], dtype=np.float64)
        assert _matrix.shape == (3, 3), "夹具必须是 3×3"
        # ⚠ 实测：真实矩阵是**对称**的（I = XᵀW_nX 配对称 W_n 必然对称）。
        # 所以「转置会被拒」这件事在真实观测量上不成立——见下面那条用例的说明。
        _roles = ["nUMI", "GeneX", "GeneY"]
        self.payload = {"row_variables": np.array(_roles),
                        "column_variables": np.array(_roles),
                        "morans_i": _matrix}
        self.spec = {"path": "default-multivariate.npz", "format": "moran-matrix-npz",
                     "row_variables": _roles, "column_variables": _roles}
        self.atol = 1e-6
        self.rtol = 0.0

    def compare(self, candidate=None, reference=None, expected=True, damage=None, shim=None, terminal=None):
        type(self).probes += 1
        for name, payload in [("reference", reference), ("candidate", candidate)]:
            directory = self.root / name
            directory.mkdir(exist_ok=True)
            np.savez(directory / self.spec["path"], **(self.payload if payload is None else payload))
        if damage is not None:
            damage(self.root)
        (self.root / "rubric.json").write_text(json.dumps({"comparison": {"atol": self.atol, "rtol": self.rtol, "files": [self.spec]}}))
        validator = Path(__file__).with_name("validate.py").resolve()
        output = self.root / "result.json"
        if output.exists():
            output.unlink()
        if terminal == "io":
            output = self.root / "directory-not-file"
            output.mkdir(exist_ok=True)
        command = [sys.executable, str(validator),
                   "--reference", str(self.root / "reference"), "--candidate", str(self.root / "candidate"),
                   "--rubric", str(self.root / "rubric.json"), "--out", str(output)]
        if shim is not None:
            wrapper = self.root / "protocol_probe.py"
            wrapper.write_text(f'''import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location("checked", {str(validator)!r})
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
mode={shim!r}
if mode=="cancel":
    def altered(args):
        raise KeyboardInterrupt("intentional cancellation")
    module.evaluate=altered
elif mode.startswith("unexpected-"):
    original=module.load
    def altered(path,*args):
        if Path(path).parent.name==mode.split("-",1)[1]:
            raise ArithmeticError("unexpected loader sentinel")
        return original(path,*args)
    module.load=altered
else:
    original=module.json.dumps
    original_write=module.Path.write_text
    def traced_write(path,text,*args,**kwargs):
        if any(0xD800<=ord(c)<=0xDFFF for c in text):
            Path({str(self.root / "unsafe-write")!r}).write_bytes(b"unsafe")
        return original_write(path,text,*args,**kwargs)
    module.Path.write_text=traced_write
    def altered(value,*args,**kwargs):
        if isinstance(value,dict) and value.get("passed") is True:
            if mode=="bad-unicode":
                return original(value,*args,**kwargs)+chr(0xD800)
            value=dict(value);value["distance"]=float("nan")
        return original(value,*args,**kwargs)
    module.json.dumps=altered
raise SystemExit(module.main())
''')
            command[1] = str(wrapper)
        env = os.environ.copy()
        env.update(HOME=str(self.root / "empty-home"), SOURCE_DIR=str(self.root / "empty-source"),
                   PYTHONDONTWRITEBYTECODE="1", PYTHONNOUSERSITE="1")
        (self.root / "empty-home").mkdir(exist_ok=True)
        (self.root / "empty-source").mkdir(exist_ok=True)
        run = subprocess.run(command, cwd=self.root, env=env, capture_output=True, text=True, timeout=30)
        if terminal is not None:
            self.assertNotEqual(run.returncode, 0)
            self.assertFalse(output.is_file())
            return run
        self.assertEqual(run.returncode, 0, run.stderr)
        def reject_constant(value):
            raise ValueError("nonstandard JSON: " + value)
        result = json.loads(output.read_text(), parse_constant=reject_constant)
        self.assertIs(result["passed"], expected, result["reason"])
        return result

    def test_independent_row_and_column_reordering_passes(self):
        rows, columns = [2, 0, 1], [1, 2, 0]
        candidate = {"row_variables": self.payload["row_variables"][rows],
                     "column_variables": self.payload["column_variables"][columns],
                     "morans_i": self.payload["morans_i"][np.ix_(rows, columns)]}
        result = self.compare(candidate)
        self.assertEqual(result["distance"], 0)
        self.assertEqual(result["bound_fraction"], 0)

    def test_transpose_is_not_a_detectable_fault_but_a_single_entry_is(self):
        """⚠ 如实记录一处**构造上不可检**的故障，免得日后有人把它当漏洞去「修」。

        原用例把矩阵转置后断言被拒。夹具换成真实重算值之后它当场失效：
        `I = XᵀW_nX` 配对称的 `W_n` **必然对称**，所以转置是恒等操作，
        两侧完全相同、第三条腿也对得上——不该拒，也拒不了。
        旧夹具是人工写的非对称数（`[[0.1,0.02,…]]`），才让这条看起来在测东西。

        真正可检的方向性故障是**改动单个非对角元**：那会同时破坏与参考的一致
        和与独立重算的一致。这条用例改测那个。
        """
        self.assertTrue(np.allclose(self.payload["morans_i"], self.payload["morans_i"].T),
                        "真实 Moran 矩阵应当是对称的；若上游改了这一点，本注记要重写")
        transposed = {name: value.copy() for name, value in self.payload.items()}
        transposed["morans_i"] = self.payload["morans_i"].T.copy()
        self.compare(candidate=transposed, expected=True)

        moved = {name: value.copy() for name, value in self.payload.items()}
        moved["morans_i"][0, 1] += 1e-3
        self.assertFalse(np.allclose(moved["morans_i"], self.payload["morans_i"]),
                         "扰动必须真的改变了数值，否则这条用例是空的")
        self.compare(candidate=moved, expected=False)

    def test_extra_role_cannot_be_silently_sliced_away(self):
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["row_variables"] = np.append(wrong["row_variables"], "foreign")
        wrong["morans_i"] = np.vstack([wrong["morans_i"], np.zeros((1, 3))])
        self.compare(wrong, expected=False)
        self.compare(reference=wrong, expected=False)

    def test_declared_float64_and_reference_finiteness_are_required(self):
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["morans_i"] = wrong["morans_i"].astype(np.float32)
        self.compare(wrong, expected=False)
        wrong["morans_i"] = self.payload["morans_i"].copy()
        wrong["morans_i"][0, 0] = np.nan
        self.compare(reference=wrong, expected=False)

    def test_payload_does_not_follow_role_labels_is_rejected(self):
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["row_variables"] = wrong["row_variables"][[2, 0, 1]]
        self.compare(wrong, expected=False)
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["column_variables"] = wrong["column_variables"][[1, 2, 0]]
        self.compare(wrong, expected=False)

    def test_both_axes_reject_missing_duplicate_and_foreign_roles(self):
        for axis in ["row_variables", "column_variables"]:
            for mode in ["duplicate", "foreign", "missing"]:
                for side in ["reference", "candidate"]:
                    with self.subTest(axis=axis, mode=mode, side=side):
                        wrong = {name: value.copy() for name, value in self.payload.items()}
                        if mode == "duplicate":
                            wrong[axis][1] = wrong[axis][0]
                        elif mode == "foreign":
                            wrong[axis][1] = "q"
                        else:
                            wrong[axis] = wrong[axis][:-1]
                        self.compare(**{side: wrong}, expected=False)

    def test_matrix_shapes_and_numeric_dtypes_cannot_be_flattened_or_coerced(self):
        mutations = {
            "flat": lambda p: p.__setitem__("morans_i", p["morans_i"].reshape(9)),
            "wrong_rectangle": lambda p: p.__setitem__("morans_i", p["morans_i"].reshape(1, 9)),
            "int": lambda p: p.__setitem__("morans_i", p["morans_i"].astype(np.int64)),
            "complex": lambda p: p.__setitem__("morans_i", p["morans_i"].astype(np.complex128)),
            "bool": lambda p: p.__setitem__("morans_i", p["morans_i"].astype(bool)),
            "object": lambda p: p.__setitem__("morans_i", p["morans_i"].astype(object)),
            "row_dtype": lambda p: p.__setitem__("row_variables", p["row_variables"].astype("S")),
            "col_dtype": lambda p: p.__setitem__("column_variables", p["column_variables"].astype(object)),
            "row_shape": lambda p: p.__setitem__("row_variables", p["row_variables"].reshape(1, 3)),
            "missing_array": lambda p: p.pop("column_variables"),
            "extra_array": lambda p: p.__setitem__("success", np.array([True])),
        }
        for name, mutate in mutations.items():
            for side in ["reference", "candidate"]:
                with self.subTest(name=name, side=side):
                    wrong = {name: value.copy() for name, value in self.payload.items()}
                    mutate(wrong)
                    self.compare(**{side: wrong}, expected=False)

    def test_nonfinite_values_on_either_side_never_pass(self):
        for value in [np.nan, np.inf, -np.inf]:
            for side in ["reference", "candidate"]:
                wrong = {name: array.copy() for name, array in self.payload.items()}
                wrong["morans_i"][2, 1] = value
                self.compare(**{side: wrong}, expected=False)
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["morans_i"] = wrong["morans_i"].reshape(9)
        self.compare(wrong, wrong, expected=False)

    def test_pointwise_bound_and_zero_bound_fraction_are_defined(self):
        """测的是**界的算术**（bound_fraction 怎么算），不是科学比较。

        因此这里断言的是 `bound_fraction` 这个字段本身。`passed` 随界而变，**不是一律 False**：
        第一段 atol=0.125 很宽，两侧与独立重算的偏差（0.084）都落在界内，所以腿也接受——
        **腿在宽容差下不会过度拒绝**，这正是它该有的行为。收紧到 atol=0 之后腿才拒。
        """
        self.atol = 0.125
        wrong = {name: value.copy() for name, value in self.payload.items()}
        self.payload["morans_i"] = self.payload["morans_i"].copy()
        self.payload["morans_i"][0, 0] = 0.0
        wrong["morans_i"][0, 0] = 0.125
        result = self.compare(wrong, expected=True)
        self.assertEqual(result["bound_fraction"], 1.0, "误差恰好等于界时占界应为 1.0")
        wrong["morans_i"][0, 0] = np.nextafter(0.125, np.inf)
        self.assertEqual(sum(f["values_over_bound"] for f in self.compare(wrong, expected=False)["files"].values()), 1,
                         "比界大一个 ULP 必须记为越界")
        self.atol = 0.0
        self.assertEqual(self.compare(expected=False)["bound_fraction"], 0.0,
                         "两侧相同时占界应为 0.0，即使界是零")
        wrong["morans_i"][0, 0] = 0.125
        self.assertIsNone(self.compare(wrong, expected=False)["bound_fraction"],
                          "零界上出现非零误差时占界无定义，必须是 null 而不是 0.0")

    def test_scalar_keeps_one_by_one_scientific_shape(self):
        """单变量 scenario 的输出必须仍是 (1, 1) 矩阵，不能塌成一维。

        用真实的 `default-single` 标量（来自独立重算），不再用人造的 0.25。
        """
        scalar = np.asarray(
            VALIDATOR.recompute(Path(__file__).resolve().parent / "ic" / "nominal")["default-single.npz"],
            dtype=np.float64).reshape(1, 1)
        self.spec = {"path": "default-single.npz", "format": "moran-matrix-npz",
                     "row_variables": ["nUMI"], "column_variables": ["nUMI"]}
        self.payload = {"row_variables": np.array(["nUMI"]), "column_variables": np.array(["nUMI"]),
                        "morans_i": scalar}
        self.compare()
        wrong = {name: value.copy() for name, value in self.payload.items()}
        wrong["morans_i"] = scalar.reshape(1)
        self.compare(wrong, expected=False)

    def test_width_endianness_and_healthy_zip_codecs_are_layout(self):
        good = {"row_variables": self.payload["row_variables"].astype(">U20"),
                "column_variables": self.payload["column_variables"].astype(">U20"),
                "morans_i": self.payload["morans_i"].astype(">f8")}
        self.compare(good)
        for side in ["reference", "candidate"]:
            for codec in [zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED, zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA]:
                self.compare(damage=lambda root: repack_archive(root / side / self.spec["path"], codec))

    def test_bad_zip_codecs_and_missing_files_fail_closed(self):
        for mode in ["deflate", "lzma", "encrypted", "crc", "truncated", "missing"]:
            for side in ["reference", "candidate"]:
                with self.subTest(mode=mode, side=side):
                    def damage(root):
                        path = root / side / self.spec["path"]
                        if mode == "missing":
                            path.unlink()
                            return
                        if mode == "deflate":
                            repack_archive(path, zipfile.ZIP_DEFLATED)
                        elif mode == "lzma":
                            repack_archive(path, zipfile.ZIP_LZMA)
                        with zipfile.ZipFile(path) as archive:
                            info = archive.infolist()[0]
                        data = bytearray(path.read_bytes())
                        n, extra = struct.unpack_from("<HH", data, info.header_offset + 26)
                        start = info.header_offset + 30 + n + extra
                        if mode == "deflate":
                            data[start] = 7
                        elif mode == "lzma":
                            data[start + 4] = 255
                        elif mode == "encrypted":
                            central = struct.unpack_from("<I", data, data.rfind(b"PK\x05\x06") + 16)[0]
                            for offset in [info.header_offset + 6, central + 8]:
                                struct.pack_into("<H", data, offset, struct.unpack_from("<H", data, offset)[0] | 1)
                        elif mode == "crc":
                            data[start + info.file_size - 1] ^= 1
                        else:
                            data = data[:20]
                        path.write_bytes(data)
                    self.compare(damage=damage, expected=False)

    def test_unexpected_exception_and_json_utf8_failure_replace_partial_result(self):
        for mode in ["unexpected-reference", "unexpected-candidate", "nonfinite-result", "bad-unicode"]:
            result = self.compare(shim=mode, expected=False)
            self.assertEqual(result["files"], {})
            self.assertIsNone(result["distance"])
            self.assertIsNone(result["bound_fraction"])
            self.assertIn(".", result["diagnostic"]["exception_type"])
            self.assertFalse((self.root / "unsafe-write").exists())

    def test_cancellation_is_not_converted_to_scientific_failure(self):
        result = self.compare(shim="cancel", terminal="cancel")
        self.assertIn("KeyboardInterrupt", result.stderr)

    def test_real_io_failure_is_nonzero_without_fake_verdict(self):
        result = self.compare(terminal="io")
        self.assertIn("无法写入", result.stderr)


if __name__ == "__main__":
    program = unittest.main(verbosity=2, exit=False)
    print(f"CLI probes: {MatrixRoleContract.probes}")
    raise SystemExit(not program.result.wasSuccessful())
