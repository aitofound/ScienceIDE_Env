#!/usr/bin/env python
"""
High-band comparison of ray-tracing vs straight-LOS GRFF emission maps.

Default frequencies (MHz): 280, 550, 800
Output figure: 2x3 (top: ray tracing, bottom: LOS)
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
BEAM_FACTOR_DEFAULT = 32e6   # high band: ~1 km
GAUSSIAN_FWHM_TO_SIGMA = 2.355


def beam_fwhm_rsun(freq_hz, beam_factor):
    """Beam FWHM in R_sun from beam_factor and frequency (Hz)."""
    return beam_factor / freq_hz


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
    ray_x_coords_list,
    ray_y_coords_list,
    los_x_coords_list,
    los_y_coords_list,
    out_png,
    plot_consider_beam=True,
    beam_factor=BEAM_FACTOR_DEFAULT,
):
    fig, axes = plt.subplots(2, 3, figsize=(9.9, 6.6), constrained_layout=True)
    xlim_plot = (-1.4, 1.4)
    ylim_plot = (-1.4, 1.4)

    for ax in axes.ravel():
        ax.set_facecolor("white")

    panel_labels = [["(a1)", "(a2)", "(a3)"], ["(b1)", "(b2)", "(b3)"]]
    for col, freq_hz in enumerate(freqs_hz):
        freq_mhz = freq_hz / 1e6
        beam_fwhm = beam_fwhm_rsun(freq_hz, beam_factor)

        ray_map = np.array(ray_maps[col], dtype=float)
        los_map = np.array(los_maps[col], dtype=float)
        ray_x = np.array(ray_x_coords_list[col], dtype=float)
        ray_y = np.array(ray_y_coords_list[col], dtype=float)
        los_x = np.array(los_x_coords_list[col], dtype=float)
        los_y = np.array(los_y_coords_list[col], dtype=float)

        ray_extent = [ray_x[0] / R_SUN_M, ray_x[-1] / R_SUN_M, ray_y[0] / R_SUN_M, ray_y[-1] / R_SUN_M]
        los_extent = [los_x[0] / R_SUN_M, los_x[-1] / R_SUN_M, los_y[0] / R_SUN_M, los_y[-1] / R_SUN_M]
        
        ray_x_span = ray_extent[1] - ray_extent[0]
        ray_y_span = ray_extent[3] - ray_extent[2]
        los_x_span = los_extent[1] - los_extent[0]
        los_y_span = los_extent[3] - los_extent[2]

        print("freq:", freq_mhz, "ray_extent:", ray_extent, "los_extent:", los_extent)
        print("ray_x_span:", ray_x_span, "ray_y_span:", ray_y_span, "los_x_span:", los_x_span, "los_y_span:", los_y_span)

        if plot_consider_beam:
            ray_map = _apply_plot_beam(ray_map, freq_hz, beam_factor, ray_x, ray_y)
            los_map = _apply_plot_beam(los_map, freq_hz, beam_factor, los_x, los_y)

        ray_vmax = np.nanmax(ray_map) if np.any(np.isfinite(ray_map)) else 1.0
        los_vmax = np.nanmax(los_map) if np.any(np.isfinite(los_map)) else 1.0
        if ray_vmax <= 0:
            ray_vmax = 1.0
        if los_vmax <= 0:
            los_vmax = 1.0

        axes[0, col].imshow(
            ray_map, origin="lower", extent=ray_extent, aspect="equal", cmap=xrt_cmap,
            vmin=0.0, vmax=ray_vmax, interpolation="bilinear"
        )
        axes[0, col].set_title(f"Ray tracing {freq_mhz:.0f} MHz")
        axes[0, col].set_xlabel(r"x ($R_\odot$)")
        axes[0, col].set_ylabel(r"y ($R_\odot$)")
        axes[0, col].set_xlim(*xlim_plot)
        axes[0, col].set_ylim(*ylim_plot)
        axes[0, col].text(
            0.03, 0.95, panel_labels[0][col], transform=axes[0, col].transAxes,
            ha="left", va="top", color="white", fontsize=12, fontweight="bold"
        )
        axes[0, col].text(
            0.97, 0.05, (
                rf"$T_b^{{\max}} = "
                f"{f'{ray_vmax:.1e}'.split('e')[0]} \\times 10^{{{int(f'{ray_vmax:.1e}'.split('e')[1])}}}"
                rf"\,\mathrm{{K}}$"
            ),
            transform=axes[0, col].transAxes,
            ha="right", va="bottom", color="white", fontsize=12, fontweight="bold"
        )
        axes[0, col].add_patch(
            plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":")
        )
        if plot_consider_beam and beam_fwhm > 0:
            axes[0, col].add_patch(
                plt.Circle((ray_extent[0] + 0.12 * ray_x_span, ray_extent[2] + 0.12 * ray_y_span), beam_fwhm / 2.0,
                           edgecolor="white", facecolor="none", linewidth=1.8)
            )

        axes[1, col].imshow(
            los_map, origin="lower", extent=los_extent, aspect="equal", cmap=xrt_cmap,
            vmin=0.0, vmax=los_vmax, interpolation="bilinear"
        )
        axes[1, col].set_title(f"LOS {freq_mhz:.0f} MHz")
        axes[1, col].set_xlabel(r"x ($R_\odot$)")
        axes[1, col].set_ylabel(r"y ($R_\odot$)")
        axes[1, col].set_xlim(*xlim_plot)
        axes[1, col].set_ylim(*ylim_plot)
        axes[1, col].text(
            0.03, 0.95, panel_labels[1][col], transform=axes[1, col].transAxes,
            ha="left", va="top", color="white", fontsize=12, fontweight="bold"
        )
        axes[1, col].text(
            0.97, 0.05, (
                rf"$T_b^{{\max}} = "
                f"{f'{los_vmax:.1e}'.split('e')[0]} \\times 10^{{{int(f'{los_vmax:.1e}'.split('e')[1])}}}"
                rf"\,\mathrm{{K}}$"
            ),
            transform=axes[1, col].transAxes,
            ha="right", va="bottom", color="white", fontsize=12, fontweight="bold"
        )
        axes[1, col].add_patch(
            plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":")
        )
        if plot_consider_beam and beam_fwhm > 0:
            axes[1, col].add_patch(
                plt.Circle((los_extent[0] + 0.12 * los_x_span, los_extent[2] + 0.12 * los_y_span), beam_fwhm / 2.0,
                           edgecolor="white", facecolor="none", linewidth=1.8)
            )

    fig.savefig(out_png, dpi=180, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Compare LOS and ray-tracing emission maps in high band."
    )
    parser.add_argument("--model-path", "-m", default="./corona2298", help="MAS model path")
    parser.add_argument("--out-dir", default="script/pub/out_compare_los_ray_highband", help="Output directory")
    parser.add_argument("--freqs-mhz", type=float, nargs="+", default=[280.0, 550.0, 800.0],
                        help="Frequencies in MHz (default: 280 550 800)")
    parser.add_argument("--N-pix", "-n", type=int, default=128, help="Image size")
    parser.add_argument("--phi0-offset", type=float, default=PHI0_EARTH_CORONA2298,
                        help="Carrington longitude at disk center (deg)")
    parser.add_argument("--device", default="cuda", choices=["cpu", "cuda"], help="Sampling device")
    parser.add_argument("--raytrace-device", default="cuda", choices=["cpu", "cuda"], help="Raytrace device")
    parser.add_argument("--workers", type=int, default=1, help="CPU raytrace workers")
    parser.add_argument("--no-fallback", action="store_true", help="Disable CUDA->CPU fallback")
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

    # Per-frequency presets, matching provided high-band parsets for 280 and 800 MHz.
    # For 550 MHz, intermediate settings are used.
    presets = {
        280: {
            "grid_n": 280, "grid_extent": 1.75, "z_observer": 1.75, "x_fov": 1.44,
            "dt": 1e-3, "n_steps": 4500, "record_stride": 10,
            "los_n_z": 700, "los_dz0": 8e-5,
        },        
        550: {
            "grid_n": 420, "grid_extent": 1.45, "z_observer": 1.45, "x_fov": 1.44,
            "dt": 0.8e-3, "n_steps": 7500, "record_stride": 5,
            "los_n_z": 900, "los_dz0": 4e-5,
        },
        800: {
            "grid_n": 560, "grid_extent": 1.45, "z_observer": 1.44, "x_fov": 1.44,
            "dt": 0.4e-3, "n_steps": 12000, "record_stride": 5,
            "los_n_z": 1100, "los_dz0": 2e-5,
        },
    }

    ray_maps = []
    los_maps = []
    ray_x_coords_list = []
    ray_y_coords_list = []
    los_x_coords_list = []
    los_y_coords_list = []

    for freq_hz in freqs_hz:
        freq_mhz = int(round(freq_hz / 1e6))
        if freq_mhz not in presets:
            raise ValueError(f"No high-band preset configured for {freq_mhz} MHz")
        p = presets[freq_mhz]

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
            ray_x_coords = np.array(ray_res["x_coords"], dtype=float)
            ray_y_coords = np.array(ray_res["y_coords"], dtype=float)
            los_x_coords = np.array(los_res["x_coords"], dtype=float)
            los_y_coords = np.array(los_res["y_coords"], dtype=float)
        else:
            if not args.quiet:
                print(
                    f"[{freq_mhz} MHz] grid_n={p['grid_n']}, X_FOV={p['x_fov']}, z_obs={p['z_observer']}, "
                    f"dt={p['dt']}, n_steps={p['n_steps']}, stride={p['record_stride']}"
                )

            ray_res = run_ray_tracing_emission(
                model_path=args.model_path,
                N_pix=args.N_pix,
                X_fov=p["x_fov"],
                freq_hz=freq_hz,
                grid_n=p["grid_n"],
                grid_extent=p["grid_extent"],
                z_observer=p["z_observer"],
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
                phi0_offset=args.phi0_offset,
            )
            ray_map = np.nan_to_num(ray_res["emission_cube"][:, :, 0], nan=0.0, posinf=0.0, neginf=0.0)

            los_data_path = out_dir / f"LOS_data_{freq_mhz}MHz.npz"
            resample_MAS(
                model_path=args.model_path,
                N_pix=args.N_pix,
                X_range=[-p["x_fov"], p["x_fov"]],
                Y_range=[-p["x_fov"], p["x_fov"]],
                N_z=p["los_n_z"],
                dz0=p["los_dz0"],
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
            ray_x_coords = np.array(ray_res["x_coords"], dtype=float)
            ray_y_coords = np.array(ray_res["y_coords"], dtype=float)
            los_x_coords = np.array(los_res["x_coords"], dtype=float)
            los_y_coords = np.array(los_res["y_coords"], dtype=float)

        ray_maps.append(ray_map)
        los_maps.append(los_map)
        ray_x_coords_list.append(ray_x_coords)
        ray_y_coords_list.append(ray_y_coords)
        los_x_coords_list.append(los_x_coords)
        los_y_coords_list.append(los_y_coords)

    fig_path = out_dir / "compare_LOS_raytracing_highband.pdf"
    _plot_compare_2x3(
        ray_maps,
        los_maps,
        freqs_hz,
        ray_x_coords_list,
        ray_y_coords_list,
        los_x_coords_list,
        los_y_coords_list,
        fig_path,
        plot_consider_beam=args.plot_consider_beam,
        beam_factor=args.beam_factor,
    )
    print(f"Saved comparison figure: {fig_path}")


if __name__ == "__main__":
    main()
