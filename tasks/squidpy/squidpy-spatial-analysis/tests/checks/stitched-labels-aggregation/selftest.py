#!/usr/bin/env python3
"""独立人工 validator 回归；仅依赖标准库与 NumPy，不读取生产初值或源码。"""

from __future__ import annotations

import contextlib
import functools
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
SPEC = importlib.util.spec_from_file_location("stitched_agg_validator_under_test", HERE / "validate.py")
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)

CELLS_IN = 707
SCORE_NAMES = ("cut_score", "smoothed_cut_score", "max_straight_edge_ratio",
               "cardinal_alignment_score", "nhood_outlier_fraction")
CENTROID_NAMES = ("centroid_y", "centroid_x")
FIELDS = ("group_id", "n_pieces", "is_stitched", "stitch_confidence",
          "centroids", "qc_scores", "is_outlier", "fake_area")
PAIRS = 25          # 合成 25 个两碎片组，其余单碎片
UNDEFINED_ROWS = (3, 17, 200)
UNDEFINED_SCORE_COLS = (0, 2, 3)


@functools.lru_cache(maxsize=1)
def _fixture():
    """合成 707 细胞标号场：25 个**紧邻的一对**（可配对），657 个**孤立**（强制单体）。

    判分器的第三条腿靠「几何上不可能配对」识别强制单体，所以合成初值必须同时含
    两类，否则要么全部可判、要么全部不可判，都测不到东西。

    22 px 网格、8×8 的块：相邻块间隔 14 px > max_gap=3，互不成对；一对碎片则是同格
    内两块 8×3、中间留 2 px 缝，竖直 bbox 边相距 2.0 ≤ 3 且行 extent 重合 → 可配对。
    """
    labels = np.zeros((600, 600), dtype=np.int32)
    groups, label, made = [], 0, 0
    for row in range(0, 594, 22):
        for col in range(0, 594, 22):
            if made >= PAIRS + (CELLS_IN - 2 * PAIRS):
                break
            if made < PAIRS:                                  # 紧邻的一对
                labels[row:row + 8, col:col + 3] = label + 1
                labels[row:row + 8, col + 5:col + 8] = label + 2
                groups.append((label + 1, [label + 1, label + 2]))
                label += 2
            else:                                             # 孤立单体
                labels[row:row + 8, col:col + 8] = label + 1
                groups.append((label + 1, [label + 1]))
                label += 1
            made += 1
    if label != CELLS_IN:
        raise AssertionError(f"合成初值应恰有 {CELLS_IN} 个细胞，实得 {label}")
    centre = {}
    flat = labels.ravel()
    keep = flat > 0
    ids = flat[keep]
    rows, cols = np.divmod(np.flatnonzero(keep), labels.shape[1])
    order = np.argsort(ids, kind="stable")
    ids, rows, cols = ids[order], rows[order], cols[order]
    unique, start = np.unique(ids, return_index=True)
    for lid, lo, hi in zip(unique.tolist(), start.tolist(), np.append(start[1:], ids.size).tolist()):
        centre[lid] = (float(rows[lo:hi].mean()), float(cols[lo:hi].mean()))
    return labels, groups, centre


def fixture_labels():
    """返回合成标号场与组划分；整图较大，算一次缓存复用。"""
    labels, groups, _ = _fixture()
    return labels, groups


def payload():
    """一份合格的合成聚合表，**由合成初值直接推出**，不抄任何参考。

    可独立推导的五项（group_id / n_pieces / is_stitched / 质心 / fake_area）按源码
    规则算；判分器判不了的三项（stitch_confidence / qc_scores / is_outlier）仍用
    合成值——它们的合法性由量程与 NaN 三态检查把关。
    """
    labels, groups, centre = _fixture()
    ids = np.array([gid for gid, _ in groups], dtype=np.int64)
    order = np.argsort(ids)
    ids = ids[order]
    members = [groups[i][1] for i in order]
    pieces = np.array([len(m) for m in members], dtype=np.int64)
    rows = ids.size
    centroids = np.array([[np.mean([centre[m][axis] for m in group]) for axis in (0, 1)]
                          for group in members], dtype=np.float64)
    scores = (np.arange(rows * len(SCORE_NAMES), dtype=np.float64).reshape(rows, len(SCORE_NAMES))
              / (rows * len(SCORE_NAMES)) * 3.0)
    for row in UNDEFINED_ROWS:
        for col in UNDEFINED_SCORE_COLS:
            scores[row, col] = np.nan
    confidence = np.full(rows, np.nan)
    confidence[pieces > 1] = np.linspace(0.7, 1.0, int((pieces > 1).sum()))
    confidence[PAIRS : PAIRS + 40] = 1.0
    flag = np.zeros(rows, dtype=np.float64)
    flag[::7] = 1.0
    return {
        "label_id": ids,
        "score_name": np.array(SCORE_NAMES),
        "centroid_name": np.array(CENTROID_NAMES),
        "group_id": ids.copy(),
        "n_pieces": pieces,
        "is_stitched": (pieces > 1).astype(np.float64),
        # NaN = 「该组没进过拼接候选」，是合法的第三态。
        "stitch_confidence": confidence,
        "centroids": centroids,
        "qc_scores": scores,
        "is_outlier": flag,
        "fake_area": pieces.astype(np.float64) * 100.0,
    }


