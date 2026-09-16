#!/usr/bin/env python
"""
Compare OVRO-LWA and ray-tracing model at ~52.4 MHz.

Steps:
1) Load OVRO-LWA image near target frequency (default 52.4 MHz).
2) Load model map from mfs index 05.
3) Convolve model map with baseline beam (default 2 km).
4) Resample both maps to common grid: FOV [-2.8, 2.8] R_sun, 128x128.
5) Plot 2x2 figure:
   (a) OVRO-LWA map
   (b) Model map
   (c) Difference map (model - obs) with bwr colormap
   (d) Central horizontal slice: obs, model, difference
"""

from pathlib import Path
import argparse
import re

import numpy as np
from astropy.io import fits
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Ellipse
from scipy.interpolate import RegularGridInterpolator
from scipy.ndimage import gaussian_filter


import sunpy.visualization.colormaps as cm
xrt_cmap = plt.get_cmap("hinodexrt")

R_SUN_M = 6.957e8
AU_M = 1.495978707e11
C_M_S = 2.99792458e8


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
            f"Missing LWA FITS ({lwa_fits}) and cannot convert from HDF without ovrolwasolar."
        ) from e
    outils.recover_fits_from_h5(str(lwa_hdf), fits_out=str(lwa_fits))
    return lwa_fits


def load_lwa_band(lwa_fits: Path, target_freq_mhz: float):
    with fits.open(lwa_fits) as hdul:
        cube_k = np.array(hdul[0].data[0], dtype=float)  # (nband, ny, nx), K
        h = hdul[0].header
        tab = hdul[1].data
        freqs_mhz = np.array(tab["cfreqs"], dtype=float) / 1e6
        bmaj_deg = np.array(tab["bmaj"], dtype=float)
        bmin_deg = np.array(tab["bmin"], dtype=float)
        bpa_deg = np.array(tab["bpa"], dtype=float)

    bd = int(np.argmin(np.abs(freqs_mhz - target_freq_mhz)))
    img_k = cube_k[bd]
    actual_freq_mhz = float(freqs_mhz[bd])

    cdelt1 = float(h["CDELT1"])
    cdelt2 = float(h["CDELT2"])
    crpix1 = float(h["CRPIX1"])
    crpix2 = float(h["CRPIX2"])
    nx = int(h["NAXIS1"])
    ny = int(h["NAXIS2"])
    rsun_arcsec = float(h.get("RSUN_OBS", 945.0))

    x_rsun = ((np.arange(nx) + 1.0 - crpix1) * cdelt1) / rsun_arcsec
    y_rsun = ((np.arange(ny) + 1.0 - crpix2) * cdelt2) / rsun_arcsec
    beam = {
        "bmaj_rsun": bmaj_deg[bd] * 3600.0 / rsun_arcsec,
        "bmin_rsun": bmin_deg[bd] * 3600.0 / rsun_arcsec,
        "bpa_deg": bpa_deg[bd],
    }
    return img_k, x_rsun, y_rsun, actual_freq_mhz, beam


def load_model_map(model_npz: Path):
    d = np.load(model_npz)
    img_k = np.array(d["emission_cube"][:, :, 0], dtype=float)
    x_rsun = np.array(d["x_coords"], dtype=float) / R_SUN_M
    y_rsun = np.array(d["y_coords"], dtype=float) / R_SUN_M
    f = parse_freq_mhz_from_name(model_npz)
    if f is None:
        f = float(d["frequencies_Hz"][0]) / 1e6
    return img_k, x_rsun, y_rsun, float(f)


def beam_fwhm_rsun(freq_hz: float, baseline_km: float):
    theta_rad = (C_M_S / freq_hz) / (baseline_km * 1e3)
    return theta_rad * AU_M / R_SUN_M


def apply_model_beam(img_k, x_rsun, y_rsun, freq_hz, baseline_km):
    if len(x_rsun) < 2 or len(y_rsun) < 2:
        return img_k.copy()
    pix_rsun = 0.5 * (abs(x_rsun[1] - x_rsun[0]) + abs(y_rsun[1] - y_rsun[0]))
    fwhm_rsun = beam_fwhm_rsun(freq_hz, baseline_km)
    sigma_pix = (fwhm_rsun / pix_rsun) / 2.355 if pix_rsun > 0 else 0.0
    if sigma_pix <= 0:
        return img_k.copy()
    return gaussian_filter(img_k, sigma=sigma_pix)


def resample_to_grid(img_k, x_rsun, y_rsun, x_new, y_new):
    interp = RegularGridInterpolator(
        (y_rsun, x_rsun), img_k, bounds_error=False, fill_value=np.nan
    )
    xx_new, yy_new = np.meshgrid(x_new, y_new)
    pts = np.column_stack([yy_new.ravel(), xx_new.ravel()])
    out = interp(pts).reshape(len(y_new), len(x_new))
    return out


