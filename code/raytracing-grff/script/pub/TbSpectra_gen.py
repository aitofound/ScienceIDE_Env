#!/usr/bin/env python
"""
Generate ray-tracing T_b maps over 30 log-spaced frequencies from 30 to 800 MHz.

Outputs one image per frequency in script/pub/mfs/ by default.
No LOS computation is performed.
"""

import argparse
from pathlib import Path
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from raytracingGRFF.coords import PHI0_EARTH_CORONA2298
from script.resample_with_ray_tracing import run_ray_tracing_emission

R_SUN_M = 6.957e8


def _lowband_params(freq_hz):
    # Matches the low-band compare script scaling setup.
    ref_freq_hz = 100e6
    base_dt = 6e-3
    base_n_steps = 4000
    base_record_stride = 5
    scaling_exp = 0.5
    min_n_steps = 1200
    scale = (ref_freq_hz / freq_hz) ** scaling_exp
    return {
        "grid_n": 256,
        "grid_extent": 4,
        "z_observer": 4,
        "x_fov": 2.8,
        "dt": base_dt * scale,
        "n_steps": max(min_n_steps, int(round(base_n_steps / max(scale, 1e-12)))),
        "record_stride": max(1, int(round(base_record_stride * scale))),
    }


def _interp_log_freq_params(freq_hz, f0_hz, p0, f1_hz, p1):
    t = (np.log(freq_hz) - np.log(f0_hz)) / (np.log(f1_hz) - np.log(f0_hz))
    t = float(np.clip(t, 0.0, 1.0))
    out = {}
    for k in p0.keys():
        out[k] = (1.0 - t) * p0[k] + t * p1[k]
    return out


def _highband_params(freq_hz):
    # Matches high-band presets used in compare_LOS_raytracing_highband.py.
    anchors = {
        280e6: {"grid_n": 400, "grid_extent": 1.75, "z_observer": 1.75, "x_fov": 1.44, "dt": 1.0e-3, "n_steps": 4500, "record_stride": 10},
        550e6: {"grid_n": 440, "grid_extent": 1.45, "z_observer": 1.45, "x_fov": 1.44, "dt": 0.8e-3, "n_steps": 7500, "record_stride": 5},
        800e6: {"grid_n": 520, "grid_extent": 1.45, "z_observer": 1.44, "x_fov": 1.44, "dt": 0.4e-3, "n_steps": 12000, "record_stride": 5},
    }
    if freq_hz <= 550e6:
        p = _interp_log_freq_params(freq_hz, 280e6, anchors[280e6], 550e6, anchors[550e6])
    else:
        p = _interp_log_freq_params(freq_hz, 550e6, anchors[550e6], 800e6, anchors[800e6])
    p["grid_n"] = int(round(p["grid_n"]))
    p["n_steps"] = int(round(p["n_steps"]))
    p["record_stride"] = int(round(p["record_stride"]))
    return p


def select_params(freq_hz):
    # Low band <=150 MHz: low-band scaling.
    # High band >=280 MHz: high-band interpolation.
    # 150-280 MHz: smooth transition in log frequency.
    if freq_hz <= 150e6:
        return _lowband_params(freq_hz)
    if freq_hz >= 280e6:
        return _highband_params(freq_hz)

    p_lo = _lowband_params(150e6)
    p_hi = _highband_params(280e6)
    p = _interp_log_freq_params(freq_hz, 150e6, p_lo, 280e6, p_hi)
    p["grid_n"] = int(round(p["grid_n"]))
    p["n_steps"] = int(round(p["n_steps"]))
    p["record_stride"] = int(round(p["record_stride"]))
    return p


