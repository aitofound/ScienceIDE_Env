#!/usr/bin/env python3
"""原官方白点零场仅调用forward，不根据类名虚构inverse阶段。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import squidpy
import xarray as xr
from squidpy.experimental.im._stain._conversion import rgb_to_sda


def main() -> None:
    parser = argparse.ArgumentParser()
    for flag in ("input", "out", "source"):
        parser.add_argument("--" + flag, required=True)
    args = parser.parse_args()
    source = Path(args.source).resolve()
    if not Path(squidpy.__file__).resolve().is_relative_to(source / "src"):
        raise RuntimeError("导入的Squidpy不属于SOURCE_DIR")
    with np.load(args.input, allow_pickle=False) as data:
        values, white_point = data["rgb"].copy(), data["white_point"].copy()
        channel, y, x = data["channel"].copy(), data["y"].copy(), data["x"].copy()
    if values.shape != (3, 4, 4) or values.dtype != np.dtype(np.float64) or not np.all(np.isfinite(values)):
        raise ValueError("官方白点RGB必须为有限3×4×4 float64场")
    if white_point.shape != (3,) or not np.all(np.isfinite(white_point)):
        raise ValueError("white point必须为三个有限数")
    rgb = xr.DataArray(values, dims=("c", "y", "x"), coords={"c": channel, "y": y, "x": x})
    forward = rgb_to_sda(rgb, white_point).transpose("c", "y", "x")
    sda = forward.values.copy()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if any(p.name not in {'run.ok', 'run.failed', 'run.skipped', 'run.log'} for p in out.iterdir()):
        raise ValueError("OUT_DIR必须为空")
    np.savez(
        out / "result.npz",
        channel=forward.coords["c"].values,
        y=forward.coords["y"].values,
        x=forward.coords["x"].values,
        sda=sda,
    )
    evidence = {
        "input_shape": list(values.shape),
        "stages_run": ["rgb_to_sda"],
        "zero_sda_values": int(np.count_nonzero(sda == 0)),
        "negative_sda_values": int(np.count_nonzero(sda < 0)),
        "positive_sda_values": int(np.count_nonzero(sda > 0)),
    }
    print("SAB_PATH_EVIDENCE=" + json.dumps(evidence, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"白点零场producer失败 ({type(exc).__name__}): {exc}", file=sys.stderr)
        raise SystemExit(1)
