#!/usr/bin/env python
"""
Compare OVRO-LWA images with ray-tracing GRFF model maps in a 3x2 panel figure.

Layout:
- Left column: OVRO-LWA (matched nearest band for each model frequency)
- Right column: ray-tracing model maps from mfs indices 02, 05, 09

Model maps are convolved with a baseline beam (default 2.5 km) and beam shapes
are plotted.
"""

from pathlib import Path
import argparse
import re

import sunpy.visualization.colormaps as cm

import numpy as np
from astropy.io import fits
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Ellipse, Circle, Rectangle
from scipy.ndimage import gaussian_filter

xrt_cmap = plt.get_cmap('hinodexrt')

R_SUN_M = 6.957e8
AU_M = 1.495978707e11
C_M_S = 2.99792458e8
ROI1 = (-0.15, 0.7, 0.30, 0.30)   # x0, y0, width, height
ROI2 = (0.0, -0.2, 0.30, 0.30)    # x0, y0, width, height


def parse_freq_mhz_from_name(path: Path):
    m = re.search(r"_(\d+\.\d+)MHz\.npz$", path.name)
    if m:
        return float(m.group(1))
    return None


def load_model_npz(npz_path: Path):
    d = np.load(npz_path)
    tb_k = np.array(d["emission_cube"][:, :, 0], dtype=float)
    x_rsun = np.array(d["x_coords"], dtype=float) / R_SUN_M
    y_rsun = np.array(d["y_coords"], dtype=float) / R_SUN_M
    freq_mhz = parse_freq_mhz_from_name(npz_path)
    if freq_mhz is None:
        freq_mhz = float(d["frequencies_Hz"][0]) / 1e6
    return tb_k, x_rsun, y_rsun, freq_mhz


def beam_fwhm_rsun(freq_hz: float, baseline_km: float):
    theta_rad = (C_M_S / freq_hz) / (baseline_km * 1e3)
    return theta_rad * AU_M / R_SUN_M


def convolve_model_beam(tb_k, x_rsun, y_rsun, freq_hz, baseline_km):
    if len(x_rsun) < 2 or len(y_rsun) < 2:
        return tb_k.copy(), 0.0
    pix_rsun = 0.5 * (abs(x_rsun[1] - x_rsun[0]) + abs(y_rsun[1] - y_rsun[0]))
    fwhm_rsun = beam_fwhm_rsun(freq_hz, baseline_km)
    sigma_pix = (fwhm_rsun / pix_rsun) / 2.355 if pix_rsun > 0 else 0.0
    if sigma_pix <= 0:
        return tb_k.copy(), fwhm_rsun
    return gaussian_filter(tb_k, sigma=sigma_pix), fwhm_rsun


def ensure_lwa_fits(lwa_hdf: Path, lwa_fits: Path):
    if lwa_fits.exists():
        return lwa_fits
    try:
        from ovrolwasolar import utils as outils
    except Exception as e:
        raise FileNotFoundError(
            f"LWA FITS not found: {lwa_fits}, and ovrolwasolar is unavailable for HDF conversion."
        ) from e
    outils.recover_fits_from_h5(str(lwa_hdf), fits_out=str(lwa_fits))
    return lwa_fits


def load_lwa_fits(lwa_fits: Path):
    with fits.open(lwa_fits) as hdul:
        img = np.array(hdul[0].data[0], dtype=float)  # (nband, ny, nx)
        h = hdul[0].header
        tab = hdul[1].data
        freqs_mhz = np.array(tab["cfreqs"], dtype=float) / 1e6
        bmaj_deg = np.array(tab["bmaj"], dtype=float)
        bmin_deg = np.array(tab["bmin"], dtype=float)
        bpa_deg = np.array(tab["bpa"], dtype=float)

    cdelt1 = float(h["CDELT1"])
    cdelt2 = float(h["CDELT2"])
    crpix1 = float(h["CRPIX1"])
    crpix2 = float(h["CRPIX2"])
    nx = int(h["NAXIS1"])
    ny = int(h["NAXIS2"])
    rsun_arcsec = float(h.get("RSUN_OBS", 945.0))

    left = (0.5 - crpix1) * cdelt1
    right = (nx - 0.5 - crpix1) * cdelt1
    bottom = (0.5 - crpix2) * cdelt2
    top = (ny - 0.5 - crpix2) * cdelt2
    extent_rsun = [left / rsun_arcsec, right / rsun_arcsec, bottom / rsun_arcsec, top / rsun_arcsec]

    return {
        "img_k": img,
        "freqs_mhz": freqs_mhz,
        "bmaj_deg": bmaj_deg,
        "bmin_deg": bmin_deg,
        "bpa_deg": bpa_deg,
        "extent_rsun": extent_rsun,
        "rsun_arcsec": rsun_arcsec,
    }


