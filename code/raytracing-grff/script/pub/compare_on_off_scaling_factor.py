#!/usr/bin/env python
"""
Compare ray-tracing emission maps with scaling factor input (S) on vs off.

Workflow:
1) Run 60 MHz ray tracing twice: s_input_on=True and s_input_on=False.
2) Save both outputs.
3) Plot 4 panels:
   - S on
   - S off
   - Difference (on - off)
   - Relative difference (on - off) / off
"""

import argparse
from pathlib import Path
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.ndimage import gaussian_filter
import sunpy.visualization.colormaps as cm  # noqa: F401 — registers hinodexrt

xrt_cmap = plt.get_cmap("hinodexrt")
from mpl_toolkits.axes_grid1 import make_axes_locatable

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from raytracingGRFF.coords import PHI0_EARTH_CORONA2298
from script.resample_with_ray_tracing import run_ray_tracing_emission

R_SUN_M = 6.957e8
AU_M = 1.495978707e11
C_M_S = 2.99792458e8


def _load_tb_and_coords(npz_path):
    data = np.load(npz_path)
    tb = np.array(data["emission_cube"][:, :, 0], dtype=float)
    x_coords = np.array(data["x_coords"], dtype=float)
    y_coords = np.array(data["y_coords"], dtype=float)
    return tb, x_coords, y_coords


def _stats_str(v):
    s = f"{v:.2e}"
    m, e = s.split("e")
    return rf"{m}\times 10^{{{int(e)}}}"


def _apply_baseline_beam(tb_map, x_coords_m, y_coords_m, freq_hz, baseline_km):
    if baseline_km <= 0:
        return np.array(tb_map, dtype=float, copy=True)
    out = np.array(tb_map, dtype=float, copy=True)
    if len(x_coords_m) < 2 or len(y_coords_m) < 2:
        return out
    pix_rsun_x = abs((x_coords_m[1] - x_coords_m[0]) / R_SUN_M)
    pix_rsun_y = abs((y_coords_m[1] - y_coords_m[0]) / R_SUN_M)
    pix_rsun = 0.5 * (pix_rsun_x + pix_rsun_y)
    if pix_rsun <= 0:
        return out
    wavelength_m = C_M_S / freq_hz
    theta_rad = wavelength_m / (baseline_km * 1e3)  # diffraction-like beam scale
    beam_fwhm_rsun = theta_rad * AU_M / R_SUN_M
    fwhm_pix = beam_fwhm_rsun / pix_rsun
    sigma_pix = fwhm_pix / 2.355
    if sigma_pix <= 0:
        return out
    return gaussian_filter(out, sigma=sigma_pix)


def _beam_size_rsun(freq_hz, baseline_km):
    wavelength_m = C_M_S / freq_hz
    theta_rad = wavelength_m / (baseline_km * 1e3)
    return theta_rad * AU_M / R_SUN_M


