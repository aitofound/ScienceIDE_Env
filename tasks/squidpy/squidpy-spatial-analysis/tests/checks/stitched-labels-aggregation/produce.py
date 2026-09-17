#!/usr/bin/env python3
"""折叠拼接组：一组一行的聚合表，merge_strategy="sum"。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import dask.array as da
import numpy as np
import squidpy
import xarray as xr
from spatialdata import SpatialData
from spatialdata.models import Labels2DModel

TILE_SIZE = 200
NMADS_CUT = 1.0
NMADS_SMOOTHED = 1.5
MIN_CONFIDENCE = 0.5
MERGE_STRATEGY = "sum"
SCORE_NAMES = ("cut_score", "smoothed_cut_score", "max_straight_edge_ratio",
               "cardinal_alignment_score", "nhood_outlier_fraction")
CENTROID_NAMES = ("centroid_y", "centroid_x")


def main() -> None:
    parser = argparse.ArgumentParser()
    for flag in ("input", "out", "source"):
        parser.add_argument("--" + flag, required=True)
    args = parser.parse_args()
    source = Path(args.source).resolve()
    if not Path(squidpy.__file__).resolve().is_relative_to(source / "src"):
        raise RuntimeError("导入的Squidpy不属于SOURCE_DIR")
    with np.load(args.input, allow_pickle=False) as data:
        if set(data.files) != {"labels"}:
            raise ValueError("初值只包含 int32 标号图")
        labels = data["labels"].copy()
    if labels.shape != (600, 600) or labels.dtype != np.dtype(np.int32):
        raise ValueError("官方标号图必须是 600×600 的 int32")
    if int(labels.max()) != 707 or int(labels.min()) != 0:
        raise ValueError("官方 tile-boundary fixture 必须给出 707 个细胞与背景 0")
    sdata = SpatialData(labels={"labels": Labels2DModel.parse(
        xr.DataArray(da.from_array(labels, chunks=(200, 200)), dims=["y", "x"]))})
    squidpy.experimental.tl.calculate_tiling_qc(
        sdata, labels_key="labels", tile_size=TILE_SIZE,
        nmads_cut=NMADS_CUT, nmads_smoothed=NMADS_SMOOTHED)
    squidpy.experimental.tl.assign_stitch_groups(
        sdata, labels_key="labels", min_confidence=MIN_CONFIDENCE)
    # 逐字取自官方 test_merge_strategy_sum_aggregates_numeric_columns
    # （tests/experimental/test_stitched_labels.py:111-112）：一列常数 100.0 的用户特征，
    # 它是这份 fixture 里唯一会真正走 merge_strategy 的列，否则 "sum" 无从检出。
    qc = sdata.tables["labels_qc"]
    qc.obs["fake_area"] = 100.0
    sdata.tables["labels_qc"] = qc
    squidpy.experimental.im.make_stitched_labels(
        sdata, labels_key="labels", write_table=True, merge_strategy=MERGE_STRATEGY)
    agg = sdata.tables["labels_stitched_table"]
    obs = agg.obs

    ids = np.asarray(obs["label_id"], dtype=np.int64)
    order = np.argsort(ids)
    take = lambda name, dtype: np.asarray(obs[name], dtype=dtype)[order]  # noqa: E731
    payload = {
        "label_id": ids[order],
        "score_name": np.asarray(SCORE_NAMES),
        "centroid_name": np.asarray(CENTROID_NAMES),
        "group_id": take("stitch_group_id", np.int64),
        "n_pieces": take("n_pieces", np.int64),
        "is_stitched": take("is_stitched", np.float64),
        "stitch_confidence": take("stitch_confidence", np.float64),
        "centroids": np.column_stack([take(name, np.float64) for name in CENTROID_NAMES]),
        "qc_scores": np.column_stack([take(name, np.float64) for name in SCORE_NAMES]),
        "is_outlier": take("is_outlier", np.float64),
        "fake_area": take("fake_area", np.float64),
    }
    if payload["label_id"].size != int(agg.n_obs):
        raise ValueError("聚合表行数与 label_id 数不符")

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if any(p.name not in {'run.ok', 'run.failed', 'run.skipped', 'run.log'} for p in out.iterdir()):
        raise ValueError("OUT_DIR必须为空")
    np.savez(out / "result.npz", **payload)
    stitched = payload["is_stitched"] > 0
    evidence = {
        "tile_size": TILE_SIZE, "min_confidence": MIN_CONFIDENCE, "merge_strategy": MERGE_STRATEGY,
        "cells_in": int(labels.max()),
        "groups_out": int(payload["label_id"].size),
        "stitched_groups": int(stitched.sum()),
        "pieces_conserved": int(payload["n_pieces"].sum()) == int(labels.max()),
        "max_group_size": int(payload["n_pieces"].max()),
        "label_id_equals_group_id": bool(np.array_equal(payload["label_id"], payload["group_id"])),
        "undefined_confidence": int(np.isnan(payload["stitch_confidence"]).sum()),
        "undefined_qc_scores": int(np.isnan(payload["qc_scores"]).sum()),
        "table_region": str(agg.uns["spatialdata_attrs"]["region"]),
        "table_instance_key": str(agg.uns["spatialdata_attrs"]["instance_key"]),
    }
    print("SAB_PATH_EVIDENCE=" + json.dumps(evidence, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"stitched aggregation producer失败 ({type(exc).__name__}): {exc}", file=sys.stderr)
        raise SystemExit(1)