def save_map_png(tb_map, x_coords_m, y_coords_m, freq_hz, out_png):
    tb = np.nan_to_num(np.array(tb_map, dtype=float), nan=0.0, posinf=0.0, neginf=0.0)
    vmax = np.nanmax(tb) if np.any(np.isfinite(tb)) else 1.0
    if vmax <= 0:
        vmax = 1.0
    extent = [
        x_coords_m[0] / R_SUN_M, x_coords_m[-1] / R_SUN_M,
        y_coords_m[0] / R_SUN_M, y_coords_m[-1] / R_SUN_M,
    ]
    fig, ax = plt.subplots(figsize=(6.0, 5.2), constrained_layout=True)
    ax.imshow(tb, origin="lower", extent=extent, aspect="equal", cmap="hot", vmin=0.0, vmax=vmax)
    ax.add_patch(plt.Circle((0.0, 0.0), 1.0, edgecolor="white", facecolor="none", linewidth=1.2, linestyle=":"))
    ax.set_xlabel(r"x ($R_\odot$)")
    ax.set_ylabel(r"y ($R_\odot$)")
    ax.set_title(f"Ray tracing $T_b$ at {freq_hz/1e6:.3f} MHz")
    fig.savefig(out_png, dpi=160, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Generate ray-tracing T_b spectra maps (30-800 MHz).")
    parser.add_argument("--model-path", "-m", default="./corona2298", help="MAS model path")
    parser.add_argument("--out-dir", default="script/pub/mfs", help="Output directory")
    parser.add_argument("--N-pix", "-n", type=int, default=128, help="Image size")
    parser.add_argument("--fmin-mhz", type=float, default=30.0, help="Minimum frequency (MHz)")
    parser.add_argument("--fmax-mhz", type=float, default=800.0, help="Maximum frequency (MHz)")
    parser.add_argument("--n-freq", type=int, default=30, help="Number of log-spaced frequencies")
    parser.add_argument("--start-from-idx", type=int, default=0,
                        help="Start processing from this index (inclusive)")
    parser.add_argument("--phi0-offset", type=float, default=PHI0_EARTH_CORONA2298, help="Carrington longitude at disk center (deg)")
    parser.add_argument("--device", default="cuda", choices=["cpu", "cuda"], help="Sampling device")
    parser.add_argument("--raytrace-device", default="cuda", choices=["cpu", "cuda"], help="Raytrace device")
    parser.add_argument("--workers", type=int, default=1, help="CPU raytrace workers")
    parser.add_argument("--no-fallback", action="store_true", help="Disable CUDA->CPU fallback")
    parser.add_argument("--grff-lib", default=None, help="Optional path to GRFF_DEM_Transfer.so")
    parser.add_argument("--plot-only", action="store_true", help="Only plot from existing npz files")
    parser.add_argument("--quiet", "-q", action="store_true", help="Less logging")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    freqs_mhz = np.logspace(np.log10(args.fmin_mhz), np.log10(args.fmax_mhz), args.n_freq)
    freqs_hz = freqs_mhz * 1e6
    if args.start_from_idx < 0 or args.start_from_idx >= len(freqs_hz):
        raise ValueError(f"--start-from-idx must be in [0, {len(freqs_hz)-1}]")

    manifest_rows = []
    for i, freq_hz in enumerate(freqs_hz):
        if i < args.start_from_idx:
            continue
        p = select_params(float(freq_hz))
        tag = f"{i:02d}_{freq_hz/1e6:08.3f}MHz"
        npz_path = out_dir / f"raytrace_{tag}.npz"
        png_path = out_dir / f"Tb_map_{tag}.png"

        if not args.plot_only:
            if not args.quiet:
                print(
                    f"[{i+1:02d}/{len(freqs_hz)}] {freq_hz/1e6:8.3f} MHz | "
                    f"grid_n={p['grid_n']} X_FOV={p['x_fov']:.3f} z_obs={p['z_observer']:.3f} "
                    f"dt={p['dt']:.3g} n_steps={p['n_steps']} stride={p['record_stride']}"
                )

            run_ray_tracing_emission(
                model_path=args.model_path,
                N_pix=args.N_pix,
                X_fov=float(p["x_fov"]),
                freq_hz=float(freq_hz),
                grid_n=int(p["grid_n"]),
                grid_extent=float(p["grid_extent"]),
                z_observer=float(p["z_observer"]),
                dt=float(p["dt"]),
                n_steps=int(p["n_steps"]),
                record_stride=int(p["record_stride"]),
                n_workers=args.workers,
                s_input_on=False,
                out_path=str(npz_path),
                grff_lib=args.grff_lib,
                Nfreq=1,
                freq0=float(freq_hz),
                freq_log_step=0.0,
                save_plots=False,
                verbose=not args.quiet,
                device=args.device,
                fallback_to_cpu=not args.no_fallback,
                raytrace_device=args.raytrace_device,
                grff_backend="get_mw",
                phi0_offset=args.phi0_offset,
            )

        if not npz_path.exists():
            raise FileNotFoundError(f"Missing expected npz file: {npz_path}")
        data = np.load(npz_path)
        tb_map = data["emission_cube"][:, :, 0]
        x_coords = data["x_coords"]
        y_coords = data["y_coords"]
        save_map_png(tb_map, x_coords, y_coords, float(freq_hz), png_path)

        manifest_rows.append((i, float(freq_hz), str(npz_path), str(png_path)))

    manifest = out_dir / "TbSpectra_manifest.txt"
    with open(manifest, "w", encoding="utf-8") as f:
        f.write("# idx freq_hz npz_path png_path\n")
        for row in manifest_rows:
            f.write(f"{row[0]:02d} {row[1]:.6e} {row[2]} {row[3]}\n")
    print(f"Saved {len(freqs_hz)} maps to {out_dir}")
    print(f"Manifest: {manifest}")


if __name__ == "__main__":
    main()
