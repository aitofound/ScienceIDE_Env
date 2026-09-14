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
SPEC = importlib.util.spec_from_file_location("sda_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


_TRUTH = VALIDATOR.recompute(HERE / "ic" / "nominal")


def payload():
    """基线取判分器对 `ic/nominal` 的**独立重算**，不再是编造的 arange 网格。

    判分器现在带第三条腿，会把物理上不对的 SDA 拒掉；用合成值做基线，测的只是
    比较逻辑自己跟自己。重算只用 numpy 逐元素运算，不 import squidpy 也不用 xarray。
    """
    import numpy as _np
    out = {axis: _np.asarray(VALIDATOR.AXES[axis]) for axis in VALIDATOR.AXES}
    for field in VALIDATOR.FIELDS:
        out[field] = _np.array(_TRUTH[field], copy=True)
    return out


def rubric():
    return {
        "policy": "pointwise",
        "comparison": {"fields": {"sda": {"atol": 1e-8, "rtol": 0}, "recovered_rgb": {"atol": 1e-8, "rtol": 0}}},
    }


class SdaValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="sda-selftest-")
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

    def test_all_1536_scientific_values_are_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        self.assertEqual(sum(field["values"] for field in result["fields"].values()), 1536)
        self.assertEqual(result["distance"], 0)

    def test_full_payload_identity_reordering(self):
        ref, cand = payload(), payload()
        channel, y, x = [2, 0, 1], np.arange(16)[::-1], np.roll(np.arange(16), 3)
        for name, order in (("channel", channel), ("y", y), ("x", x)):
            cand[name] = cand[name][order]
        for field in ("sda", "recovered_rgb"):
            cand[field] = cand[field][np.ix_(channel, y, x)]
        self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_scientific_stage_faults_not_only_roundtrip_consistency(self):
        for fault in (
            "sda_changed_rgb_unchanged",
            "inverse_changed",
            "integer_cast",
            "one_field_binding",
            "missing_stage",
        ):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "sda_changed_rgb_unchanged":
                    cand["sda"] *= 2
                elif fault == "inverse_changed":
                    cand["recovered_rgb"] += 1
                elif fault == "integer_cast":
                    cand["recovered_rgb"] = cand["recovered_rgb"].astype(np.uint8)
                elif fault == "one_field_binding":
                    cand["channel"] = cand["channel"][::-1]
                    cand["sda"] = cand["sda"][::-1]
                else:
                    del cand["sda"]
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                if fault == "sda_changed_rgb_unchanged":
                    self.assertEqual(result["fields"]["recovered_rgb"]["max_abs_error"], 0)
                    self.assertGreater(result["fields"]["sda"]["values_over_bound"], 0)

    def test_nonfinite_reference_or_candidate_is_rejected(self):
        for side in ("reference", "candidate"):
            for value in (np.nan, np.inf, -np.inf):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    bad = ref if side == "reference" else cand
                    bad["sda"][0, 0, 0] = value
                    self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_schema_and_missing_file_rejections(self):
        for fault in ("duplicate_axis", "missing_axis", "extra", "empty", "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_axis":
                    cand["channel"][1] = "R"
                elif fault == "missing_axis":
                    del cand["x"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "empty":
                    cand["sda"] = np.array([])
                elif fault == "object":
                    cand["sda"] = cand["sda"].astype(object)
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
                if fault == "huge_header" and name == "sda":
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<f8", "fortran_order": False, "shape": (2**40,)}
                    )
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr("sda.npy", buffer.getvalue())

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
                contract["comparison"]["fields"]["sda"]["atol"] = value
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
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {"sda": invalid}}
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
            values["sda"][0, 0, 0] = values["sda"][0, 0, 0] + 0.5
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_third_leg_covers_every_graded_value_and_is_exact(self):
        result = self.run_pair(payload(), payload())
        m = result["measurements"]
        self.assertTrue(result["passed"])
        self.assertEqual(m["items_with_a_third_leg"], m["graded_items"])
        self.assertIs(m["third_leg_is_partial"], False)
        # 这一族五条 check 的独立重算与真实产物**逐位相同**，钉住这一点。
        self.assertEqual(m["third_leg"]["reference"]["max_abs_gap"], 0.0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
