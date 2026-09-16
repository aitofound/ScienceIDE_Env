#!/usr/bin/env python3
"""独立人工 validator 回归；仅依赖标准库与 NumPy，不读取生产初值或源码。"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path
from unittest.mock import patch

import numpy as np

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("derive_mpp_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)

CASES = ("pitch_hex", "pitch_hex_scaled", "pitch_square", "diameter_points",
         "square_edge_polygons", "pitch_large_grid")
FIELD = "mpp"
ULP4 = 4 * float(np.finfo(np.float64).eps)


_TRUTH = VALIDATOR.recompute(HERE / "ic" / "nominal")


def payload():
    """基线取判分器对 `ic/nominal` 的**独立重算**，不再是编造的常数。

    判分器现在带第三条腿，会把物理上不对的 mpp 拒掉；用合成值做基线，测的只是
    比较逻辑自己跟自己。
    """
    import numpy as _np
    out = {"case": _np.asarray(VALIDATOR.CASES)}
    for field in VALIDATOR.FIELDS:
        out[field] = _np.array(_TRUTH[field], copy=True)
    return out

def rubric():
    return {"policy": "pointwise", "comparison": {"fields": {FIELD: {"atol": 0.0, "rtol": ULP4}}}}


class DeriveMppValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="derivempp-selftest-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reference = self.root / "reference"
        self.candidate = self.root / "candidate"
        self.reference.mkdir()
        self.candidate.mkdir()
        self.contract = self.root / "rubric.json"
        self.contract.write_text(json.dumps(rubric()))
        self.report = self.root / "result.json"

    def call_main(self, out=None):
        argv = ["validate.py", "--reference", str(self.reference), "--candidate", str(self.candidate),
                "--rubric", str(self.contract), "--out", str(out or self.report)]
        errors = io.StringIO()
        failure = None
        code = None
        with patch.object(sys, "argv", argv), contextlib.redirect_stderr(errors):
            try:
                code = VALIDATOR.main()
            except Exception as exc:
                failure = exc
        return code, failure, errors.getvalue()

    def read_report(self, require_ascii=False):
        raw = self.report.read_bytes()
        if require_ascii:
            self.assertTrue(raw.isascii(), "JSON 必须使用 ASCII escape，UTF-8 编码不能遗留 surrogate")
        data = json.loads(raw.decode("utf-8"), parse_constant=lambda word: self.fail("非法 JSON 数值 " + word))
        self.assertIsInstance(data["passed"], bool)
        return data

    def run_pair(self, ref, cand):
        if ref is not None:
            np.savez_compressed(self.reference / "result.npz", **ref)
        if cand is not None:
            np.savez_compressed(self.candidate / "result.npz", **cand)
        code, failure, stderr = self.call_main()
        self.assertIsNone(failure, f"结果协议意外抛出 {failure!r}; stderr={stderr!r}")
        self.assertEqual(code, 0)
        return self.read_report()

    def test_all_six_cases_are_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        self.assertEqual(result["fields"][FIELD]["values"], len(CASES))
        self.assertEqual(result["distance"], 0)

    def test_bound_is_a_representation_margin_not_a_physical_tolerance(self):
        """界限只有几个 ULP 宽：界内的偏离通过、界外的被拒。

        这条把「bound 是表示余量」变成可执行的断言——取 1e-9 的话两者都会通过。
        **不写死 ULP 个数**：`rtol = 4·eps` 只有在 [1,2) 上才恰好是 4 ULP，
        在别的二进制区间是 4~8 个，写死就会随合成值的量级失效（第一版就是这么坏的）。
        """
        base = payload()[FIELD][0]
        bound_ulps = ULP4 * abs(base) / np.spacing(base)
        self.assertLess(bound_ulps, 16, "界限应当是几个 ULP 量级，不是物理容差")
        for ulps, expected_pass in ((bound_ulps * 0.5, True), (bound_ulps * 2.0, False)):
            with self.subTest(ulps=round(ulps, 3)):
                ref, cand = payload(), payload()
                cand[FIELD][0] = base + ulps * np.spacing(base)
                self.assertEqual(self.run_pair(ref, cand)["passed"], expected_pass)

    def test_case_identity_not_row_order(self):
        for seed in (0, 13):
            with self.subTest(seed=seed):
                ref, cand = payload(), payload()
                order = np.random.default_rng(seed).permutation(len(CASES))
                cand["case"] = cand["case"][order]
                cand[FIELD] = cand[FIELD][order]
                self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_one_case_wrong_is_rejected(self):
        for index in range(len(CASES)):
            with self.subTest(case=CASES[index]):
                ref, cand = payload(), payload()
                cand[FIELD][index] *= 1.000001
                self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_nonpositive_and_nonfinite_mpp_are_rejected(self):
        for side in ("reference", "candidate"):
            for value in (0.0, -1.0, np.nan, np.inf):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    (ref if side == "reference" else cand)[FIELD][2] = value
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertEqual(result["fields"], {})

    def test_integer_mpp_dtype_is_rejected(self):
        ref, cand = payload(), payload()
        cand[FIELD] = np.ones(len(CASES), dtype=np.int64)
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(result["fields"], {})

    def test_schema_and_identity_rejections(self):
        for fault in ("duplicate_case", "unknown_case", "numeric_axis", "missing_field",
                      "missing_axis", "extra", "wrong_shape", "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_case":
                    cand["case"] = np.asarray((CASES[0],) + CASES[:-1])
                elif fault == "unknown_case":
                    cand["case"] = np.asarray(("bogus",) + CASES[1:])
                elif fault == "numeric_axis":
                    cand["case"] = np.arange(len(CASES))
                elif fault == "missing_field":
                    del cand[FIELD]
                elif fault == "missing_axis":
                    del cand["case"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "wrong_shape":
                    cand[FIELD] = np.ones(len(CASES) - 1)
                elif fault == "object":
                    cand[FIELD] = cand[FIELD].astype(object)
                else:
                    (self.candidate / "result.npz").unlink(missing_ok=True)
                    cand = None
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})

    def write_archive(self, method=zipfile.ZIP_STORED, fault=None):
        with zipfile.ZipFile(self.candidate / "result.npz", "w", compression=method) as archive:
            for name, value in payload().items():
                buffer = io.BytesIO()
                if fault == "huge_header" and name == FIELD:
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<f8", "fortran_order": False, "shape": (2**40,)})
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr(FIELD + ".npy", buffer.getvalue())

    def test_all_standard_zip_compressions_remain_accepted(self):
        for method in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED, zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA):
            with self.subTest(compression=method):
                self.write_archive(method=method)
                self.assertTrue(self.run_pair(payload(), None)["passed"])

    def test_corrupt_duplicate_and_huge_header_rejections(self):
        for fault in ("corrupt", "duplicate", "huge_header"):
            with self.subTest(fault=fault):
                if fault == "corrupt":
                    (self.candidate / "result.npz").write_bytes(b"not a zip")
                else:
                    self.write_archive(fault=fault)
                self.assertFalse(self.run_pair(payload(), None)["passed"])

    def test_oversized_archive_is_rejected_before_decompression(self):
        with zipfile.ZipFile(self.candidate / "result.npz", "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, value in payload().items():
                buffer = io.BytesIO()
                np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            archive.writestr("case.npy", np.zeros(VALIDATOR.MAX_BYTES, dtype=np.uint8).tobytes())
        self.assertFalse(self.run_pair(payload(), None)["passed"])

    def test_invalid_tolerances_use_failed_result(self):
        for value in (-1, float("nan"), True, "NaN"):
            with self.subTest(value=value):
                contract = rubric()
                contract["comparison"]["fields"][FIELD]["rtol"] = value
                self.contract.write_text(json.dumps(contract))
                self.assertFalse(self.run_pair(payload(), payload())["passed"])

    def test_unknown_exception_with_surrogate_emits_fresh_failed_result(self):
        class UnknownFailure(Exception):
            pass

        with patch.object(VALIDATOR, "compare", side_effect=UnknownFailure("unsafe surrogate: \ud800")):
            code, failure, stderr = self.call_main()
        self.assertIsNone(failure, repr(failure))
        self.assertEqual(code, 0)
        result = self.read_report(require_ascii=True)
        self.assertFalse(result["passed"])
        self.assertEqual(result["fields"], {})
        self.assertIn("UnknownFailure", result["reason"])
        self.assertIn("Traceback", stderr)

    def test_partial_pass_nan_and_object_cannot_escape_serialization_guard(self):
        for invalid in (float("nan"), object()):
            with self.subTest(invalid_type=type(invalid).__name__):
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {FIELD: invalid}}
                with patch.object(VALIDATOR, "compare", return_value=partial):
                    code, failure, stderr = self.call_main()
                self.assertIsNone(failure, repr(failure))
                self.assertEqual(code, 0)
                result = self.read_report(require_ascii=True)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})
                self.assertIsNone(result["distance"])
                self.assertIn("Traceback", stderr)

    def test_successful_reason_surrogate_is_safely_escaped(self):
        valid = {"passed": True, "reason": "surrogate: \ud800", "distance": 0, "fields": {}}
        with patch.object(VALIDATOR, "compare", return_value=valid):
            code, failure, _ = self.call_main()
        self.assertIsNone(failure, repr(failure))
        self.assertEqual(code, 0)
        self.assertTrue(self.read_report(require_ascii=True)["passed"])

    def test_cancellation_baseexceptions_are_not_swallowed(self):
        for cancel in (KeyboardInterrupt(), SystemExit(23)):
            with self.subTest(type=type(cancel).__name__):
                self.report.unlink(missing_ok=True)
                with patch.object(VALIDATOR, "compare", side_effect=cancel):
                    with self.assertRaises(type(cancel)):
                        self.call_main()
                self.assertFalse(self.report.exists())

    def test_real_output_io_failure_is_not_relabelled_as_scientific_failure(self):
        valid = {"passed": True, "reason": "ok", "distance": 0, "fields": {}}
        with patch.object(VALIDATOR, "compare", return_value=valid):
            _, failure, _ = self.call_main(out=self.root)
        self.assertIsInstance(failure, OSError)



    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐项比较结构上拒不掉，只有独立重算能拒。"""
        ref, cand = payload(), payload()
        for values in (ref, cand):
            values["mpp"][0] *= 1.01
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_third_leg_covers_every_graded_value(self):
        result = self.run_pair(payload(), payload())
        m = result["measurements"]
        self.assertTrue(result["passed"])
        self.assertEqual(m["items_with_a_third_leg"], m["graded_items"])
        self.assertIs(m["third_leg_is_partial"], False)

if __name__ == "__main__":
    unittest.main(verbosity=2)
