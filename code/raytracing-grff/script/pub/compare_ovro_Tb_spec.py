#!/usr/bin/env python
"""
Compare OVRO-LWA and model Tb spectra in two ROIs.

Produces a 2-row, 1-column figure:
- Panel 1: ROI1
- Panel 2: ROI2

For each panel, plots average Tb vs frequency:
- Model (ray-tracing, dashed)
- OVRO-LWA (solid)
"""

from pathlib import Path
import argparse
import re

import numpy as np
from astropy.io import fits
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

R_SUN_M = 6.957e8

# ROI definitions in R_sun
ROI1 = {"name": "ROI1", "x": (-0.15, 0.15), "y": (0.7, 1.0)}
ROI2 = {"name": "ROI2", "x": (0.0, 0.3), "y": (-0.2, 0.1)}


def parse_freq_mhz_from_name(path: Path):
    m = re.search(r"_(\d+\.\d+)MHz\.npz$", path.name)
    if m:
        return float(m.group(1))
    return None


def ensure_lwa_fits(lwa_hdf: Path, lwa_fits: Path):
    if lwa_fits.exists():
        return lwa_fits
    try:
        from ovrolwasolar import utils as outils
    except Exception as e:
        raise FileNotFoundError(
            f"Missing LWA FITS ({lwa_fits}) and cannot convert from HDF because ovrolwasolar is unavailable."
        ) from e
    outils.recover_fits_from_h5(str(lwa_hdf), fits_out=str(lwa_fits))
    return lwa_fits


def load_lwa_data(lwa_fits: Path):
    with fits.open(lwa_fits) as hdul:
        img_k = np.array(hdul[0].data[0], dtype=float)  # (nband, ny, nx), K
        h = hdul[0].header
        tab = hdul[1].data
        freqs_mhz = np.array(tab["cfreqs"], dtype=float) / 1e6

    cdelt1 = float(h["CDELT1"])
    cdelt2 = float(h["CDELT2"])
    crpix1 = float(h["CRPIX1"])
    crpix2 = float(h["CRPIX2"])
    nx = int(h["NAXIS1"])
    ny = int(h["NAXIS2"])
    rsun_arcsec = float(h.get("RSUN_OBS", 945.0))

    # Pixel-center coordinates in R_sun (FITS uses 1-based CRPIX)
    x_rsun = ((np.arange(nx) + 1.0 - crpix1) * cdelt1) / rsun_arcsec
    y_rsun = ((np.arange(ny) + 1.0 - crpix2) * cdelt2) / rsun_arcsec
    xx, yy = np.meshgrid(x_rsun, y_rsun)

    return {
        "img_k": img_k,
        "freqs_mhz": freqs_mhz,
        "xx": xx,
        "yy": yy,
    }


def roi_avg(tb_k, xx, yy, roi):
    mask = (
        (xx >= roi["x"][0]) & (xx <= roi["x"][1]) &
        (yy >= roi["y"][0]) & (yy <= roi["y"][1])
    )
    vals = np.array(tb_k, dtype=float)[mask]
    vals = vals[np.isfinite(vals)]
    if vals.size == 0:
        return np.nan
    return float(np.nanmean(vals))


def collect_model_points(mfs_dir: Path, idx_start=1, idx_end=9):
    files = []
    for idx in range(idx_start, idx_end + 1):
        matches = sorted(mfs_dir.glob(f"raytrace_{idx:02d}_*.npz"))
        if not matches:
            raise FileNotFoundError(f"No model file found for index {idx:02d} in {mfs_dir}")
        files.append(matches[0])

    points = []
    for f in files:
        d = np.load(f)
        tb_k = np.array(d["emission_cube"][:, :, 0], dtype=float)
        x_rsun = np.array(d["x_coords"], dtype=float) / R_SUN_M
        y_rsun = np.array(d["y_coords"], dtype=float) / R_SUN_M
        xx, yy = np.meshgrid(x_rsun, y_rsun)
        freq_mhz = parse_freq_mhz_from_name(f)
        if freq_mhz is None:
            freq_mhz = float(d["frequencies_Hz"][0]) / 1e6
        points.append({"freq_mhz": freq_mhz, "tb_k": tb_k, "xx": xx, "yy": yy})

    points = sorted(points, key=lambda z: z["freq_mhz"])
    return points


