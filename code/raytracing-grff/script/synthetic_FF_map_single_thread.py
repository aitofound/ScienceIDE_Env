#!/usr/bin/env python
"""
Synthetic free-free emission calculation using GRFF.

Calls GRFF_DEM_Transfer.so along each LOS using the resampled MAS data.
Supports multi-frequency: outputs a cube (N_pix, N_pix, N_freq).
Plots use the first frequency only. Uses T_e, N_e, B, and ds from the LOS sampling.
"""

import argparse
import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import sys
from pathlib import Path

# Constants
R_sun = 6.957e10  # cm

_grff_rs = Path(__file__).resolve().parents[1]
if str(_grff_rs) not in sys.path:
    sys.path.insert(0, str(_grff_rs))
from raytracingGRFF.grff_ctypes import default_grff_lib_path, initGET_MW
from raytracingGRFF.grff_parms import (
    GRFF_PARMS_EXT_SIZE,
    fill_grff_parms_ext_column,
    rl_stokes_to_tb_vi,
    vi_plot_vmax,
)

GRFF_LIB = str(default_grff_lib_path())


def _save_center_pixel_plots(Ne_LOS, Te_LOS, B_LOS, ds_LOS, N_pix, R_sun, fname_output):
    """Plot Ne, Te, B, and ds along LOS for the center pixel (inspection)."""
    i_c = N_pix // 2
    j_c = N_pix // 2
    R_sun_cm = R_sun
    ne_c = Ne_LOS[i_c, j_c, :].copy()
    te_c = Te_LOS[i_c, j_c, :].copy()
    b_c = B_LOS[i_c, j_c, :].copy()
    ds_c = ds_LOS[i_c, j_c, :].copy()
    valid = np.isfinite(ne_c) & np.isfinite(te_c) & np.isfinite(b_c)
    if not np.any(valid):
        print("Center pixel has no valid LOS points; skipping inspection plot.")
        return
    ne_c = np.where(valid, ne_c, np.nan)
    te_c = np.where(valid, te_c, np.nan)
    b_c = np.where(valid, b_c, np.nan)
    ds_c = np.where(valid, ds_c, np.nan)
    N_z = len(ds_c)
    ds_safe = np.where(np.isfinite(ds_c), ds_c, 0.0)
    dist_cm = np.zeros(N_z, dtype=float)
    dist_cm[0] = 0.0
    if N_z > 1:
        dist_cm[1:] = np.cumsum(ds_safe[:-1])
    dist_Rsun = dist_cm / R_sun_cm

    fig, axes = plt.subplots(2, 2, figsize=(10, 8))
    axes[0, 0].plot(dist_Rsun, ne_c, 'b-', linewidth=1.5)
    axes[0, 0].set_xlabel('Distance along LOS (R_sun)')
    axes[0, 0].set_ylabel('N_e (cm$^{-3}$)')
    axes[0, 0].set_yscale('log')
    axes[0, 0].set_title('Center pixel: N_e along LOS')
    axes[0, 0].grid(True, alpha=0.3)

    axes[0, 1].plot(dist_Rsun, te_c, 'r-', linewidth=1.5)
    axes[0, 1].set_xlabel('Distance along LOS (R_sun)')
    axes[0, 1].set_ylabel('T_e (K)')
    axes[0, 1].set_yscale('log')
    axes[0, 1].set_title('Center pixel: T_e along LOS')
    axes[0, 1].grid(True, alpha=0.3)

    axes[1, 0].plot(dist_Rsun, b_c, 'green', linewidth=1.5)
    axes[1, 0].set_xlabel('Distance along LOS (R_sun)')
    axes[1, 0].set_ylabel('|B| (G)')
    axes[1, 0].set_yscale('log')
    axes[1, 0].set_title('Center pixel: |B| along LOS')
    axes[1, 0].grid(True, alpha=0.3)

    axes[1, 1].plot(dist_Rsun, ds_c / R_sun_cm, 'k-', linewidth=1.5)
    axes[1, 1].set_xlabel('Distance along LOS (R_sun)')
    axes[1, 1].set_ylabel('ds (R_sun)')
    axes[1, 1].set_title('Center pixel: segment length ds along LOS')
    axes[1, 1].grid(True, alpha=0.3)

    plt.tight_layout()
    plot_path = fname_output + '_center_pixel.png'
    plt.savefig(plot_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Center-pixel inspection plot saved to {plot_path}")


def SyntheticFF(
    fname_input,
    freq0,
    Nfreq,
    freq_log_step,
    fname_output,
    do_inspection_plot=False,
    *,
    grff_dist_e=0.0,
    grff_kappa=0.0,
):
    """
    Compute synthetic free-free brightness temperature and V/I from LOS data using GRFF.

    Parameters
    ----------
    fname_input : str
        Path to LOS npz file (from resampling_MAS.py).
    freq0 : float
        Start frequency in Hz.
    Nfreq : int
        Number of frequencies.
    freq_log_step : float
        log10 step between frequencies.
    fname_output : str
        Base path for output files (no extension). Writes fname_output.npz,
        fname_output.png, fname_output_Tb_VI.png, fname_output_log.png.
    do_inspection_plot : bool
        If True, save center-pixel LOS sampling plot (fname_output_center_pixel.png).

    Returns
    -------
    dict
        emission_cube, emission_polVI_cube, frequencies_Hz, x_coords, y_coords.
    """
    print(f"Loading GRFF library from: {GRFF_LIB}")
    GET_MW = initGET_MW(GRFF_LIB)

    data = np.load(fname_input)
    Ne_LOS = data['Ne_LOS']
    Te_LOS = data['Te_LOS']
    B_LOS = data['B_LOS']
    ds_LOS = data['ds_LOS']
    x_coords = data['x_coords']
    y_coords = data['y_coords']

    N_pix = Ne_LOS.shape[0]
    N_z = Ne_LOS.shape[2]
    Nf = Nfreq
    frequencies_Hz = freq0 * (10.0 ** (freq_log_step * np.arange(Nf)))

    Lparms = np.zeros(5, dtype='int32')
    Lparms[0] = N_z
    Lparms[1] = Nf
    Rparms = np.zeros(3, dtype='double')
    pixel_size_Rsun = (x_coords[1] - x_coords[0]) / (R_sun * 1e-2)
    pixel_size_cm = pixel_size_Rsun * R_sun
    Rparms[0] = pixel_size_cm * pixel_size_cm
    Rparms[1] = freq0
    Rparms[2] = freq_log_step

    emission_cube = np.zeros((N_pix, N_pix, Nf), dtype='double')
    emission_polVI_cube = np.zeros((N_pix, N_pix, Nf), dtype='double')

    print(f"Image size: {N_pix}x{N_pix}, LOS points: {N_z}")
    print(f"Frequencies: {frequencies_Hz/1e6} MHz (Nf={Nf})")

    if do_inspection_plot:
        _save_center_pixel_plots(Ne_LOS, Te_LOS, B_LOS, ds_LOS, N_pix, R_sun, fname_output)

    for i in range(N_pix):
        if (i + 1) % 50 == 0:
            print(f"Processing row {i+1}/{N_pix}...")

        for j in range(N_pix):
            ne_los = Ne_LOS[i, j, :].copy()
            te_los = Te_LOS[i, j, :].copy()
            b_los = B_LOS[i, j, :].copy()
            ds_los = ds_LOS[i, j, :].copy()
            valid_mask = ~(np.isnan(ne_los) | np.isnan(te_los) | np.isnan(b_los))
            if not np.any(valid_mask):
                emission_cube[i, j, :] = 0.0
                continue
            ne_valid = ne_los[valid_mask]
            te_valid = te_los[valid_mask]
            b_valid = b_los[valid_mask]
            ds_valid = ds_los[valid_mask]
            N_valid = len(ne_valid)
            if N_valid == 0:
                emission_cube[i, j, :] = 0.0
                continue
            Parms = np.zeros((GRFF_PARMS_EXT_SIZE, N_valid), dtype='double', order='F')
            for k in range(N_valid):
                fill_grff_parms_ext_column(
                    Parms,
                    k,
                    ds_valid[k],
                    te_valid[k],
                    ne_valid[k],
                    b_valid[k],
                    s_cm2=0.0,
                    dist_e=grff_dist_e,
                    kappa=grff_kappa,
                )
            Lparms_local = Lparms.copy()
            Lparms_local[0] = N_valid
            dummy_T = np.array(0, dtype='double')
            dummy_DEM = np.array(0, dtype='double')
            dummy_DDM = np.array(0, dtype='double')
            RL = np.zeros((7, Nf), dtype='double', order='F')
            try:
                res = GET_MW(Lparms_local, Rparms, Parms, dummy_T, dummy_DEM, dummy_DDM, RL)
                if res != 0:
                    emission_cube[i, j, :] = 0.0
                    emission_polVI_cube[i, j, :] = 0.0
                    continue
                for ifreq in range(Nf):
                    nu_GHz = RL[0, ifreq]
                    nu_Hz = frequencies_Hz[ifreq] if nu_GHz <= 0 else nu_GHz * 1e9
                    tb_k, vi = rl_stokes_to_tb_vi(RL, ifreq, nu_Hz, Rparms[0])
                    emission_cube[i, j, ifreq] = tb_k
                    emission_polVI_cube[i, j, ifreq] = vi
            except Exception as e:
                print(f"Error processing pixel ({i}, {j}): {e}")
                emission_cube[i, j, :] = 0.0
                emission_polVI_cube[i, j, :] = 0.0

    print("\nBrightness temperature calculation complete!")
    frequency_first = frequencies_Hz[0]
    emission_map_first = emission_cube[:, :, 0]
    emission_polVI_map_first = emission_polVI_cube[:, :, 0]
    center_size = 16
    center_start = N_pix // 2 - center_size // 2
    center_end = N_pix // 2 + center_size // 2
    center_region = emission_map_first[center_start:center_end, center_start:center_end]
    valid_center = center_region[center_region > 0]
    avg_center_str = f"{np.mean(valid_center):.2e}" if len(valid_center) > 0 else "N/A"
    if len(valid_center) > 0:
        print(f"\nAverage brightness temperature (center {center_size}x{center_size}, first freq): {np.mean(valid_center):.2e} K")

    print("\nSaving brightness temperature cube...")
    np.savez_compressed(fname_output + '.npz',
                        emission_cube=emission_cube, emission_polVI_cube=emission_polVI_cube,
                        frequencies_Hz=frequencies_Hz, x_coords=x_coords, y_coords=y_coords)
    print(f"Brightness temperature cube saved to {fname_output}.npz (shape {N_pix} x {N_pix} x {Nf})")

    print("\nPlotting brightness temperature map (first frequency)...")
    fig, ax = plt.subplots(figsize=(6, 4.8))
    x_range = [x_coords[0] / (R_sun * 1e-2), x_coords[-1] / (R_sun * 1e-2)]
    y_range = [y_coords[0] / (R_sun * 1e-2), y_coords[-1] / (R_sun * 1e-2)]
    emission_plot = emission_map_first.copy()
    emission_plot[emission_plot == 0] = np.nan
    im = ax.imshow(emission_plot, origin='lower', extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                   aspect='equal', cmap='hot', interpolation='bilinear')
    ax.set_xlabel('x (R_sun)')
    ax.set_ylabel('y (R_sun)')
    ax.set_title(f'synthetic $T_b$ map at {frequency_first/1e9:.3f} GHz')
    plt.colorbar(im, ax=ax, label='T_b (K)')
    ax.text(0.97, 0.97, f'Center $T_b$: {avg_center_str}', transform=ax.transAxes, fontsize=12,
            verticalalignment='top', horizontalalignment='right',
            bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    plt.tight_layout()
    plt.savefig(fname_output + '.png', dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Emission map saved to {fname_output}.png")

    fig_tb_vi, (ax_tb, ax_vi) = plt.subplots(1, 2, figsize=(12, 4.2))
    im_tb = ax_tb.imshow(emission_plot, origin='lower', extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                         aspect='equal', cmap='hot', interpolation='bilinear')
    ax_tb.set_xlabel('x (R_sun)')
    ax_tb.set_ylabel('y (R_sun)')
    ax_tb.set_title(f'$T_b$ at {frequency_first/1e9:.3f} GHz')
    plt.colorbar(im_tb, ax=ax_tb, label='T_b (K)')
    pol_vi_plot = emission_polVI_map_first.copy()
    pol_vi_plot[emission_map_first == 0] = np.nan
    vmax_vi = vi_plot_vmax(pol_vi_plot, emission_map_first)
    im_vi = ax_vi.imshow(pol_vi_plot, origin='lower', extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                         aspect='equal', cmap='RdBu_r', interpolation='bilinear', vmin=-vmax_vi, vmax=vmax_vi)
    ax_vi.set_xlabel('x (R_sun)')
    ax_vi.set_ylabel('y (R_sun)')
    ax_vi.set_title(f'V/I at {frequency_first/1e9:.3f} GHz')
    plt.colorbar(im_vi, ax=ax_vi, label='V/I')
    plt.tight_layout()
    plt.savefig(fname_output + '_Tb_VI.png', dpi=150, bbox_inches='tight')
    plt.close(fig_tb_vi)
    print(f"T_b and V/I side-by-side plot saved to {fname_output}_Tb_VI.png")

    fig2, ax2 = plt.subplots(figsize=(10, 10))
    im2 = ax2.imshow(emission_plot, origin='lower', extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                     aspect='equal', cmap='hot', interpolation='bilinear',
                     norm=mcolors.LogNorm(vmin=np.nanmin(emission_plot[emission_plot > 0]),
                                          vmax=np.nanmax(emission_plot)))
    ax2.set_xlabel('x (R_sun)')
    ax2.set_ylabel('y (R_sun)')
    ax2.set_title(f'synthetic $T_b$ map at {frequency_first/1e9:.3f} GHz (Log Scale)')
    plt.colorbar(im2, ax=ax2, label='T_b (K)')
    ax2.text(0.97, 0.97, f'Center $T_b$: {avg_center_str}', transform=ax2.transAxes, fontsize=12,
             verticalalignment='top', horizontalalignment='right',
             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    plt.tight_layout()
    plt.savefig(fname_output + '_log.png', dpi=150, bbox_inches='tight')
    plt.close(fig2)
    print(f"Log-scale brightness temperature map saved to {fname_output}_log.png")
    print("\nSynthetic brightness temperature calculation complete!")

    return {
        'emission_cube': emission_cube,
        'emission_polVI_cube': emission_polVI_cube,
        'frequencies_Hz': frequencies_Hz,
        'x_coords': x_coords,
        'y_coords': y_coords,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Synthetic free-free emission via GRFF.')
    parser.add_argument('--input', '-i', type=str, default='LOS_data.npz',
                        help='Path to LOS npz file (default: LOS_data.npz)')
    parser.add_argument('--output', '-o', type=str, default='emission_map',
                        help='Base path for output files, no extension (default: emission_map)')
    parser.add_argument('--freq0', '-f', type=float, default=450e6,
                        help='Start frequency in Hz (default: 450e6)')
    parser.add_argument('--Nfreq', '-n', type=int, default=4,
                        help='Number of frequencies (default: 4)')
    parser.add_argument('--freq-log-step', '-s', type=float, default=0.1,
                        help='log10 step between frequencies (default: 0.1)')
    parser.add_argument('--do-inspection-plot', action='store_true',
                        help='Save center-pixel LOS sampling plot (Ne, Te, B, ds along LOS)')
    parser.add_argument(
        '--grff-dist-e',
        type=float,
        default=0.0,
        help='GRFF Dist_E: 0 Maxwellian (default), 1 kappa, 2 n-distribution',
    )
    parser.add_argument(
        '--grff-kappa',
        type=float,
        default=0.0,
        help='GRFF kappa index when --grff-dist-e is 1 or 2',
    )
    args = parser.parse_args()
    SyntheticFF(
        args.input,
        args.freq0,
        args.Nfreq,
        args.freq_log_step,
        args.output,
        do_inspection_plot=args.do_inspection_plot,
        grff_dist_e=args.grff_dist_e,
        grff_kappa=args.grff_kappa,
    )
