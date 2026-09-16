#!/usr/bin/env python
"""
Compare emission maps with and without --s-input-on.

Runs resample_with_ray_tracing.py twice (same args, once with --s-input-on,
once without), loads both npz outputs, and reports/plots the difference.
"""

import argparse
import subprocess
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Script dir: run resample_with_ray_tracing.py from same dir
SCRIPT_DIR = Path(__file__).resolve().parent
RESAMPLE_SCRIPT = SCRIPT_DIR / 'resample_with_ray_tracing.py'


def run_resample(extra_args, out_path, quiet=True):
    """Run resample_with_ray_tracing.py with given extra args and output path."""
    cmd = [
        sys.executable,
        str(RESAMPLE_SCRIPT),
        '-o', str(out_path),
        '--no-plots',
    ] + extra_args
    if quiet:
        cmd.append('-q')
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    # Run from repo root so ./corona and raytracingGRFF resolve
    r = subprocess.run(cmd, cwd=SCRIPT_DIR.parent)
    return r.returncode == 0


def load_map(npz_path, key='emission_cube'):
    """Load first frequency slice from npz as (N_pix, N_pix)."""
    data = np.load(npz_path)
    cube = data[key]
    if cube.ndim == 3:
        return cube[:, :, 0]
    return cube


def main():
    parser = argparse.ArgumentParser(
        description='Compare T_b map with vs without --s-input-on.')
    parser.add_argument('--model-path', '-m', type=str, default='./corona',
                        help='MAS model path (default: ./corona)')
    parser.add_argument('--N-pix', '-n', type=int, default=100,
                        help='Image size (default: 100)')
    parser.add_argument('--X-FOV', '-f', type=float, default=2.25,
                        help='Half FOV in R_sun (default: 2.25)')
    parser.add_argument('--freq', type=float, default=60e6,
                        help='Frequency in Hz (default: 60e6)')
    parser.add_argument('--grid-n', type=int, default=150,
                        help='3D grid points (default: 150)')
    parser.add_argument('--raytrace-device', type=str, default='cuda', choices=['cpu', 'cuda'],
                        help='Ray tracing device (default: cuda)')
    parser.add_argument('--grff-backend', type=str, default='fastgrff', choices=['get_mw', 'fastgrff'],
                        help='GRFF backend (default: fastgrff)')
    parser.add_argument('--device', type=str, default='cuda', choices=['cpu', 'cuda'],
                        help='LOS/GRFF device (default: cuda)')
    parser.add_argument('--beam-fwhm-rsun', type=float, default=None,
                        help='Gaussian beam FWHM in R_sun')
    parser.add_argument('--beam-diameter-m', type=float, default=None,
                        help='Telescope D (m) for λ/D beam')
    parser.add_argument('--no-fallback', action='store_true',
                        help='Do not fall back to CPU')
    parser.add_argument('--skip-run', action='store_true',
                        help='Skip running; only load existing npz and compare/plot')
    parser.add_argument('--out-dir', '-o', type=str, default=None,
                        help='Directory for run outputs (default: script dir)')
    parser.add_argument('--no-plot', action='store_true',
                        help='Do not save comparison plot')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Less output from subprocess runs')
    args = parser.parse_args()

    out_dir = Path(args.out_dir) if args.out_dir else SCRIPT_DIR
    out_dir = out_dir.resolve()
    path_s_on = out_dir / 'ray_tracing_compare_s_on.npz'
    path_s_off = out_dir / 'ray_tracing_compare_s_off.npz'

    base_extra = [
        '-m', args.model_path,
        '-n', str(args.N_pix),
        '-f', str(args.X_FOV),
        '--freq', str(args.freq),
        '--grid-n', str(args.grid_n),
        '--raytrace-device', args.raytrace_device,
        '--grff-backend', args.grff_backend,
        '--device', args.device,
    ]
    if args.beam_fwhm_rsun is not None:
        base_extra += ['--beam-fwhm-rsun', str(args.beam_fwhm_rsun)]
    if args.beam_diameter_m is not None:
        base_extra += ['--beam-diameter-m', str(args.beam_diameter_m)]
    if args.no_fallback:
        base_extra.append('--no-fallback')

    if not args.skip_run:
        print('Run 1: with --s-input-on ...')
        ok1 = run_resample(base_extra + ['--s-input-on'], path_s_on, quiet=args.quiet)
        if not ok1:
            print('Run 1 failed.', file=sys.stderr)
            sys.exit(1)
        print('Run 2: without --s-input-on ...')
        ok2 = run_resample(base_extra, path_s_off, quiet=args.quiet)
        if not ok2:
            print('Run 2 failed.', file=sys.stderr)
            sys.exit(1)
    else:
        if not path_s_on.is_file() or not path_s_off.is_file():
            print('--skip-run set but missing npz files.', file=sys.stderr)
            sys.exit(1)

    # Load and compare
    T_s_on = load_map(path_s_on)
    T_s_off = load_map(path_s_off)
    if T_s_on.shape != T_s_off.shape:
        print('Shape mismatch:', T_s_on.shape, T_s_off.shape, file=sys.stderr)
        sys.exit(1)

    valid = np.isfinite(T_s_on) & np.isfinite(T_s_off) & (T_s_on > 0) & (T_s_off > 0)
    diff = T_s_on - T_s_off
    with np.errstate(divide='ignore', invalid='ignore'):
        ratio = np.where(T_s_off > 0, T_s_on / T_s_off, np.nan)

    print('\n--- T_b with --s-input-on ---')
    print(f'  min={np.nanmin(T_s_on):.4e}, max={np.nanmax(T_s_on):.4e}, mean(valid)={np.nanmean(T_s_on[valid]):.4e} K')
    print('--- T_b without --s-input-on ---')
    print(f'  min={np.nanmin(T_s_off):.4e}, max={np.nanmax(T_s_off):.4e}, mean(valid)={np.nanmean(T_s_off[valid]):.4e} K')
    print('--- Difference (S_on - S_off) ---')
    print(f'  mean(diff)={np.nanmean(diff[valid]):.4e}, mean(|diff|)={np.nanmean(np.abs(diff[valid])):.4e}, max|diff|={np.nanmax(np.abs(diff[valid])):.4e} K')
    print('--- Ratio (S_on / S_off) on valid pixels ---')
    r_valid = ratio[valid]
    print(f'  min={np.nanmin(r_valid):.4f}, max={np.nanmax(r_valid):.4f}, mean={np.nanmean(r_valid):.4f}')

    if not args.no_plot:
        fig, axes = plt.subplots(1, 3, figsize=(12, 4))
        extent = [-args.X_FOV, args.X_FOV, -args.X_FOV, args.X_FOV]
        for ax, arr, title in [
            (axes[0], T_s_on, r'$T_b$ with S (--s-input-on)'),
            (axes[1], T_s_off, r'$T_b$ without S'),
            (axes[2], diff, r'Difference (S_on $-$ S_off)'),
        ]:
            plot_arr = arr.copy()
            if 'Difference' in title:
                plot_arr[~valid] = np.nan
                v = np.nanmax(np.abs(plot_arr))
                v = max(v, 1e-10)
                im = ax.imshow(plot_arr, origin='lower', extent=extent, aspect='equal',
                               cmap='RdBu_r', vmin=-v, vmax=v, interpolation='bilinear')
            else:
                plot_arr[plot_arr <= 0] = np.nan
                im = ax.imshow(plot_arr, origin='lower', extent=extent, aspect='equal',
                               cmap='hot', interpolation='bilinear')
            ax.set_xlabel('x (R_sun)')
            ax.set_ylabel('y (R_sun)')
            ax.set_title(title)
            plt.colorbar(im, ax=ax, label='K' if 'Difference' not in title else r'$\Delta$ K')
        plt.tight_layout()
        plot_path = out_dir / 'compare_s_input.png'
        plt.savefig(plot_path, dpi=150, bbox_inches='tight')
        plt.close()
        print(f'\nComparison plot saved to {plot_path}')


if __name__ == '__main__':
    main()
