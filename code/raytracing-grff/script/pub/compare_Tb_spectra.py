#!/usr/bin/env python
"""
Compare model Tb spectrum from ray-tracing maps with literature points.

Model extraction:
- Input maps: script/pub/mfs/raytrace_*.npz
- ROI in R_sun:
  x in [-0.3, 0.3], y in [0.2, 0.8]
- For each frequency map, Tb_model = average Tb in ROI.

Output:
- compare_Tb_spec.pdf
"""

from pathlib import Path
import argparse
import re

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

R_SUN_M = 6.957e8


def _parse_freq_mhz_from_name(path: Path):
    m = re.search(r"_(\d+\.\d+)MHz\.npz$", path.name)
    if m:
        return float(m.group(1))
    return None


def load_model_spectrum(mfs_dir: Path, x_range=(-0.4, 0.4), y_range=(-0.4, 0.4)):
    files = sorted(mfs_dir.glob("raytrace_*.npz"))
    if not files:
        raise FileNotFoundError(f"No raytrace_*.npz files found in {mfs_dir}")

    freq_mhz = []
    tb_model = []
    for f in files:
        d = np.load(f)
        tb = np.array(d["emission_cube"][:, :, 0], dtype=float)
        x = np.array(d["x_coords"], dtype=float) / R_SUN_M
        y = np.array(d["y_coords"], dtype=float) / R_SUN_M

        xx, yy = np.meshgrid(x, y)
        roi = (xx >= x_range[0]) & (xx <= x_range[1]) & (yy >= y_range[0]) & (yy <= y_range[1])
        vals = tb[roi]
        vals = vals[np.isfinite(vals)]
        if vals.size == 0:
            mean_tb = np.nan
        else:
            mean_tb = float(np.nanmean(vals))

        f_mhz = _parse_freq_mhz_from_name(f)
        if f_mhz is None:
            f_mhz = float(d["frequencies_Hz"][0]) / 1e6

        freq_mhz.append(f_mhz)
        tb_model.append(mean_tb)

    freq_mhz = np.array(freq_mhz, dtype=float)
    tb_model = np.array(tb_model, dtype=float)
    order = np.argsort(freq_mhz)
    return freq_mhz[order], tb_model[order]


def plot_points_like_notebook(df: pd.DataFrame):
    for src, g in df.groupby("source", sort=False):
        g = g.copy()
        dup_freqs = g["freq_MHz"].duplicated(keep=False)
        g_dup = g[dup_freqs].copy()
        g_nondup = g[~dup_freqs].copy()

        if not g_dup.empty:
            for f, gg in g_dup.groupby("freq_MHz"):
                vals = gg["Tb_K"].dropna().to_numpy()
                if len(vals) >= 2:
                    y0, y1 = np.min(vals), np.max(vals)
                    plt.vlines(f, y0, y1, linewidth=1.8)
                    plt.plot([f], [(y0 + y1) / 2], marker="o", linestyle="none", label=src)
                else:
                    plt.plot(gg["freq_MHz"], gg["Tb_K"], marker="o", linestyle="none", label=src)
            already_labeled = True
        else:
            already_labeled = False

        if not g_nondup.empty:
            x = g_nondup["freq_MHz"].to_numpy()
            y = g_nondup["Tb_K"].to_numpy()
            yerr = g_nondup["Tb_err_K"].to_numpy()
            has_err = np.isfinite(yerr).any()
            if has_err:
                plt.errorbar(
                    x,
                    y,
                    yerr=np.where(np.isfinite(yerr), yerr, 0.0),
                    fmt="o",
                    linestyle="none",
                    capsize=2.5,
                    label=(None if already_labeled else src),
                )
            else:
                plt.plot(
                    x,
                    y,
                    marker="o",
                    linestyle="none",
                    label=(None if already_labeled else src),
                )


def main():
    parser = argparse.ArgumentParser(description="Compare Tb model spectrum with literature points.")
    parser.add_argument("--mfs-dir", default="script/pub/mfs", help="Directory with raytrace_*.npz maps")
    parser.add_argument("--points-csv", default="script/pub/TbSpectra.csv", help="Points CSV from notebook")
    parser.add_argument("--out", default="script/pub/compare_Tb_spec.pdf", help="Output PDF")
    parser.add_argument("--x-min", type=float, default=-0.3, help="ROI x min (R_sun)")
    parser.add_argument("--x-max", type=float, default=0.3, help="ROI x max (R_sun)")
    parser.add_argument("--y-min", type=float, default=0.2, help="ROI y min (R_sun)")
    parser.add_argument("--y-max", type=float, default=0.8, help="ROI y max (R_sun)")
    args = parser.parse_args()

    mfs_dir = Path(args.mfs_dir)
    points_csv = Path(args.points_csv)
    out_pdf = Path(args.out)
    out_pdf.parent.mkdir(parents=True, exist_ok=True)

    freq_mhz, tb_model = load_model_spectrum(
        mfs_dir=mfs_dir,
        x_range=(args.x_min, args.x_max),
        y_range=(args.y_min, args.y_max),
    )
    df_points = pd.read_csv(points_csv)

    plt.figure(figsize=(6, 3))
    valid = np.isfinite(tb_model) & (tb_model > 0)
    plt.plot(
        freq_mhz[valid],
        tb_model[valid],
        "-k",
        linewidth=2.0,
        label="ray-tracing MAS GRFF",
    )



    plot_points_like_notebook(df_points)


    plt.xscale("log")
    plt.yscale("log")
    plt.xlabel("Frequency (MHz)")
    plt.ylabel(r"$T_B$ (K)")
    #plt.grid(True, which="both", linewidth=0.5, alpha=0.6)
    ax = plt.gca()
    box = ax.get_position()
    ax.set_position([box.x0, box.y0, box.width * 0.8, box.height])
    ax.legend(loc="center right", bbox_to_anchor=(1.7, 0.5), fontsize=10)
    plt.tight_layout()
    plt.savefig(out_pdf, dpi=200, bbox_inches="tight")
    plt.close()

    model_csv = out_pdf.with_suffix(".model.csv")
    pd.DataFrame({"freq_MHz": freq_mhz, "Tb_model_K": tb_model}).to_csv(model_csv, index=False)

    print(f"Saved: {out_pdf}")
    print(f"Saved: {model_csv}")


if __name__ == "__main__":
    main()