def rubric(inputs_root=None):
    tight = {"atol": 0.0, "rtol": 0.0}
    loose = {"atol": 1e-9, "rtol": 1e-12}
    return {
        "policy": "pointwise",
        "comparison": {
            "fields": {
                "group_id": dict(tight), "n_pieces": dict(tight),
                "is_stitched": dict(tight), "is_outlier": dict(tight),
                "stitch_confidence": dict(loose), "centroids": dict(loose),
                "qc_scores": dict(loose), "fake_area": dict(loose),
            },
            **({"inputs_root": str(inputs_root)} if inputs_root is not None else {}),
        },
    }


class StitchedAggregationValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stitchedagg-selftest-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reference = self.root / "reference"
        self.candidate = self.root / "candidate"
        self.reference.mkdir()
        self.candidate.mkdir()
        self.contract = self.root / "rubric.json"
        # 第三条腿要读 ic/；真实 ic/ 是官方 707 细胞的 fixture，与合成表无关，
        # 所以自检写一份与 `fixture_labels()` 一致的小 ic/ 并通过 inputs_root 指过去。
        self.inputs_root = self.root / "ic"
        (self.inputs_root / "nominal").mkdir(parents=True)
        np.savez_compressed(self.inputs_root / "nominal" / "input.npz",
                            labels=fixture_labels()[0])
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
            np.savez(self.reference / "result.npz", **ref)
        if cand is not None:
            np.savez(self.candidate / "result.npz", **cand)
        code, failure, stderr = self.call_main()
        self.assertIsNone(failure, f"结果协议意外抛出 {failure!r}; stderr={stderr!r}")
        self.assertEqual(code, 0)
        return self.read_report()

    def test_every_aggregated_value_is_compared(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        rows = CELLS_IN - PAIRS
        self.assertEqual(sum(field["values"] for field in result["fields"].values()),
                         rows * (6 + len(SCORE_NAMES) + len(CENTROID_NAMES)))
        self.assertEqual(result["distance"], 0)

    def test_group_identity_not_row_order(self):
        for seed in (0, 13):
            with self.subTest(seed=seed):
                ref, cand = payload(), payload()
                rng = np.random.default_rng(seed)
                order = rng.permutation(cand["label_id"].size)
                for name in ("label_id", *FIELDS):
                    cand[name] = cand[name][order]
                self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_wrong_group_set_is_a_scientific_failure_not_a_schema_failure(self):
        """拼错了组：行数守恒仍成立，但组标号集合与参考不同。"""
        ref, cand = payload(), payload()
        cand["label_id"] = cand["label_id"].copy()
        cand["group_id"] = cand["group_id"].copy()
        # 挑一个合成初值里**没有**用作组标号的正整数（配对组里较大的那半就是）；
        # 不能再写死 CELLS_IN——它在新 fixture 里本来就是一个组标号，写死会变成重复
        # 标号，被 schema 检查先拒掉，这条用例就测不到「科学失败」了。
        absent = min(set(range(1, CELLS_IN + 1)) - set(ref["label_id"].tolist()))
        cand["label_id"][5] = absent
        cand["group_id"][5] = absent
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertIn("\\u62fc\\u63a5\\u7ec4", json.dumps(result["reason"]))  # 「拼接组」
        self.assertEqual(result["fields"], {})

    def test_undefined_confidence_is_legal_and_excluded_from_the_distance(self):
        result = self.run_pair(payload(), payload())
        self.assertTrue(result["passed"])
        undefined = CELLS_IN - PAIRS - PAIRS - 40
        self.assertEqual(result["fields"]["stitch_confidence"]["undefined_positions"], undefined)
        self.assertEqual(result["fields"]["qc_scores"]["undefined_positions"],
                         len(UNDEFINED_ROWS) * len(UNDEFINED_SCORE_COLS))

    def test_moved_undefined_position_is_rejected(self):
        for side in ("reference", "candidate"):
            for field, fault in (("stitch_confidence", "extra_nan"), ("stitch_confidence", "fewer_nan"),
                                 ("qc_scores", "shifted_nan")):
                with self.subTest(side=side, field=field, fault=fault):
                    ref, cand = payload(), payload()
                    bad = ref if side == "reference" else cand
                    if fault == "extra_nan":
                        bad[field][PAIRS + 1] = np.nan
                    elif fault == "fewer_nan":
                        bad[field][-1] = 0.9
                    else:
                        bad[field][UNDEFINED_ROWS[0], UNDEFINED_SCORE_COLS[0]] = 1.0
                        bad[field][UNDEFINED_ROWS[0] + 1, UNDEFINED_SCORE_COLS[0]] = np.nan
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertIn("NaN", result["reason"])
                    self.assertIsNone(result["fields"][field]["max_abs_error"])

    def test_derived_columns_are_graded_not_schema_checked(self):
        """is_stitched / group_id 与其它列可互相推导，但错了要报成科学失败。"""
        for field in ("is_stitched", "group_id"):
            with self.subTest(field=field):
                ref, cand = payload(), payload()
                cand[field] = cand[field].copy()
                cand[field][0] = 0 if field == "is_stitched" else CELLS_IN
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"][field]["values_over_bound"], 1)

    def test_conservation_and_range_rejections(self):
        for side in ("reference", "candidate"):
            for fault in ("pieces_sum", "oversized_group", "confidence_over_one",
                          "centroid_outside", "negative_score", "zero_pieces", "float_pieces"):
                with self.subTest(side=side, fault=fault):
                    ref, cand = payload(), payload()
                    bad = ref if side == "reference" else cand
                    if fault == "pieces_sum":
                        bad["n_pieces"] = bad["n_pieces"].copy()
                        bad["n_pieces"][-1] = 2
                    elif fault == "oversized_group":
                        bad["n_pieces"] = bad["n_pieces"].copy()
                        bad["n_pieces"][0] = 5
                        bad["n_pieces"][1] = 1
                        bad["n_pieces"][2] = 1
                        bad["n_pieces"][3] = 1
                    elif fault == "confidence_over_one":
                        bad["stitch_confidence"][0] = 1.5
                    elif fault == "centroid_outside":
                        bad["centroids"][0, 0] = 601.0
                    elif fault == "negative_score":
                        bad["qc_scores"][1, 1] = -1.0
                    elif fault == "zero_pieces":
                        bad["n_pieces"] = bad["n_pieces"].copy()
                        bad["n_pieces"][-1] = 0
                        bad["n_pieces"][0] = 3
                    else:
                        bad["n_pieces"] = bad["n_pieces"].astype(np.float64)
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertEqual(result["fields"], {})

    def test_infinity_is_still_rejected(self):
        for side in ("reference", "candidate"):
            for value in (np.inf, -np.inf):
                with self.subTest(side=side, value=value):
                    ref, cand = payload(), payload()
                    (ref if side == "reference" else cand)["qc_scores"][10, 1] = value
                    result = self.run_pair(ref, cand)
                    self.assertFalse(result["passed"])
                    self.assertEqual(result["fields"], {})

    def test_flags_must_be_zero_or_one_and_are_exact(self):
        ref, cand = payload(), payload()
        cand["is_outlier"][7] = 1.0 - cand["is_outlier"][7]
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(result["fields"]["is_outlier"]["values_over_bound"], 1)
        for side in ("reference", "candidate"):
            with self.subTest(side=side):
                ref, cand = payload(), payload()
                (ref if side == "reference" else cand)["is_outlier"][3] = 0.5
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})

    def test_boolean_flag_dtype_is_accepted(self):
        ref, cand = payload(), payload()
        for values in (ref, cand):
            for field in ("is_stitched", "is_outlier"):
                values[field] = values[field].astype(bool)
        self.assertTrue(self.run_pair(ref, cand)["passed"])

    def test_absolute_bounds_are_two_sided(self):
        ref, cand = payload(), payload()
        cand["centroids"][2, 1] += 5e-10
        self.assertTrue(self.run_pair(ref, cand)["passed"])
        cand["centroids"][2, 1] += 1e-6
        self.assertFalse(self.run_pair(ref, cand)["passed"])

    def test_score_and_centroid_column_names_are_pinned(self):
        for axis, replacement in (("score_name", ("a", "b", "c", "d", "e")),
                                  ("centroid_name", ("centroid_x", "centroid_y"))):
            with self.subTest(axis=axis):
                ref, cand = payload(), payload()
                cand[axis] = np.array(replacement)
                result = self.run_pair(ref, cand)
                self.assertFalse(result["passed"])
                self.assertEqual(result["fields"], {})

    def test_schema_and_identity_rejections(self):
        for fault in ("duplicate_group", "group_zero", "group_over_input", "string_group_axis",
                      "missing_field", "missing_axis", "extra", "wrong_rows", "wrong_columns",
                      "object", "missing_file"):
            with self.subTest(fault=fault):
                ref, cand = payload(), payload()
                if fault == "duplicate_group":
                    cand["label_id"][1] = cand["label_id"][0]
                elif fault == "group_zero":
                    cand["label_id"][1] = 0
                elif fault == "group_over_input":
                    cand["label_id"][1] = CELLS_IN + 1
                elif fault == "string_group_axis":
                    cand["label_id"] = np.array([str(v) for v in cand["label_id"]])
                elif fault == "missing_field":
                    del cand["fake_area"]
                elif fault == "missing_axis":
                    del cand["score_name"]
                elif fault == "extra":
                    cand["extra"] = np.ones(1)
                elif fault == "wrong_rows":
                    cand["is_outlier"] = np.zeros(cand["label_id"].size - 1)
                elif fault == "wrong_columns":
                    cand["qc_scores"] = cand["qc_scores"][:, :4]
                elif fault == "object":
                    cand["qc_scores"] = cand["qc_scores"].astype(object)
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
                if fault == "huge_header" and name == "qc_scores":
                    np.lib.format.write_array_header_1_0(
                        buffer, {"descr": "<f8", "fortran_order": False, "shape": (2**40,)}
                    )
                else:
                    np.save(buffer, value, allow_pickle=False)
                archive.writestr(name + ".npy", buffer.getvalue())
            if fault == "duplicate":
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    archive.writestr("qc_scores.npy", buffer.getvalue())

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
                contract["comparison"]["fields"]["qc_scores"]["atol"] = value
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
                partial = {"passed": True, "reason": "partial", "distance": 0, "fields": {"qc_scores": invalid}}
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
        """两边改成一样：逐值比较结构上拒不掉，只有独立推导能拒。

        改的是**孤立**细胞那一行——判分器能几何地证明它不可能进任何拼接组，
        于是 `fake_area` 必须恰为一个成员求和 = 100.0。
        """
        ref, cand = payload(), payload()
        row = int(np.flatnonzero(ref["n_pieces"] == 1)[0])
        for values in (ref, cand):
            values["fake_area"] = values["fake_area"].copy()
            values["fake_area"][row] = 200.0
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(
            sum(f["values_over_bound"] for f in result["fields"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])

    def test_two_sided_loss_of_a_forced_singleton_row_is_caught_by_the_third_leg(self):
        """组集合两侧同错：`label_id` 集合一致，比较放行，但强制单体必须各自成行。"""
        ref, cand = payload(), payload()
        absent = min(set(range(1, CELLS_IN + 1)) - set(ref["label_id"].tolist()))
        row = int(np.flatnonzero(ref["n_pieces"] == 1)[0])
        for values in (ref, cand):
            for key in ("label_id", "group_id"):
                values[key] = values[key].copy()
                values[key][row] = absent
        result = self.run_pair(ref, cand)
        self.assertFalse(result["passed"])
        self.assertEqual(result["measurements"]["third_leg_failures"],
                         ["reference", "candidate"])
        self.assertIn("独自成行", result["measurements"]["third_leg"]["reference"]["note"])

    def test_third_leg_is_blind_to_groups_it_cannot_prove_are_singletons(self):
        """如实钉住盲点：真正参与拼接的组，第三条腿判不了。

        判定它们要走完整条 QC 打分链（`_tiling_qc.py` 756 行 + `_tiling_stitch.py`
        919 行），判分器不重算那条链——抄一遍就既不独立也没有意义。
        `stitch_confidence`、`qc_scores`、`is_outlier` 三列同理，**任何**行都判不了。
        单侧改值仍由逐值比较抓到。
        """
        ref, cand = payload(), payload()
        row = int(np.flatnonzero(ref["n_pieces"] > 1)[0])
        for values in (ref, cand):
            values["centroids"] = values["centroids"].copy()
            values["centroids"][row, 0] += 5.0
        result = self.run_pair(ref, cand)
        self.assertTrue(result["passed"])
        self.assertEqual(result["measurements"]["third_leg_failures"], [])

    def test_third_leg_coverage_is_reported_and_partial(self):
        result = self.run_pair(payload(), payload())
        m = result["measurements"]
        self.assertTrue(result["passed"])
        self.assertIs(m["third_leg_is_partial"], True)
        self.assertEqual(m["rows_graded"], CELLS_IN - PAIRS)
        self.assertEqual(m["rows_with_a_third_leg"], CELLS_IN - 2 * PAIRS)
        self.assertEqual(m["fields_without_a_third_leg"],
                         ["stitch_confidence", "qc_scores", "is_outlier"])
        self.assertEqual(m["third_leg"]["reference"]["max_abs_gap"], 0.0)

if __name__ == "__main__":
    unittest.main(verbosity=2)
