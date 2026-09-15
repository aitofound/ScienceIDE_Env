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
SPEC = importlib.util.spec_from_file_location("cell_features_intensity_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)

INTENSITY_NAMES = (
    'intensity_max_0', 'intensity_max_1', 'intensity_max_2', 'intensity_mean_0', 'intensity_mean_1',
    'intensity_mean_2', 'intensity_min_0', 'intensity_min_1', 'intensity_min_2', 'intensity_std_0',
    'intensity_std_1', 'intensity_std_2',
)


_TRUTH = VALIDATOR.recompute(HERE / "ic" / "nominal")


def payload():
    """基线取判分器对 `ic/nominal` 的**独立重算**，不再是编造的数组。"""
    import numpy as _np
    out = {axis: _np.asarray(VALIDATOR.AXES[axis]) for axis in VALIDATOR.AXES}
    for field in VALIDATOR.FIELDS:
        out[field] = _np.array(_TRUTH[field], copy=True)
    return out

def rubric():
    return {
        "policy": "pointwise",
        "comparison": {
            "fields": {
                "intensity": {"atol": 1e-6, "rtol": 1e-6},
            }
        },
    }


class CellIntensityValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cellintensity-selftest-")
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

    def test_all_192_scientific_values_are_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        self.assertEqual(sum(field["values"] for field in result["fields"].values()), 192)
        self.assertEqual(result["distance"], 0)

    def test_cell_and_feature_identity_not_row_order(self):
        for seed in (0, 9, 42):
            with self.subTest(seed=seed):
                ref, cand = payload(), payload()
                rng = np.random.default_rng(seed)
                cells, feats = rng.permutation(16), rng.permutation(12)
                cand["label_id"] = cand["label_id"][cells]
                cand["intensity_feature"] = cand["intensity_feature"][feats]
                cand["intensity"] = cand["intensity"][np.ix_(cells, feats)]
                self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_cell_binding_and_feature_faults_are_scientific(self):
        for fault in ("one_cell", "rolled_cells", "feature_swap", "one_grey_level"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "one_cell":
                    cand["intensity"][3, 4] += 1e-3
                elif fault == "rolled_cells":
                    cand["intensity"] = np.roll(cand["intensity"], 1, axis=0)
                elif fault == "feature_swap":
                    cand["intensity"] = cand["intensity"][:, [1, 0] + list(range(2, 12))]
                else:
                    # 一个灰度级落在 900 像素细胞上的均值变化，约 1.1e-3
                    cand["intensity"][7, 5] += 1.0 / 900.0
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(set(result["fields"]), set(rubric()["comparison"]["fields"]))

    def test_absolute_bounds_are_two_sided(self):
        """界内通过、界外拒绝。扰动量按**该值自身的界**推导，不写死常数。

        原用例写的是 `+= 1e-4`，那是照旧的合成基线调的；换成真实重算基线后该格约为
        一百多，`rtol` 使界涨到 ~1.0e-4，同样的扰动**刚好不越界**，这条用例就失效了。
        """
        ref, cand = payload(), payload()
        atol, rtol = rubric()["comparison"]["fields"]["intensity"].values()
        bound = atol + rtol * abs(float(ref["intensity"][2, 2]))
        cand["intensity"][2, 2] += bound / 2.0
        self.assertTrue(self.run_pair(ref, cand)["passed"])
        cand["intensity"][2, 2] += 10.0 * bound
        self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_intensity_domain_is_enforced(self):
        for side in ("reference", "candidate"):
            for value in (np.nextafter(0.0, -np.inf), np.nextafter(255.0, np.inf)):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    (ref if side == "reference" else cand)["intensity"][0, 0] = value
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertEqual(result["fields"], {})

    def test_exact_domain_bounds_remain_accepted(self):
        """uint8 强度的定义域端点 0 与 255 是合法取值，不得被当越界硬拒。

        判分器现在带第三条腿，这些值不是这份 `ic/` 的真实特征，所以整体会判不一致
        ——但那必须来自第三条腿，逐项比较仍须 0 超界。
        """
        ref, cand = payload(), payload()
        for values in (ref, cand):
            values["intensity"][0, 0] = 0.0
            values["intensity"][0, 1] = 255.0
        result = self.run_pair(ref, cand)
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_nonfinite_values_are_rejected_on_both_sides(self):
        for side in ("reference", "candidate"):
            for value in (np.nan, np.inf, -np.inf):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    (ref if side == "reference" else cand)["intensity"][0, 0] = value
                    self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_schema_and_identity_rejections(self):
        for fault in ("duplicate_cell", "unknown_cell", "string_cell_axis", "numeric_feature_axis",
                      "missing_field", "missing_axis", "extra", "wrong_shape", "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_cell":
                    cand["label_id"][1] = 1
                elif fault == "unknown_cell":
                    cand["label_id"][1] = 99
                elif fault == "string_cell_axis":
                    cand["label_id"] = np.array([str(v) for v in range(1, 17)])
                elif fault == "numeric_feature_axis":
                    cand["intensity_feature"] = np.arange(12)
                elif fault == "missing_field":
                    del cand["intensity"]
                elif fault == "missing_axis":
                    del cand["label_id"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "wrong_shape":
                    cand["intensity"] = np.zeros((16, 11))
                elif fault == "object":
                    cand["intensity"] = cand["intensity"].astype(object)
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
                if fault == "huge_header" and name == "intensity":
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<f8", "fortran_order": False, "shape": (2**40,)}
                    )
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr("intensity.npy", buffer.getvalue())

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

    def test_invalid_tolerances_use_failed_result(self):
        for value in (-1, float("nan"), True, "NaN"):
            with self.subTest(value=value):
                contract = rubric()
                contract["comparison"]["fields"]["intensity"]["atol"] = value
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
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {"intensity": invalid}}
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
            values["intensity"][0, 0] += 1.0
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
        self.assertEqual(m["third_leg"]["reference"]["max_abs_gap"], 0.0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