def main():
    parser = argparse.ArgumentParser(description="Compare OVRO-LWA and model Tb spectra for two ROIs.")
    parser.add_argument("--mfs-dir", default="script/pub/mfs", help="Directory with model raytrace npz maps")
    parser.add_argument("--idx-start", type=int, default=1, help="Start model index (default 1)")
    parser.add_argument("--idx-end", type=int, default=9, help="End model index (default 9)")
    parser.add_argument("--lwa-hdf", default="script/pub/hdf/ovro-lwa-352.lev1.5_fch_10s.2025-06-08T200703Z.image_I.hdf",
                        help="OVRO-LWA HDF")
    parser.add_argument("--lwa-fits", default="script/pub/hdf/ovro-lwa-352.lev1.5_fch_10s.2025-06-08T200703Z.image_I.fits",
                        help="OVRO-LWA FITS")
    parser.add_argument("--out", default="script/pub/compare_ovro_Tb_spec.pdf", help="Output figure path")
    args = parser.parse_args()

    mfs_dir = Path(args.mfs_dir)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    model_points = collect_model_points(mfs_dir, args.idx_start, args.idx_end)

    lwa_fits = ensure_lwa_fits(Path(args.lwa_hdf), Path(args.lwa_fits))
    lwa = load_lwa_data(lwa_fits)

    rois = [ROI1, ROI2]
    fig, axes = plt.subplots(2, 1, figsize=(4.5, 6), constrained_layout=True, sharex=True)


    ROI_colors = ["limegreen", "deepskyblue"]
    panel_labels = ["(c) ROI-1", "(d) ROI-2"]
    idx_color = 0
    for ax, roi in zip(axes, rois):
        f_model = []
        tb_model_mk = []

        for p in model_points:
            freq_mhz = p["freq_mhz"]
            # Model avg
            mavg_k = roi_avg(p["tb_k"], p["xx"], p["yy"], roi)
            f_model.append(freq_mhz)
            tb_model_mk.append(mavg_k / 1e6 if np.isfinite(mavg_k) else np.nan)

        # OVRO-LWA: use all available frequencies (all bands).
        f_lwa = np.array(lwa["freqs_mhz"], dtype=float)
        tb_lwa_mk = []
        err_lwa_mk = []
        for bd in range(lwa["img_k"].shape[0]):
            band_k = np.array(lwa["img_k"][bd], dtype=float)
            lavg_k = roi_avg(band_k, lwa["xx"], lwa["yy"], roi)
            # noise estimate from image corner
            corner = band_k[0:32, 0:32]
            sigma_k = float(np.nanstd(corner))
            err_k = 3.0 * sigma_k
            if lavg_k > 1e5:
                tb_lwa_mk.append(lavg_k / 1e6 if np.isfinite(lavg_k) else np.nan)
                err_lwa_mk.append(err_k / 1e6 if np.isfinite(err_k) else np.nan)
            else:
                tb_lwa_mk.append(np.nan)
                err_lwa_mk.append(np.nan)

        f_model = np.array(f_model, dtype=float)
        tb_model_mk = np.array(tb_model_mk, dtype=float)
        tb_lwa_mk = np.array(tb_lwa_mk, dtype=float)
        err_lwa_mk = np.array(err_lwa_mk, dtype=float)

        ax.plot(f_model, tb_model_mk, "o--", linewidth=1.8, markersize=4, label="Model", color=ROI_colors[idx_color])
        ax.errorbar(
            f_lwa,
            tb_lwa_mk,
            yerr=err_lwa_mk,
            fmt="o-",
            linewidth=1.6,
            markersize=3.5,
            capsize=2.5,
            label=r"OVRO-LWA",
            color=ROI_colors[idx_color],
        )
        ax.set_ylabel(r"Average $T_B$ (MK)")
        #ax.grid(True, alpha=0.4, linewidth=0.6)
        ax.legend(loc="lower right")
        ax.set_ylim(0, 0.9)
        ax.text(
            0.02, 0.98, panel_labels[idx_color],
            transform=ax.transAxes, ha="left", va="top",
            color=ROI_colors[idx_color], fontsize=12, fontweight="bold"
        )
        #ax.set_title(f"{roi['name']}: x[{roi['x'][0]:.2f},{roi['x'][1]:.2f}], y[{roi['y'][0]:.2f},{roi['y'][1]:.2f}]")
        idx_color += 1
    axes[-1].set_xlabel("Frequency (MHz)")
    fig.savefig(out_path, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")



if __name__ == "__main__":
    main()
