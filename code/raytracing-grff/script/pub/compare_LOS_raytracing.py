#!/usr/bin/env python
"""
Compare ray-tracing vs straight-LOS GRFF emission maps at multiple frequencies.

This script runs both pipelines for each target frequency:
1) Ray tracing path from script/resample_with_ray_tracing.py
2) LOS path from LOS/resample_MAS_LOS.py + LOS/grff_image_from_LOS.py

It then plots a 2x3 figure:
- Row 1: Ray tracing (50, 100, 300 MHz by default)
- Row 2: LOS (50, 100, 300 MHz by default)
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

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from raytracingGRFF.coords import PHI0_EARTH_CORONA2298
from script.resample_with_ray_tracing import run_ray_tracing_emission
from LOS.resample_MAS_LOS import resample_MAS
import LOS.grff_image_from_LOS as los_grff

R_SUN_M = 6.957e8
# Plot-time Gaussian beam: FWHM[Rsun] = beam_factor / f[Hz].
# Pub convention: array size D[km] ≈ 32e6 / beam_factor (32e6 → ~1 km, 16e6 → ~2 km).
# Plot FWHM[Rsun] = beam_factor / f[Hz]. Strict λ/D θ=64.5/(D[km]·f[MHz]) needs beam_factor ≈ 6.45e7/D.
BEAM_FACTOR_DEFAULT = 16e6   # low band: ~2 km
GAUSSIAN_FWHM_TO_SIGMA = 2.355


def beam_fwhm_rsun(freq_hz, beam_factor):
    """Beam FWHM in R_sun from beam_factor and frequency (Hz)."""
    return beam_factor / freq_hz


def frequency_scaled_params(
    freq_hz,
    ref_freq_hz,
    base_dt,
    base_n_steps,
    base_record_stride,
    base_dz0,
    base_nz,
    scaling_exp,
    min_n_steps,
    min_nz,
):
    """
    Scale integration settings with frequency.
    Lower frequency -> larger steps (larger dt, dz0) and fewer steps/samples.
    """
    scale = (ref_freq_hz / freq_hz) ** scaling_exp
    dt = base_dt * scale
    n_steps = max(min_n_steps, int(round(base_n_steps / max(scale, 1e-12))))
    record_stride = max(1, int(round(base_record_stride * scale)))
    dz0 = base_dz0 * scale
    n_z = max(min_nz, int(round(base_nz / max(scale, 1e-12))))
    return {
        "dt": dt,
        "n_steps": n_steps,
        "record_stride": record_stride,
        "dz0": dz0,
        "n_z": n_z,
    }


def _apply_plot_beam(map_in, freq_hz, beam_factor, x_coords_m, y_coords_m):
    """Convolve map with circular Gaussian beam (FWHM = beam_factor / f[Hz])."""
    out = np.array(map_in, dtype=float, copy=True)
    fwhm_rsun = beam_fwhm_rsun(freq_hz, beam_factor)
    if fwhm_rsun <= 0 or out.size == 0:
        return out
    if len(x_coords_m) < 2 or len(y_coords_m) < 2:
        return out
    dx_rsun = abs((x_coords_m[1] - x_coords_m[0]) / R_SUN_M)
    dy_rsun = abs((y_coords_m[1] - y_coords_m[0]) / R_SUN_M)
    pix_rsun = 0.5 * (dx_rsun + dy_rsun)
    if pix_rsun <= 0:
        return out
    sigma_pix = (fwhm_rsun / pix_rsun) / GAUSSIAN_FWHM_TO_SIGMA
    if sigma_pix <= 0:
        return out
    return gaussian_filter(out, sigma=sigma_pix)


def _plot_compare_2x3(
    ray_maps,
    los_maps,
    freqs_hz,
    x_coords_m,
    y_coords_m,
    out_png,
    plot_consider_beam=True,
    beam_factor=BEAM_FACTOR_DEFAULT,
):
    fig, axes = plt.subplots(2, 3, figsize=(9.9, 6.6), constrained_layout=True)

    x0, x1 = x_coords_m[0] / R_SUN_M, x_coords_m[-1] / R_SUN_M
    y0, y1 = y_coords_m[0] / R_SUN_M, y_coords_m[-1] / R_SUN_M
    extent = [x0, x1, y0, y1]
    x_span = x1 - x0
    y_span = y1 - y0

    panel_labels = [["(a1)", "(a2)", "(a3)"], ["(b1)", "(b2)", "(b3)"]]

    for col, freq_hz in enumerate(freqs_hz):
        freq_mhz = freq_hz / 1e6
        beam_fwhm = beam_fwhm_rsun(freq_hz, beam_factor)

        ray_map = np.array(ray_maps[col], dtype=float)
        los_map = np.array(los_maps[col], dtype=float)
        if plot_consider_beam:
            ray_map = _apply_plot_beam(ray_map, freq_hz, beam_factor, x_coords_m, y_coords_m)
            los_map = _apply_plot_beam(los_map, freq_hz, beam_factor, x_coords_m, y_coords_m)

        ray_vmax = np.nanmax(ray_map) if np.any(np.isfinite(ray_map)) else 1.0
        los_vmax = np.nanmax(los_map) if np.any(np.isfinite(los_map)) else 1.0
        if ray_vmax <= 0:
            ray_vmax = 1.0
        if los_vmax <= 0:
            los_vmax = 1.0

        im0 = axes[0, col].imshow(
            ray_map,
            origin="lower",
            extent=extent,
            aspect="equal",
            cmap=xrt_cmap,
            vmin=0.0,
            vmax=ray_vmax*1.05,
            interpolation="bilinear",
        )
        axes[0, col].set_title(f"Ray tracing {freq_mhz:.0f} MHz")
        axes[0, col].set_xlabel(r"x ($R_\odot$)")
        axes[0, col].set_ylabel(r"y ($R_\odot$)")
        axes[0, col].text(
            0.03, 0.95, panel_labels[0][col],
            transform=axes[0, col].transAxes,
            ha="left", va="top", color="white", fontsize=12, fontweight="bold"
        )

        formatted = f"{ray_vmax:.1e}"
        mantissa, exponent = formatted.split('e')
        latex_str = f"{mantissa} \\times 10^{{{int(exponent)}}}"

        axes[0, col].text(
            0.97, 0.05, rf"$T_b^{{\max}} = {latex_str}\,\mathrm{{K}}$",
            transform=axes[0, col].transAxes,
            ha="right", va="bottom", color="white", fontsize=12, fontweight="bold"
        )
        axes[0, col].add_patch(
            plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":")
        )
        if plot_consider_beam and beam_fwhm > 0:
            cx = x0 + 0.12 * x_span
            cy = y0 + 0.12 * y_span
            axes[0, col].add_patch(
                plt.Circle((cx, cy), beam_fwhm / 2.0, edgecolor="white", facecolor="none", linewidth=1.8)
            )

        im1 = axes[1, col].imshow(
            los_map,
            origin="lower",
            extent=extent,
            aspect="equal",
            cmap=xrt_cmap,
            vmin=0.0,
            vmax=los_vmax*1.05,
            interpolation="bilinear",
        )
        axes[1, col].set_title(f"LOS {freq_mhz:.0f} MHz")
        axes[1, col].set_xlabel(r"x ($R_\odot$)")
        axes[1, col].set_ylabel(r"y ($R_\odot$)")
        axes[1, col].text(
            0.03, 0.95, panel_labels[1][col],
            transform=axes[1, col].transAxes,
            ha="left", va="top", color="white", fontsize=12, fontweight="bold"
        )


        formatted = f"{los_vmax:.1e}"
        mantissa, exponent = formatted.split('e')
        latex_str = f"{mantissa} \\times 10^{{{int(exponent)}}}"
        axes[1, col].text(
            0.97, 0.05, rf"$T_b^{{\max}} = {latex_str}\,\mathrm{{K}}$",
            transform=axes[1, col].transAxes,
            ha="right", va="bottom", color="white", fontsize=12, fontweight="bold"
        )
        axes[1, col].add_patch(
            plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":")
        )
        if plot_consider_beam and beam_fwhm > 0:
            cx = x0 + 0.12 * x_span
            cy = y0 + 0.12 * y_span
            axes[1, col].add_patch(
                plt.Circle((cx, cy), beam_fwhm / 2.0, edgecolor="white", facecolor="none", linewidth=1.8)
            )

    fig.savefig(out_png, dpi=180, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Compare LOS and ray-tracing emission maps at multiple frequencies."
    )
    parser.add_argument("--model-path", "-m", default="./corona2298", help="MAS model path")
    parser.add_argument("--out-dir", default="script/pub/out_compare_los_ray", help="Output directory")
    parser.add_argument("--freqs-mhz", type=float, nargs="+", default=[40.0, 80.0, 150.0],
                        help="Frequencies in MHz (default: 40 80 150)")

    # Shared imaging/grid settings
    parser.add_argument("--N-pix", "-n", type=int, default=128, help="Image size")
    parser.add_argument("--X-FOV", type=float, default=2.5, help="Half FOV in R_sun")
    parser.add_argument("--grid-n", type=int, default=256, help="3D cube grid size")
    parser.add_argument("--grid-extent", type=float, default=3.5, help="3D cube half extent in R_sun")
    parser.add_argument("--z-observer", type=float, default=3.5, help="Observer z in R_sun")
    parser.add_argument("--phi0-offset", type=float, default=PHI0_EARTH_CORONA2298,
                        help="Carrington longitude at disk center (deg)")

    parser.add_argument("--device", default="cuda", choices=["cpu", "cuda"], help="Sampling device")
    parser.add_argument("--raytrace-device", default="cuda", choices=["cpu", "cuda"], help="Raytrace device")
    parser.add_argument("--workers", type=int, default=1, help="CPU raytrace workers")
    parser.add_argument("--no-fallback", action="store_true", help="Disable CUDA->CPU fallback")
    parser.add_argument("--beam-fwhm-rsun", type=float, default=None, help="Gaussian beam FWHM in R_sun")
    parser.add_argument("--beam-diameter-m", type=float, default=None, help="Telescope D (m) for λ/D beam")

    # Frequency scaling controls
    parser.add_argument("--ref-freq-mhz", type=float, default=100.0, help="Reference frequency for scaling")
    parser.add_argument("--scaling-exp", type=float, default=0.5, help="Scaling exponent")
    parser.add_argument("--base-dt", type=float, default=6e-3, help="Ray dt at reference frequency")
    parser.add_argument("--base-n-steps", type=int, default=4000, help="Ray n_steps at reference frequency")
    parser.add_argument("--base-record-stride", type=int, default=5, help="Ray record_stride at reference frequency")
    parser.add_argument("--base-dz0", type=float, default=3e-4, help="LOS dz0 at reference frequency (R_sun)")
    parser.add_argument("--base-n-z", type=int, default=400, help="LOS N_z at reference frequency")
    parser.add_argument("--min-n-steps", type=int, default=1200, help="Minimum ray n_steps")
    parser.add_argument("--min-n-z", type=int, default=120, help="Minimum LOS N_z")

    parser.add_argument("--grff-lib", default=None, help="Optional path to GRFF_DEM_Transfer.so")
    parser.add_argument("--plot-only", action="store_true",
                        help="Only plot from existing npz maps in --out-dir")
    parser.add_argument("--plot-consider-beam", action="store_true", default=True,
                        help="Convolve maps with freq-dependent Gaussian beam at plot time (default: on)")
    parser.add_argument("--no-plot-beam", action="store_false", dest="plot_consider_beam",
                        help="Disable plot-time beam convolution")
    parser.add_argument("--beam-factor", type=float, default=BEAM_FACTOR_DEFAULT,
                        help="Plot beam FWHM scale: FWHM[Rsun]=beam_factor/f[Hz]; D[km]≈32e6/beam_factor")
    parser.add_argument("--quiet", "-q", action="store_true", help="Less logging")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.grff_lib:
        los_grff.GRFF_LIB = args.grff_lib

    freqs_hz = [f * 1e6 for f in args.freqs_mhz]
    if len(freqs_hz) != 3:
        raise ValueError("This script expects exactly 3 frequencies for a 2x3 plot.")

    ray_maps = []
    los_maps = []
    x_coords = None
    y_coords = None

    for freq_hz in freqs_hz:
        freq_mhz = int(round(freq_hz / 1e6))
        ray_out = out_dir / f"raytrace_{freq_mhz}MHz.npz"
        los_base = out_dir / f"LOS_emission_{freq_mhz}MHz"
        los_emission_npz = Path(str(los_base) + ".npz")

        if args.plot_only:
            if not ray_out.exists():
                raise FileNotFoundError(f"Missing ray-tracing map npz for --plot-only: {ray_out}")
            if not los_emission_npz.exists():
                raise FileNotFoundError(f"Missing LOS emission map npz for --plot-only: {los_emission_npz}")
            ray_res = np.load(ray_out)
            los_res = np.load(los_emission_npz)
            ray_map = np.nan_to_num(ray_res["emission_cube"][:, :, 0], nan=0.0, posinf=0.0, neginf=0.0)
            los_map = np.nan_to_num(los_res["emission_cube"][:, :, 0], nan=0.0, posinf=0.0, neginf=0.0)
            if x_coords is None:
                x_coords = np.array(ray_res["x_coords"], dtype=float)
                y_coords = np.array(ray_res["y_coords"], dtype=float)
        else:
            p = frequency_scaled_params(
                freq_hz=freq_hz,
                ref_freq_hz=args.ref_freq_mhz * 1e6,
                base_dt=args.base_dt,
                base_n_steps=args.base_n_steps,
                base_record_stride=args.base_record_stride,
                base_dz0=args.base_dz0,
                base_nz=args.base_n_z,
                scaling_exp=args.scaling_exp,
                min_n_steps=args.min_n_steps,
                min_nz=args.min_n_z,
            )

            if not args.quiet:
                print(
                    f"[{freq_mhz} MHz] ray(dt={p['dt']:.4g}, n_steps={p['n_steps']}, stride={p['record_stride']}), "
                    f"LOS(dz0={p['dz0']:.4g}, N_z={p['n_z']})"
                )

            ray_res = run_ray_tracing_emission(
                model_path=args.model_path,
                N_pix=args.N_pix,
                X_fov=args.X_FOV,
                freq_hz=freq_hz,
                grid_n=args.grid_n,
                grid_extent=args.grid_extent,
                z_observer=args.z_observer,
                dt=p["dt"],
                n_steps=p["n_steps"],
                record_stride=p["record_stride"],
                n_workers=args.workers,
                s_input_on=False,
                out_path=str(ray_out),
                grff_lib=args.grff_lib,
                Nfreq=1,
                freq0=freq_hz,
                freq_log_step=0.0,
                save_plots=False,
                verbose=not args.quiet,
                device=args.device,
                fallback_to_cpu=not args.no_fallback,
                raytrace_device=args.raytrace_device,
                grff_backend="get_mw",
                beam_fwhm_rsun=args.beam_fwhm_rsun,
                beam_diameter_m=args.beam_diameter_m,
                phi0_offset=args.phi0_offset,
            )
            ray_map = np.nan_to_num(ray_res["emission_cube"][:, :, 0], nan=0.0, posinf=0.0, neginf=0.0)

            los_data_path = out_dir / f"LOS_data_{freq_mhz}MHz.npz"
            resample_MAS(
                model_path=args.model_path,
                N_pix=args.N_pix,
                X_range=[-args.X_FOV, args.X_FOV],
                Y_range=[-args.X_FOV, args.X_FOV],
                N_z=p["n_z"],
                dz0=p["dz0"],
                variable_spacing_z=True,
                z_range=None,
                out_path=str(los_data_path),
                save_plots=False,
                verbose=not args.quiet,
                phi0_offset=args.phi0_offset,
            )
            los_res = los_grff.SyntheticFF(
                fname_input=str(los_data_path),
                freq0=freq_hz,
                Nfreq=1,
                freq_log_step=0.0,
                fname_output=str(los_base),
            )
            los_map = np.nan_to_num(los_res["emission_cube"][:, :, 0], nan=0.0, posinf=0.0, neginf=0.0)

            if x_coords is None:
                x_coords = np.array(ray_res["x_coords"], dtype=float)
                y_coords = np.array(ray_res["y_coords"], dtype=float)

        ray_maps.append(ray_map)
        los_maps.append(los_map)

    fig_path = out_dir / "compare_LOS_raytracing_2x3.pdf"
    _plot_compare_2x3(
        ray_maps,
        los_maps,
        freqs_hz,
        x_coords,
        y_coords,
        fig_path,
        plot_consider_beam=args.plot_consider_beam,
        beam_factor=args.beam_factor,
    )
    print(f"Saved comparison figure: {fig_path}")


if __name__ == "__main__":
    main()