def nearest_lwa_band(freq_mhz, lwa_freqs_mhz):
    return int(np.argmin(np.abs(lwa_freqs_mhz - freq_mhz)))


def main():
    parser = argparse.ArgumentParser(description="3x2 OVRO-LWA vs ray-tracing model image comparison.")
    parser.add_argument("--mfs-dir", default="script/pub/mfs", help="Directory containing raytrace_*.npz")
    parser.add_argument("--model-indices", nargs=3, type=int, default=[2, 5, 9],
                        help="Three mfs indices for model maps (default: 2 5 9)")
    parser.add_argument("--lwa-hdf", default="script/pub/hdf/ovro-lwa-352.lev1.5_mfs_10s.2025-06-08T200703Z.image_I.hdf",
                        help="OVRO-LWA HDF input")
    parser.add_argument("--lwa-fits", default="script/pub/hdf/ovro-lwa-352.lev1.5_mfs_10s.2025-06-08T200703Z.image_I.fits",
                        help="OVRO-LWA FITS file (generated from HDF if needed)")
    parser.add_argument("--baseline-km", type=float, default=2, help="Model beam baseline in km")
    parser.add_argument("--fov-rsun", type=float, default=5.6, help="Displayed FoV width in R_sun")
    parser.add_argument("--out", default="script/pub/compare_ovro_lwa_with_model_img.pdf", help="Output figure path")
    args = parser.parse_args()

    mfs_dir = Path(args.mfs_dir)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    model_files = [mfs_dir / f"raytrace_{i:02d}_" for i in args.model_indices]
    resolved_model_files = []
    for prefix in model_files:
        matches = sorted(mfs_dir.glob(prefix.name + "*.npz"))
        if not matches:
            raise FileNotFoundError(f"No model npz found for index prefix: {prefix.name}")
        resolved_model_files.append(matches[0])

    lwa_fits = ensure_lwa_fits(Path(args.lwa_hdf), Path(args.lwa_fits))
    lwa = load_lwa_fits(lwa_fits)

    fig, axes = plt.subplots(3, 2, figsize=(5, 7.5), constrained_layout=True)
    fov_half = args.fov_rsun / 2.0

    for row, model_npz in enumerate(resolved_model_files):
        # Model map
        model_tb_k, x_rsun, y_rsun, model_freq_mhz = load_model_npz(model_npz)
        model_tb_k = np.nan_to_num(model_tb_k, nan=0.0, posinf=0.0, neginf=0.0)
        model_conv_k, model_beam_fwhm_rsun = convolve_model_beam(
            model_tb_k, x_rsun, y_rsun, model_freq_mhz * 1e6, args.baseline_km
        )
        model_mk = model_conv_k / 1e6
        model_extent = [x_rsun[0], x_rsun[-1], y_rsun[0], y_rsun[-1]]
        model_vmax = np.nanmax(model_mk) if np.any(np.isfinite(model_mk)) else 1.0
        if not np.isfinite(model_vmax) or model_vmax <= 0:
            model_vmax = 1.0

        # Matched LWA band
        bd = nearest_lwa_band(model_freq_mhz, lwa["freqs_mhz"])
        lwa_img_mk = np.array(lwa["img_k"][bd], dtype=float) / 1e6
        lwa_vmax = np.nanmax(lwa_img_mk) if np.any(np.isfinite(lwa_img_mk)) else 1.0
        if not np.isfinite(lwa_vmax) or lwa_vmax <= 0:
            lwa_vmax = 1.0

        # Left: LWA
        ax_l = axes[row, 0]
        ax_l.set_facecolor("black")
        ax_l.imshow(lwa_img_mk, origin="lower", extent=lwa["extent_rsun"], cmap=xrt_cmap, vmin=0, vmax=lwa_vmax)
        ax_l.add_patch(Circle((0, 0), 1.0, fill=False, ls="-", lw=1.2, color="w", alpha=0.8))
        ax_l.add_patch(Rectangle((ROI1[0], ROI1[1]), ROI1[2], ROI1[3], fill=False, ec="lime", lw=1.6))
        ax_l.add_patch(Rectangle((ROI2[0], ROI2[1]), ROI2[2], ROI2[3], fill=False, ec="deepskyblue", lw=1.6))
        bmaj_rsun = lwa["bmaj_deg"][bd] * 3600.0 / lwa["rsun_arcsec"]
        bmin_rsun = lwa["bmin_deg"][bd] * 3600.0 / lwa["rsun_arcsec"]
        bpa = lwa["bpa_deg"][bd]
        beam_lwa = Ellipse(
            (-fov_half * 0.75, -fov_half * 0.75),
            bmaj_rsun,
            bmin_rsun,
            angle=-(90.0 - bpa),
            fc="none",
            ec="w",
            lw=1.8,
        )
        ax_l.add_patch(beam_lwa)
        ax_l.set_xlim([-fov_half, fov_half])
        ax_l.set_ylim([-fov_half, fov_half])
        ax_l.set_aspect("equal")
        if row == 2:
            ax_l.set_xlabel(r"x ($R_\odot$)")
        else:
            ax_l.set_xlabel(r"")
        ax_l.set_ylabel(r"y ($R_\odot$)")
        ax_l.text(0.98, 0.98, f"{lwa['freqs_mhz'][bd]:.1f} MHz", color="w", ha="right", va="top",
                  transform=ax_l.transAxes, fontsize=11, fontweight="bold")
        ax_l.text(0.03, 0.97, f"(a{row+1})", color="w", ha="left", va="top",
                  transform=ax_l.transAxes, fontsize=11, fontweight="bold")
        ax_l.text(0.98, 0.03, rf"$T_b^{{\max}}={lwa_vmax:.2f}\,\mathrm{{MK}}$", color="w",
                  ha="right", va="bottom", transform=ax_l.transAxes, fontsize=10, fontweight="bold")
        if row == 0:
            ax_l.set_title("OVRO-LWA")

        # Right: Model
        ax_r = axes[row, 1]
        ax_r.set_facecolor("black")
        ax_r.imshow(model_mk, origin="lower", extent=model_extent, cmap=xrt_cmap, vmin=0, vmax=lwa_vmax)
        ax_r.add_patch(Circle((0, 0), 1.0, fill=False, ls="-", lw=1.2, color="w", alpha=0.8))
        ax_r.add_patch(Rectangle((ROI1[0], ROI1[1]), ROI1[2], ROI1[3], fill=False, ec="lime", lw=1.6))
        ax_r.add_patch(Rectangle((ROI2[0], ROI2[1]), ROI2[2], ROI2[3], fill=False, ec="deepskyblue", lw=1.6))
        # Circular beam shape for model baseline
        ax_r.add_patch(Circle(
            (-fov_half * 0.75, -fov_half * 0.75),
            model_beam_fwhm_rsun/2,
            fill=False, ls="-", lw=1.8, color="w"
        ))
        ax_r.set_xlim([-fov_half, fov_half])
        ax_r.set_ylim([-fov_half, fov_half])
        ax_r.set_aspect("equal")
        if row == 2:
            ax_r.set_xlabel(r"x ($R_\odot$)")
        else:
            ax_r.set_xlabel(r"")
        ax_r.set_ylabel(r"")
        ax_r.text(0.98, 0.98, f"{model_freq_mhz:.1f} MHz", color="w", ha="right", va="top",
                  transform=ax_r.transAxes, fontsize=11, fontweight="bold")
        ax_r.text(0.03, 0.97, f"(b{row+1})", color="w", ha="left", va="top",
                  transform=ax_r.transAxes, fontsize=11, fontweight="bold")
        ax_r.text(0.98, 0.03, rf"$T_b^{{\max}}={model_vmax:.2f}\,\mathrm{{MK}}$", color="w",
                  ha="right", va="bottom", transform=ax_r.transAxes, fontsize=10, fontweight="bold")
        if row == 0:
            ax_r.set_title(f"Ray-tracing MAS GRFF")

    fig.savefig(out_path, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")


if __name__ == "__main__":
    main()
