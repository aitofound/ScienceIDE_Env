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
SPEC = importlib.util.spec_from_file_location("stitched_image_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)

SIZE = 600
FIELDS = ("original_labels", "stitched_labels", "stitched_labels_joined")


def fixture_labels():
    """合成标号场：一半格子是**紧邻的一对**碎片，另一半是**孤立**的单体。

    判分器的第三条腿靠「几何上不可能配对」把 label 判成强制单体，所以自检的合成
    初值必须同时含有这两类，否则要么全部可判（测不到盲点），要么全部不可判
    （测不到第三条腿）。

    - 一对碎片：同格内两块 15×7，中间留 2 px 缝。它们的竖直 bbox 边相距 2.0
      （≤ max_gap=3），行 extent 完全重合 → 判分器认定「可能配对」，不作判定。
    - 孤立单体：单块 15×15。同一列相邻格子的竖直边坐标虽然相同，但行 extent
      不相交 → 判定为强制单体。
    """
    original = np.zeros((SIZE, SIZE), dtype=np.int32)
    label = 0
    paired = []
    for index, row in enumerate(range(0, SIZE - 20, 40)):
        for col in range(0, SIZE - 20, 40):
            if index % 2 == 0:                      # 紧邻的一对
                label += 1
                left = label
                original[row:row + 15, col:col + 7] = left
                label += 1
                original[row:row + 15, col + 9:col + 16] = label
                paired.append((left, label, row, col))
            else:                                   # 孤立单体
                label += 1
                original[row:row + 15, col:col + 15] = label
    return original, paired


def payload():
    """与 `fixture_labels()` 一致的一份合格产物；由该初值直接推出，不抄任何参考。"""
    original, paired = fixture_labels()
    stitched = original.copy()
    joined = original.copy()
    for left, right, row, col in paired:
        stitched[original == right] = left          # 组号取组内最小标号
        joined[original == right] = left
        joined[row:row + 15, col + 7:col + 9] = left  # 闭合把 2 px 缝填上
    return {
        "y": np.arange(SIZE, dtype=np.int64),
        "x": np.arange(SIZE, dtype=np.int64),
        "original_labels": original,
        "stitched_labels": stitched,
        "stitched_labels_joined": joined,
    }


def rubric(inputs_root=None):
    comparison = {"fields": {field: {"atol": 0.0, "rtol": 0.0} for field in FIELDS}}
    if inputs_root is not None:
        comparison["inputs_root"] = str(inputs_root)
    return {"policy": "pointwise", "comparison": comparison}


class StitchedImageValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stitchedimage-selftest-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reference = self.root / "reference"
        self.candidate = self.root / "candidate"
        self.reference.mkdir()
        self.candidate.mkdir()
        # 第三条腿要读 ic/；真实 ic/ 是官方 707 细胞的 fixture，与合成产物无关，
        # 所以自检写一份与 `fixture_labels()` 一致的小 ic/ 并通过 inputs_root 指过去。
        self.inputs_root = self.root / "ic"
        (self.inputs_root / "nominal").mkdir(parents=True)
        np.savez_compressed(self.inputs_root / "nominal" / "input.npz",
                            labels=fixture_labels()[0])
        self.contract = self.root / "rubric.json"
        self.contract.write_text(json.dumps(rubric(self.inputs_root)))
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
            np.savez_compressed(self.reference / "result.npz", **ref)
        if cand is not None:
            np.savez_compressed(self.candidate / "result.npz", **cand)
        code, failure, stderr = self.call_main()
        self.assertIsNone(failure, f"结果协议意外抛出 {failure!r}; stderr={stderr!r}")
        self.assertEqual(code, 0)
        return self.read_report()

    def test_all_1080000_pixels_are_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        self.assertEqual(sum(field["values"] for field in result["fields"].values()), 3 * SIZE * SIZE)
        self.assertEqual(result["distance"], 0)

    def test_labels_are_graded_as_is_not_up_to_renumbering(self):
        """与 image-pipeline-* 的有意对照：这里编号是合同，一致重编号也必须拒绝。"""
        ref, cand = payload(), payload()
        for field in ("stitched_labels", "stitched_labels_joined"):
            renumbered = cand[field].copy()
            renumbered[renumbered > 0] += 1000  # 划分完全相同，只是整体换了名字
            cand[field] = renumbered
        cand["original_labels"] = cand["original_labels"] + (cand["original_labels"] > 0) * 1000
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertGreater(result["fields"]["stitched_labels"]["values_over_bound"], 0)

    def test_single_pixel_difference_is_rejected(self):
        for field in FIELDS:
            with self.subTest(field=field):
                ref, cand = payload(), payload()
                target = np.argwhere(cand[field] > 0)[0]
                cand[field][tuple(target)] = ref["original_labels"].max()
                if field == "stitched_labels":
                    cand["stitched_labels_joined"][tuple(target)] = ref["original_labels"].max()
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])

    def test_pixel_identity_not_row_order(self):
        for seed in (0, 13):
            with self.subTest(seed=seed):
                ref, cand = payload(), payload()
                rng = np.random.default_rng(seed)
                rows, cols = rng.permutation(SIZE), rng.permutation(SIZE)
                cand["y"] = cand["y"][rows]
                cand["x"] = cand["x"][cols]
                for field in FIELDS:
                    cand[field] = cand[field][np.ix_(rows, cols)]
                self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_background_set_must_match_the_original(self):
        for fault in ("filled_background", "erased_cell"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "filled_background":
                    cand["stitched_labels"][300, 300] = 1
                    cand["stitched_labels_joined"][300, 300] = 1
                else:
                    spot = tuple(np.argwhere(cand["stitched_labels"] > 0)[0])
                    cand["stitched_labels"][spot] = 0
                    cand["stitched_labels_joined"][spot] = 0
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})

    def test_join_may_only_fill_background(self):
        ref, cand = payload(), payload()
        spot = tuple(np.argwhere(cand["original_labels"] > 0)[0])
        cand["stitched_labels_joined"][spot] = cand["stitched_labels"][spot] + 2
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(result["fields"], {})

    def test_invented_label_is_rejected(self):
        for field in ("stitched_labels", "stitched_labels_joined"):
            with self.subTest(field=field):
                ref, cand = payload(), payload()
                spot = tuple(np.argwhere(cand[field] > 0)[0])
                cand[field][spot] = 999999
                if field == "stitched_labels":
                    cand["stitched_labels_joined"][spot] = 999999
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})

    def test_negative_and_float_labels_are_rejected(self):
        for side in ("reference", "candidate"):
            for fault in ("negative", "float"):
                with self.subTest(side=side, fault=fault):
                    ref, cand = payload(), payload()
                    bad = ref if side == "reference" else cand
                    if fault == "negative":
                        bad["stitched_labels"] = bad["stitched_labels"].astype(np.int32)
                        bad["stitched_labels"][0, 0] = -1
                    else:
                        bad["stitched_labels"] = bad["stitched_labels"].astype(np.float64)
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertEqual(result["fields"], {})

    def test_unsigned_label_dtype_is_accepted(self):
        ref, cand = payload(), payload()
        for values in (ref, cand):
            for field in FIELDS:
                values[field] = values[field].astype(np.uint32)
        self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_schema_and_identity_rejections(self):
        for fault in ("duplicate_pixel", "unknown_pixel", "string_axis", "missing_field",
                      "missing_axis", "extra", "wrong_shape", "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_pixel":
                    cand["y"][1] = 0
                elif fault == "unknown_pixel":
                    cand["y"][1] = 9999
                elif fault == "string_axis":
                    cand["x"] = np.array([str(v) for v in range(SIZE)])
                elif fault == "missing_field":
                    del cand["stitched_labels_joined"]
                elif fault == "missing_axis":
                    del cand["x"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "wrong_shape":
                    cand["stitched_labels"] = np.zeros((SIZE, SIZE - 1), dtype=np.int32)
                elif fault == "object":
                    cand["stitched_labels"] = cand["stitched_labels"].astype(object)
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
                if fault == "huge_header" and name == "stitched_labels":
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<i4", "fortran_order": False, "shape": (2**40,)}
                    )
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr("stitched_labels.npy", buffer.getvalue())

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
            archive.writestr("y.npy", np.zeros(VALIDATOR.MAX_BYTES, dtype=np.uint8).tobytes())
        self.assertFalse(self.run_pair(payload(), None)["passed"])

    def test_invalid_tolerances_use_failed_result(self):
        for value in (-1, float("nan"), True, "NaN"):
            with self.subTest(value=value):
                contract = rubric()
                contract["comparison"]["fields"]["stitched_labels"]["atol"] = value
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
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {"stitched_labels": invalid}}
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



    def test_two_sided_pollution_on_a_forced_singleton_is_caught_only_by_third_leg(self):
        """两边改成一样：逐项比较结构上拒不掉，只有独立推导能拒。

        改的是**孤立单体**的像素——判分器能几何地证明它不可能进任何拼接组，
        所以 `stitched` 在那里必须原样保留原标号。
        """
        original = fixture_labels()[0]
        spot = (40, 0)                                  # 孤立单体那一行的格子
        self.assertNotEqual(int(original[spot]), 1)
        ref, cand = payload(), payload()
        for values in (ref, cand):
            values["stitched_labels"][spot] = 1
            values["stitched_labels_joined"][spot] = 1
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_third_leg_is_blind_to_labels_it_cannot_prove_are_singletons(self):
        """如实钉住盲点：真正参与拼接的那些 label，第三条腿判不了。

        它们要走完整条 QC 打分链（`_tiling_qc.py` 756 行 + `_tiling_stitch.py` 919 行），
        判分器不重算那条链。所以两侧对**可配对** label 同样改号，第三条腿放行——
        这不是缺陷，是这条腿的边界，`third_leg_is_partial` 已经如实报出。
        单侧改号仍会被逐像素比较抓到（见 test_single_pixel_difference_is_rejected）。
        """
        ref, cand = payload(), payload()
        for values in (ref, cand):                       # (0,0) 属于紧邻一对的左块
            values["stitched_labels"][0, 0] = 2
            values["stitched_labels_joined"][0, 0] = 2
        result = self.run_pair(ref, cand)
        self.assertTrue(result["passed"])
        self.assertEqual(result["measurements"]["third_leg_failures"], [])

    def test_third_leg_coverage_is_reported_per_field_and_is_partial(self):
        result = self.run_pair(payload(), payload())
        m = result["measurements"]
        self.assertTrue(result["passed"])
        self.assertIs(m["third_leg_is_partial"], True)
        self.assertEqual(m["graded_items"], 3 * SIZE * SIZE)
        # 整张原标号场永远可判；另两项在这份合成初值上只覆盖背景与孤立单体。
        self.assertEqual(m["items_with_a_third_leg_by_field"]["original_labels"],
                         SIZE * SIZE)
        for field in ("stitched_labels", "stitched_labels_joined"):
            covered = m["items_with_a_third_leg_by_field"][field]
            self.assertGreater(covered, 0)
            self.assertLess(covered, SIZE * SIZE)
        self.assertEqual(m["third_leg"]["reference"]["max_abs_gap"], 0.0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