def main():
    parser = argparse.ArgumentParser(description="Compare OVRO-LWA and model map at ~52.4 MHz.")
    parser.add_argument("--target-freq-mhz", type=float, default=66, help="Target LWA frequency")
    parser.add_argument("--model-npz", default="/home/pjzhang/dev/GRFFradioSun/script/pub/mfs/raytrace_07_0066.271MHz.npz", help="Model npz path")
    parser.add_argument("--lwa-hdf", default="script/pub/hdf/ovro-lwa-352.lev1.5_fch_10s.2025-06-08T200703Z.image_I.hdf",
                        help="LWA HDF path")
    parser.add_argument("--lwa-fits", default="script/pub/hdf/ovro-lwa-352.lev1.5_fch_10s.2025-06-08T200703Z.image_I.fits",
                        help="LWA FITS path")
    parser.add_argument("--baseline-km", type=float, default=2.0, help="Model baseline beam in km")
    parser.add_argument("--fov-rsun", type=float, default=2.8, help="Half FOV in R_sun for resampling")
    parser.add_argument("--grid-n", type=int, default=128, help="Common resampled grid size")
    parser.add_argument("--out", default="script/pub/compare_diff_and_slice.pdf", help="Output figure")
    args = parser.parse_args()

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    lwa_fits = ensure_lwa_fits(Path(args.lwa_hdf), Path(args.lwa_fits))
    lwa_k, lwa_x, lwa_y, lwa_freq_mhz, lwa_beam = load_lwa_band(lwa_fits, args.target_freq_mhz)
    model_k, model_x, model_y, model_freq_mhz = load_model_map(Path(args.model_npz))
    model_k = apply_model_beam(model_k, model_x, model_y, model_freq_mhz * 1e6, args.baseline_km)

    xg = np.linspace(-args.fov_rsun, args.fov_rsun, args.grid_n)
    yg = np.linspace(-args.fov_rsun, args.fov_rsun, args.grid_n)
    lwa_rs = resample_to_grid(lwa_k, lwa_x, lwa_y, xg, yg)
    model_rs = resample_to_grid(model_k, model_x, model_y, xg, yg)

    lwa_mk = lwa_rs / 1e6
    model_mk = model_rs / 1e6
    diff_mk = - model_mk + lwa_mk

    vmax_lwa = np.nanmax(lwa_mk) if np.any(np.isfinite(lwa_mk)) else 1.0
    vmax_model = np.nanmax(model_mk) if np.any(np.isfinite(model_mk)) else 1.0
    dmax = np.nanmax(np.abs(diff_mk)) if np.any(np.isfinite(diff_mk)) else 1.0
    if not np.isfinite(vmax_lwa) or vmax_lwa <= 0:
        vmax_lwa = 1.0
    if not np.isfinite(vmax_model) or vmax_model <= 0:
        vmax_model = 1.0
    if not np.isfinite(dmax) or dmax <= 0:
        dmax = 1.0

    fig = plt.figure(figsize=(13*0.7, 8*0.7))
    # Explicit axes placement: [left, bottom, width, height] in figure fraction.
    # Top row: 3 columns
    ax_a = fig.add_axes([0.06,  0.55, 0.24, 0.3])
    ax_b = fig.add_axes([0.365, 0.55, 0.24, 0.3])
    ax_c = fig.add_axes([0.67,  0.55, 0.24, 0.3])
    # Bottom row: 2 columns
    ax_d = fig.add_axes([0.095, 0.09, 0.35, 0.29])
    ax_e = fig.add_axes([0.57, 0.09, 0.35, 0.29])
    extent = [xg[0], xg[-1], yg[0], yg[-1]]

    im0 = ax_a.imshow(lwa_mk, origin="lower", extent=extent, cmap=xrt_cmap, vmin=0, vmax=vmax_lwa)
    ax_a.add_patch(Circle((0, 0), 1.0, fill=False, ls="-", lw=1.2, color="w", alpha=0.8))
    # LWA restoring beam (ellipse) at lower-left
    ax_a.add_patch(
        Ellipse(
            (-args.fov_rsun * 0.75, -args.fov_rsun * 0.75),
            width=lwa_beam["bmaj_rsun"],
            height=lwa_beam["bmin_rsun"],
            angle=-(90.0 - lwa_beam["bpa_deg"]),
            fill=False,
            ec="w",
            lw=2.0,
        )
    )
    ax_a.axvline(0, color="w", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_a.axhline(0, color="w", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_a.set_title(f"(a) OVRO-LWA {lwa_freq_mhz:.1f} MHz")
    ax_a.set_xlabel(r"x ($R_\odot$)")
    ax_a.set_ylabel(r"y ($R_\odot$)")
    plt.colorbar(im0, ax=ax_a, fraction=0.046, pad=0.02, label=r"$T_B$ (MK)")

    im1 = ax_b.imshow(model_mk, origin="lower", extent=extent, cmap=xrt_cmap, vmin=0, vmax=vmax_lwa)
    ax_b.add_patch(Circle((0, 0), 1.0, fill=False, ls="-", lw=1.2, color="w", alpha=0.8))
    # Model beam (assumed circular) at lower-left
    model_beam_rsun = beam_fwhm_rsun(model_freq_mhz * 1e6, args.baseline_km)
    ax_b.add_patch(
        Circle(
            (-args.fov_rsun * 0.75, -args.fov_rsun * 0.75),
            model_beam_rsun/2,
            fill=False,
            ec="w",
            lw=2.0,
        )
    )
    ax_b.axvline(0, color="w", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_b.axhline(0, color="w", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_b.set_title(f"(b) Model {model_freq_mhz:.1f} MHz")
    ax_b.set_xlabel(r"x ($R_\odot$)")
    #ax_b.set_ylabel(r"y ($R_\odot$)")
    plt.colorbar(im1, ax=ax_b, fraction=0.046, pad=0.02, label=r"$T_B$ (MK)")

#    diff_mk[lwa_mk < 0.2] = np.nan

    im2 = ax_c.imshow(diff_mk/np.max(lwa_mk), origin="lower", extent=extent, cmap="bwr", vmin=-1, vmax=1)
    ax_c.add_patch(Circle((0, 0), 1.0, fill=False, ls="-", lw=1.2, color="k", alpha=0.8))
    ax_c.axvline(0, color="k", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_c.axhline(0, color="k", lw=0.6, alpha=0.6, ls=":", zorder=10)
    ax_c.set_title(r"(c)  $(I_{obs} - I_{model})/I_{obs}^{max}$")
    ax_c.set_xlabel(r"x ($R_\odot$)")
    #ax_c.set_ylabel(r"y ($R_\odot$)")
    plt.colorbar(im2, ax=ax_c, fraction=0.046, pad=0.02)

    # Central horizontal slice (closest y=0)
    j0 = int(np.argmin(np.abs(yg - 0.0)))
    obs_slice = lwa_mk[j0, :]
    model_slice = model_mk[j0, :]
    diff_slice = diff_mk[j0, :]

    ax_d.plot(xg, obs_slice, "-", lw=1.8, label="OVRO")
    ax_d.plot(xg, model_slice, "-", lw=1.8, label="Model")
    ax_d.plot(xg, diff_slice, "k--", lw=1.8, label="Diff")
    ax_d.axhline(0.0, color="k", lw=0.8, alpha=0.6)
    ax_d.set_ylim(-0.1, 0.8)
    ax_d.set_title("(d) Slice at y=0")
    ax_d.set_xlabel(r"x ($R_\odot$)")
    ax_d.set_ylabel(r"$T_B$ (MK)")

    ax_d.text(-2.5, 0.74, "slice", color="k", fontsize=12, horizontalalignment="center", verticalalignment="center")
    ax_d.scatter(-2.5, 0.6, color="orange", s=250, alpha=0.8)
    ax_d.scatter(-2.5, 0.6, marker="s", color="none", edgecolor="k", s=700, alpha=0.8)
    ax_d.plot([-2.5-0.28, -2.5+0.28], [0.6, 0.6], "k-", lw=1.8)


    #ax_d.grid(True, alpha=0.35, linewidth=0.6)
    ax_d.legend(loc="upper right", fontsize=9)

    # Central vertical slice (closest x=0)
    i0 = int(np.argmin(np.abs(xg - 0.0)))
    obs_slice_x0 = lwa_mk[:, i0]
    model_slice_x0 = model_mk[:, i0]
    diff_slice_x0 = diff_mk[:, i0]

    ax_e.plot(yg, obs_slice_x0, "-", lw=1.8, label="OVRO")
    ax_e.plot(yg, model_slice_x0, "-", lw=1.8, label="Model")
    ax_e.plot(yg, diff_slice_x0, "k--", lw=1.8, label="Diff")
    ax_e.axhline(0.0, color="k", lw=0.8, alpha=0.6)

    ax_e.text(-2.5, 0.74, "slice", color="k", fontsize=12, horizontalalignment="center", verticalalignment="center")
    ax_e.scatter(-2.5, 0.6, color="orange", s=250, alpha=0.8)
    ax_e.scatter(-2.5, 0.6, marker="s", color="none", edgecolor="k", s=700, alpha=0.8)
    ax_e.plot([-2.5, -2.5], [0.6-0.07, 0.6+0.07], "k-", lw=1.8)

    ax_e.set_ylim(-0.1, 0.8)
    ax_e.set_title("(e) Slice at x=0")
    ax_e.set_xlabel(r"y ($R_\odot$)")
    ax_e.set_ylabel(r"$T_B$ (MK)")
    #ax_e.grid(True, alpha=0.35, linewidth=0.6)
    ax_e.legend(loc="upper right", fontsize=9)

    fig.savefig(out_path, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")


if __name__ == "__main__":
    main()