def _plot_four_panel(tb_on, tb_off, x_coords_m, y_coords_m, out_png, freq_hz, baseline_km):
    tb_on = np.nan_to_num(tb_on, nan=0.0, posinf=0.0, neginf=0.0)
    tb_off = np.nan_to_num(tb_off, nan=0.0, posinf=0.0, neginf=0.0)

    # Compare maps after telescope beam convolution.
    tb_on_conv = _apply_baseline_beam(tb_on, x_coords_m, y_coords_m, freq_hz, baseline_km)
    tb_off_conv = _apply_baseline_beam(tb_off, x_coords_m, y_coords_m, freq_hz, baseline_km)
    diff = tb_on_conv - tb_off_conv
    with np.errstate(divide="ignore", invalid="ignore"):
        rel = np.where(tb_off_conv != 0, diff / tb_off_conv, np.nan)

    extent = [
        x_coords_m[0] / R_SUN_M, x_coords_m[-1] / R_SUN_M,
        y_coords_m[0] / R_SUN_M, y_coords_m[-1] / R_SUN_M,
    ]
    x_span = extent[1] - extent[0]
    y_span = extent[3] - extent[2]
    beam_rsun = _beam_size_rsun(freq_hz, baseline_km) if baseline_km > 0 else 0.0

    on_vmax = max(float(np.nanmax(tb_on_conv)), 1.0)
    off_vmax = max(float(np.nanmax(tb_off_conv)), 1.0)
    dmax = np.nanmax(np.abs(diff[np.isfinite(diff)])) if np.any(np.isfinite(diff)) else 1.0
    dmax = max(float(dmax), 1.0)
    rmax = np.nanmax(np.abs(rel[np.isfinite(rel)])) if np.any(np.isfinite(rel)) else 1.0
    rmax = max(float(rmax), 1e-3)

    fig, axes = plt.subplots(1, 4, figsize=(10.5, 3.8), constrained_layout=True)

    im0 = axes[0].imshow(tb_on_conv, origin="lower", extent=extent, aspect="equal", cmap=xrt_cmap, vmin=0.0, vmax=on_vmax)
    axes[0].set_title("With magnification factor", pad=48)
    im1 = axes[1].imshow(tb_off_conv, origin="lower", extent=extent, aspect="equal", cmap=xrt_cmap, vmin=0.0, vmax=off_vmax)
    axes[1].set_title("Without magnification factor", pad=48)
    im2 = axes[2].imshow(diff, origin="lower", extent=extent, aspect="equal", cmap="RdBu_r", vmin=-dmax, vmax=dmax)
    axes[2].set_title("Difference ($I_{a} - I_{b}$)", pad=48)
    im3 = axes[3].imshow(rel, origin="lower", extent=extent, aspect="equal", cmap="RdBu_r", vmin=-rmax, vmax=rmax)
    axes[3].set_title("Relative difference ($I_{a} - I_{b}$)/$I_{b}$", pad=48)

    panel_labels = ["(a)", "(b)", "(c)", "(d)"]
    for i, ax in enumerate(axes):
        ax.set_xlabel(r"x ($R_\odot$)")
        if i == 0:
            ax.set_ylabel(r"y ($R_\odot$)")
            ax.set_xlabel(r"x ($R_\odot$)")
        else:
            ax.set_xlabel(r"x ($R_\odot$)")
        ax.add_patch(plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":"))
        # Beam shape marker (solid) at lower-left.
        if beam_rsun > 0:
            ax.add_patch(
                plt.Circle(
                    (extent[0] + 0.12 * x_span, extent[2] + 0.12 * y_span),
                    beam_rsun,
                    edgecolor="white",
                    facecolor="none",
                    linewidth=1.6,
                )
            )
        label_color = "black" if i >= 2 else "white"
        ax.text(0.03, 0.95, panel_labels[i], transform=ax.transAxes, ha="left", va="top",
                color=label_color, fontsize=12, fontweight="bold")

    # Lower-right T_b^max annotation for panels (a) and (b) only.
    panel_max = [on_vmax, off_vmax, float(np.nanmax(diff)), float(np.nanmax(rel))]
    for i, ax in enumerate(axes):
        if i >= 2:
            continue
        txt_color = "black" if i >= 2 else "white"
        ax.text(
            0.97, 0.05, rf"$T_b^{{\max}}={_stats_str(panel_max[i])}$",
            transform=ax.transAxes, ha="right", va="bottom",
            color=txt_color, fontsize=11, fontweight="bold"
        )

    cbar_defs = [
        (im0, axes[0], r"$T_b$ (K)"),
        (im1, axes[1], r"$T_b$ (K)"),
        (im2, axes[2], r"$\Delta T_b$ (K)"),
        (im3, axes[3], r"$(\Delta T_b/T_b)$"),
    ]
    for im, ax, label in cbar_defs:
        divider = make_axes_locatable(ax)
        cax = divider.append_axes("top", size="9%", pad=0.08)
        cb = fig.colorbar(im, cax=cax, orientation="horizontal")
        cb.set_label(label, labelpad=-28)
        cax.xaxis.set_ticks_position("top")
        cax.xaxis.set_label_position("top")

    fig.savefig(out_png, dpi=170, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Compare emission maps with s_input_on=True vs False (ray tracing only)."
    )
    parser.add_argument("--model-path", "-m", default="./corona2298", help="MAS model path")
    parser.add_argument("--out-dir", default="script/pub", help="Output directory")
    parser.add_argument("--freq", type=float, default=60e6, help="Frequency in Hz (default: 60e6)")

    # Defaults follow low-band compare script style around 60 MHz.
    parser.add_argument("--N-pix", "-n", type=int, default=128, help="Image size")
    parser.add_argument("--X-FOV", type=float, default=2.5, help="Half FOV in R_sun")
    parser.add_argument("--grid-n", type=int, default=256, help="3D grid size")
    parser.add_argument("--grid-extent", type=float, default=3.5, help="3D grid extent in R_sun")
    parser.add_argument("--z-observer", type=float, default=3.5, help="Observer z in R_sun")
    parser.add_argument("--dt", type=float, default=7.75e-3, help="Ray integration dt")
    parser.add_argument("--n-steps", type=int, default=3200, help="Ray integration steps")
    parser.add_argument("--record-stride", type=int, default=6, help="Ray recorder stride")
    parser.add_argument("--phi0-offset", type=float, default=PHI0_EARTH_CORONA2298,
                        help="Carrington longitude at disk center (deg)")
    parser.add_argument("--device", default="cuda", choices=["cpu", "cuda"], help="Sampling device")
    parser.add_argument("--raytrace-device", default="cuda", choices=["cpu", "cuda"], help="Raytrace device")
    parser.add_argument("--workers", type=int, default=1, help="CPU raytrace workers")
    parser.add_argument("--no-fallback", action="store_true", help="Disable CUDA->CPU fallback")
    parser.add_argument("--grff-lib", default=None, help="Optional path to GRFF_DEM_Transfer.so")
    parser.add_argument("--baseline-km", type=float, default=15.0,
                        help="Telescope baseline in km for beam convolution (default: 15)")
    parser.add_argument("--plot-only", action="store_true", help="Skip runs and only plot from existing npz files")
    parser.add_argument("--quiet", "-q", action="store_true", help="Less logging")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    freq_tag = f"{args.freq/1e6:.1f}".replace(".", "p")
    out_on = out_dir / f"raytrace_s_on_{freq_tag}MHz.npz"
    out_off = out_dir / f"raytrace_s_off_{freq_tag}MHz.npz"
    out_fig = out_dir / f"compare_on_off_scaling_factor_{freq_tag}MHz.pdf"

    if not args.plot_only:
        common_kwargs = dict(
            model_path=args.model_path,
            N_pix=args.N_pix,
            X_fov=args.X_FOV,
            freq_hz=args.freq,
            grid_n=args.grid_n,
            grid_extent=args.grid_extent,
            z_observer=args.z_observer,
            dt=args.dt,
            n_steps=args.n_steps,
            record_stride=args.record_stride,
            n_workers=args.workers,
            grff_lib=args.grff_lib,
            Nfreq=1,
            freq0=args.freq,
            freq_log_step=0.0,
            save_plots=False,
            verbose=not args.quiet,
            device=args.device,
            fallback_to_cpu=not args.no_fallback,
            raytrace_device=args.raytrace_device,
            grff_backend="get_mw",
            phi0_offset=args.phi0_offset,
        )
        if not args.quiet:
            print("Running s_input_on=True ...")
        run_ray_tracing_emission(s_input_on=True, out_path=str(out_on), **common_kwargs)
        if not args.quiet:
            print("Running s_input_on=False ...")
        run_ray_tracing_emission(s_input_on=False, out_path=str(out_off), **common_kwargs)

    if not out_on.exists() or not out_off.exists():
        raise FileNotFoundError("Missing on/off npz output files. Run without --plot-only first.")

    tb_on, x_on, y_on = _load_tb_and_coords(out_on)
    tb_off, x_off, y_off = _load_tb_and_coords(out_off)

    if tb_on.shape != tb_off.shape:
        raise ValueError(f"Shape mismatch: on={tb_on.shape}, off={tb_off.shape}")
    if (x_on.shape != x_off.shape) or (y_on.shape != y_off.shape):
        raise ValueError("Coordinate shape mismatch between on/off maps.")

    _plot_four_panel(tb_on, tb_off, x_on, y_on, out_fig, args.freq, args.baseline_km)
    print(f"Saved s-on output:  {out_on}")
    print(f"Saved s-off output: {out_off}")
    print(f"Saved comparison figure: {out_fig}")


if __name__ == "__main__":
    main()
