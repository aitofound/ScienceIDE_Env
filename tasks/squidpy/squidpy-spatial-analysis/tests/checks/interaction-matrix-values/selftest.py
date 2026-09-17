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
SPEC = importlib.util.spec_from_file_location("interaction_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


def payload():
    return {
        "source_cluster": np.array(["a", "b"]),
        "target_cluster": np.array(["a", "b"]),
        "weighted": np.array([[5, 1], [2, 3]]),
        "unweighted": np.array([[4, 1], [2, 2]]),
    }


def rubric():
    return {
        "policy": "pointwise",
        "comparison": {"fields": {name: {"atol": 0, "rtol": 0} for name in ("weighted", "unweighted")}},
    }


class InteractionValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="interaction-selftest-")
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
        argv = [
            "validate.py",
            "--reference",
            str(self.reference),
            "--candidate",
            str(self.candidate),
            "--rubric",
            str(self.contract),
            "--out",
            str(out or self.report),
        ]
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
            np.savez(self.reference / "result.npz", **ref)
        if cand is not None:
            np.savez(self.candidate / "result.npz", **cand)
        code, failure, stderr = self.call_main()
        self.assertIsNone(failure, f"结果协议意外抛出 {failure!r}; stderr={stderr!r}")
        self.assertEqual(code, 0)
        return self.read_report()

    def test_all_eight_values_are_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        self.assertEqual(sum(field["values"] for field in result["fields"].values()), 8)
        self.assertEqual(result["distance"], 0)

    def test_independent_axes_reorder_both_matrices(self):
        for rows, cols in (([1, 0], [0, 1]), ([0, 1], [1, 0]), ([1, 0], [1, 0])):
            with self.subTest(rows=rows, cols=cols):
                ref = payload()
                cand = payload()
                cand["source_cluster"] = cand["source_cluster"][rows]
                cand["target_cluster"] = cand["target_cluster"][cols]
                for field in ("weighted", "unweighted"):
                    cand[field] = cand[field][np.ix_(rows, cols)]
                self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_direction_weighting_and_binding_faults(self):
        for fault in ("transpose", "weights", "one_field_binding", "axis_only", "row_sums", "symmetrize"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "transpose":
                    for field in ("weighted", "unweighted"):
                        cand[field] = cand[field].T
                elif fault == "weights":
                    cand["weighted"] = cand["unweighted"].copy()
                elif fault == "one_field_binding":
                    cand["source_cluster"] = cand["source_cluster"][::-1]
                    cand["weighted"] = cand["weighted"][::-1]
                elif fault == "axis_only":
                    cand["target_cluster"] = cand["target_cluster"][::-1]
                elif fault == "row_sums":
                    cand["weighted"] = cand["weighted"].sum(axis=1)
                else:
                    cand["weighted"] = (cand["weighted"] + cand["weighted"].T) / 2
                self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_fraction_must_be_checked_before_float64_narrowing(self):
        if np.finfo(np.longdouble).nmant <= np.finfo(np.float64).nmant:
            self.skipTest("当前平台 longdouble 没有额外精度；整数范围回归仍单独执行")
        fractional = np.longdouble(5) + np.ldexp(np.longdouble(1), -60)
        self.assertGreater(fractional, np.longdouble(5))
        self.assertEqual(float(fractional), 5.0)
        for side in ("reference", "candidate"):
            with self.subTest(side=side):
                ref, cand = payload(), payload()
                bad = ref if side == "reference" else cand
                bad["weighted"] = bad["weighted"].astype(np.longdouble)
                bad["weighted"][0, 0] = fractional
                self.assertFalse(self.run_pair(ref, cand)["passed"], "不能在缩窄后把非法小数当整数")

    def test_integer_range_must_be_checked_before_float64_narrowing(self):
        for dtype in (np.uint64, np.int64, np.longdouble):
            for side in ("reference", "candidate"):
                with self.subTest(dtype=str(dtype), side=side):
                    ref, cand = payload(), payload()
                    for data in (ref, cand):
                        data["weighted"] = data["weighted"].astype(dtype)
                        data["weighted"][0, 0] = 2**53
                    bad = ref if side == "reference" else cand
                    bad["weighted"][0, 0] = dtype(2**53 + 1)
                    if bad["weighted"][0, 0] == dtype(2**53):
                        continue
                    self.assertFalse(self.run_pair(ref, cand)["passed"], "上界之外的整数不能先round到上界")

    def test_legal_integer_encodings_remain_accepted(self):
        for dtype in (np.uint8, np.int16, np.uint64, np.int64, np.float32, np.float64, np.longdouble):
            with self.subTest(dtype=str(dtype)):
                ref, cand = payload(), payload()
                for data in (ref, cand):
                    for field in ("weighted", "unweighted"):
                        data[field] = data[field].astype(dtype)
                self.assertTrue(self.run_pair(ref, cand)["passed"])
        # 精度边界上的**合法**编码（2**53）不得被当成越界硬拒。判分器现在带第三条腿，
        # 2**53 当然不是这份 ic/ 的真实计数，所以整体会判不一致——但那必须来自
        # 第三条腿，而不是解析或取值范围报错：逐项比较仍须 0 超界。
        for dtype in (np.uint64, np.int64, np.longdouble):
            with self.subTest(boundary_dtype=str(dtype)):
                ref, cand = payload(), payload()
                for data in (ref, cand):
                    data["weighted"] = data["weighted"].astype(dtype)
                    data["weighted"][0, 0] = 2**53
                result = self.run_pair(ref, cand)
                self.assertEqual(
                    sum(f["values_over_bound"] for f in result["fields"].values()), 0)
                self.assertEqual(result["measurements"]["third_leg_failures"],
                                 ["reference", "candidate"])

    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐项比较结构上拒不掉，只有独立重算能拒。"""
        ref, cand = payload(), payload()
        for data in (ref, cand):
            data["weighted"][0, 0] += 1
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_third_leg_covers_every_graded_count(self):
        result = self.run_pair(payload(), payload())
        m = result["measurements"]
        self.assertTrue(result["passed"])
        self.assertEqual(m["items_with_a_third_leg"], m["graded_items"])
        self.assertIs(m["third_leg_is_partial"], False)

    def test_invalid_numeric_payloads_on_either_side(self):
        for side in ("reference", "candidate"):
            for value in (np.nan, np.inf, -1, 5.25):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    bad = ref if side == "reference" else cand
                    bad["weighted"] = bad["weighted"].astype(float)
                    bad["weighted"][0, 0] = value
                    self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_schema_and_missing_file_rejections(self):
        for fault in ("duplicate_axis", "missing_axis", "extra", "empty", "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_axis":
                    cand["source_cluster"][1] = "a"
                elif fault == "missing_axis":
                    del cand["target_cluster"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "empty":
                    cand["weighted"] = np.array([])
                elif fault == "object":
                    cand["weighted"] = cand["weighted"].astype(object)
                else:
                    (self.candidate / "result.npz").unlink(missing_ok=True)
                    cand = None
                self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_all_standard_zip_compressions_remain_accepted(self):
        for method in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED, zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA):
            with self.subTest(compression=method):
                self.write_archive(method=method)
                self.assertTrue(self.run_pair(payload(), None)["passed"])

    def write_archive(self, method=zipfile.ZIP_STORED, fault=None):
        with zipfile.ZipFile(self.candidate / "result.npz", "w", compression=method) as archive:
            for name, value in payload().items():
                buffer = io.BytesIO()
                if fault == "huge_header" and name == "weighted":
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<f8", "fortran_order": False, "shape": (2**40,)}
                    )
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr("weighted.npy", buffer.getvalue())

    def test_corrupt_duplicate_and_huge_header_rejections(self):
        for fault in ("corrupt", "duplicate", "huge_header"):
            with self.subTest(fault=fault):
                if fault == "corrupt":
                    (self.candidate / "result.npz").write_bytes(b"not a zip")
                else:
                    self.write_archive(fault=fault)
                self.assertFalse(self.run_pair(payload(), None)["passed"])

    def test_invalid_tolerances_use_failed_result(self):
        for value in (-1, float("nan"), True, "NaN"):
            with self.subTest(value=value):
                contract = rubric()
                contract["comparison"]["fields"]["weighted"]["atol"] = value
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
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {"weighted": invalid}}
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
