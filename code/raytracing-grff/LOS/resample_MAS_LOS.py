#!/usr/bin/env python
"""
Resample MAS model along line-of-sight (LOS) for emission calculation.

HCC frame: +x solar west, +y projected solar north, +z toward the observer.
For a N_pix x N_pix image covering [-X-FOV, X-FOV] in x and y (R_sun),
each LOS at (x, y) samples along +z. See ``raytracingGRFF.coords``.
"""

import argparse
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import warnings
import re
from pathlib import Path

import astropy.units as u
from psipy.model import MASOutput
from psipy.io.mas import _read_mas
import xarray as xr
from psipy.model.variable import Variable

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))
from raytracingGRFF.coords import cart_to_mas_lonlat

# Suppress warnings
warnings.filterwarnings('ignore')

# ============================================================================
# CONSTANTS
# ============================================================================

R_sun_cm = 6.957e10  # cm
R_sun_m = 6.957e8    # meters
r_min = 0.9999999    # Minimum r in R_sun for valid interpolation

phi0_offset = 0.0


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def load_mas_var_filtered(model, var_name):
    """Load MAS variable, filtering out files that don't match {var}{3digits}.hdf pattern"""
    directory = Path(model.path)
    all_files = sorted(Path(directory).glob(f"{var_name}*"))
    pattern = re.compile(rf"^{var_name}\d{{3}}\.hdf$")
    filtered_files = [str(f) for f in all_files if pattern.match(f.name)]

    if not filtered_files:
        return model[var_name]

    data = [_read_mas(f, var_name) for f in filtered_files]
    var_data = data[0] if len(data) == 1 else xr.concat(data, dim="time")
    unit_info = model.get_unit(var_name)
    var_unit = unit_info[0] * unit_info[1]
    return Variable(var_data, var_name, var_unit, model.get_runit())


# ============================================================================
# MAIN RESAMPLING FUNCTION
# ============================================================================

def resample_MAS(model_path, N_pix, X_range, Y_range, N_z, dz0, variable_spacing_z=True,
                 z_range=None, out_path='LOS_data.npz', save_plots=True, verbose=True, phi0_offset=0.0):
    """
    Resample MAS model along line-of-sight for each pixel.

    Parameters
    ----------
    model_path : str
        Path to MAS model directory (e.g. "./corona")
    N_pix : int
        Image size (N_pix x N_pix)
    X_range : tuple or list
        (x_min, x_max) in R_sun
    Y_range : tuple or list
        (y_min, y_max) in R_sun
    N_z : int
        Number of points along each LOS
    dz0 : float
        Initial spacing for irregular z grid (used when variable_spacing_z=True)
    variable_spacing_z : bool, optional
        If True, use irregular z grid: dz = dz0 * (1 + (5*idx_z/N_z)**2.5).
        If False, use regular linear spacing; z_range must be set.
    z_range : tuple or list, optional
        (z_min, z_max) in R_sun for LOS. Used when variable_spacing_z=False.
        Default [0, 4] when variable_spacing_z=False.
    out_path : str, optional
        Path to save LOS_data.npz
    save_plots : bool, optional
        If True, save LOS_test_profiles.png and LOS_2D_slices.png
    verbose : bool, optional
        If True, print progress messages

    Returns
    -------
    dict
        Keys: 'Ne_LOS', 'Te_LOS', 'B_LOS', 'ds_LOS', 'x_coords', 'y_coords', 'z_coords'
        All arrays in physical units (cm^-3, K, G, cm, meters).
    """
    x_range = list(X_range)
    y_range = list(Y_range)

    if variable_spacing_z:
        # Irregular z grid: dz = dz0 * (1 + (5*idx_z/N_z)**2.5)
        idx_z = np.arange(N_z)
        dz = dz0 * (1 + (5*idx_z/N_z)**2.5)
        z_grid_cumsum = np.cumsum(dz)
        z_coords_Rsun = z_grid_cumsum.copy()  # Starts at 0, in R_sun
    else:
        # Regular linear spacing
        if z_range is None:
            z_range = [0.0, 4.0]
        z_coords_Rsun = np.linspace(z_range[0], z_range[1], N_z)
        dz = np.diff(z_coords_Rsun, prepend=z_coords_Rsun[0])
        z_grid_cumsum = np.cumsum(np.abs(dz))
        dz = np.abs(dz)

    z_coords = z_coords_Rsun * R_sun_m  # meters

    if verbose:
        print(f"max(z_coords_Rsun) = {np.max(z_coords_Rsun):.6f} R_sun")

    if verbose:
        print(f"Loading MAS model from {model_path}...")
    model = MASOutput(model_path)
    if verbose:
        print(f"Available variables: {model.variables}")

    if "te" in model.variables:
        temp_var = "te"
    elif "t" in model.variables:
        temp_var = "t"
    else:
        raise ValueError("No electron temperature variable (te or t) found!")
    if "br" not in model.variables or "bt" not in model.variables or "bp" not in model.variables:
        raise ValueError("Magnetic field components (br, bt, bp) not all found!")

    if verbose:
        print(f"Using temperature variable: {temp_var}\nLoading variables...")
    rho_var = model["rho"]
    te_var = model[temp_var]
    br_var = load_mas_var_filtered(model, "br")
    bt_var = load_mas_var_filtered(model, "bt")
    bp_var = load_mas_var_filtered(model, "bp")

    # Print datetime / time from dataset
    if verbose:
        time_vals = np.atleast_1d(rho_var.time_coords)
        if len(time_vals) == 1:
            print(f"Dataset time: {time_vals[0]}")
        else:
            print(f"Dataset time: {time_vals}")

    x_coords = np.linspace(x_range[0], x_range[1], N_pix) * R_sun_m
    y_coords = np.linspace(y_range[0], y_range[1], N_pix) * R_sun_m
    X, Y = np.meshgrid(x_coords, y_coords)

    Ne_LOS = np.zeros((N_pix, N_pix, N_z))
    Te_LOS = np.zeros((N_pix, N_pix, N_z))
    B_LOS = np.zeros((N_pix, N_pix, N_z))
    ds_LOS = np.zeros((N_pix, N_pix, N_z))

    for k in range(N_z):
        ds_LOS[:, :, k] = dz[k] * R_sun_cm

    if verbose:
        print(f"\nSampling along LOS... Image size: {N_pix}x{N_pix}, LOS points: {N_z}")

    for i in range(N_pix):
        if verbose and (i + 1) % 50 == 0:
            print(f"Processing row {i+1}/{N_pix}...")

        for j in range(N_pix):
            x = X[i, j]
            y = Y[i, j]
            # Stable projected radius; avoid negative radicands at the limb from float noise.
            rho_xy = np.hypot(x, y)
            if rho_xy < R_sun_m:
                z_start = np.sqrt(max(R_sun_m**2 - rho_xy**2, 0.0)) - 1e-6
            else:
                z_start = -np.sqrt(max(rho_xy**2 - R_sun_m**2, 0.0)) - 1e-6

            x_arr = np.full(N_z, x)
            y_arr = np.full(N_z, y)
            z_arr = z_start + z_coords

            lon_deg_arr, lat_deg_arr, r_m_arr = cart_to_mas_lonlat(
                x_arr, y_arr, z_arr, phi0_offset=phi0_offset
            )
            r_Rsun_arr = r_m_arr / R_sun_m
            valid_mask = r_Rsun_arr >= r_min

            Ne_LOS[i, j, :] = np.nan
            Te_LOS[i, j, :] = np.nan
            B_LOS[i, j, :] = np.nan

            if not np.any(valid_mask):
                continue

            r_arr = r_Rsun_arr * u.R_sun
            lat_arr = lat_deg_arr * u.deg
            lon_arr = lon_deg_arr * u.deg

            try:
                ne_val_arr = rho_var.sample_at_coords(lon_arr, lat_arr, r_arr)
                Ne_LOS[i, j, :] = np.asarray(ne_val_arr.to(u.cm**-3).value)
                te_val_arr = te_var.sample_at_coords(lon_arr, lat_arr, r_arr)
                Te_LOS[i, j, :] = np.asarray(te_val_arr.to(u.K).value)
                br_val_arr = br_var.sample_at_coords(lon_arr, lat_arr, r_arr)
                bt_val_arr = bt_var.sample_at_coords(lon_arr, lat_arr, r_arr)
                bp_val_arr = bp_var.sample_at_coords(lon_arr, lat_arr, r_arr)
                br_G_arr = np.asarray(br_val_arr.to(u.G).value)
                bt_G_arr = np.asarray(bt_val_arr.to(u.G).value)
                bp_G_arr = np.asarray(bp_val_arr.to(u.G).value)
                B_LOS[i, j, :] = np.sqrt(br_G_arr**2 + bt_G_arr**2 + bp_G_arr**2)
                Ne_LOS[i, j, ~valid_mask] = np.nan
                Te_LOS[i, j, ~valid_mask] = np.nan
                B_LOS[i, j, ~valid_mask] = np.nan
            except Exception:
                pass

    if verbose:
        print("Sampling complete!")

    result = {
        'Ne_LOS': Ne_LOS,
        'Te_LOS': Te_LOS,
        'B_LOS': B_LOS,
        'ds_LOS': ds_LOS,
        'x_coords': x_coords,
        'y_coords': y_coords,
        'z_coords': z_coords,
    }

    np.savez_compressed(out_path, **result)
    if verbose:
        print(f"LOS data saved to {out_path}")

    if save_plots:
        _save_resampling_plots(result, N_pix, x_range, y_range, R_sun_m, verbose)

    if verbose:
        print("Resampling complete!")
    return result


def _save_resampling_plots(result, N_pix, x_range, y_range, R_sun_m, verbose):
    """Save LOS test profiles and 2D slice plots."""
    Ne_LOS = result['Ne_LOS']
    Te_LOS = result['Te_LOS']
    B_LOS = result['B_LOS']
    z_coords = result['z_coords']

    test_pixels = [(N_pix//2, N_pix//2), (N_pix//4, N_pix//4), (3*N_pix//4, 3*N_pix//4)]
    fig, axes = plt.subplots(2, len(test_pixels), figsize=(15, 8))
    for idx, (i, j) in enumerate(test_pixels):
        ax1 = axes[0, idx]
        z_plot = z_coords / R_sun_m
        ax1.plot(z_plot, Ne_LOS[i, j, :], 'b-', linewidth=2)
        ax1.set_xlabel('z (R_sun)')
        ax1.set_ylabel('N_e (cm^-3)')
        ax1.set_yscale('log')
        ax1.set_title(f'Pixel ({i}, {j})\nN_e along LOS')
        ax1.grid(True, alpha=0.3)
        ax2 = axes[1, idx]
        ax2.plot(z_plot, Te_LOS[i, j, :], 'r-', linewidth=2)
        ax2.set_xlabel('z (R_sun)')
        ax2.set_ylabel('T_e (K)')
        ax2.set_yscale('log')
        ax2.set_title('T_e along LOS')
        ax2.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig('LOS_test_profiles.png', dpi=150, bbox_inches='tight')
    plt.close()

    z_mid_idx = min(2, Ne_LOS.shape[2] - 1)
    z_mid_idx = 1
    fig2, axes2 = plt.subplots(1, 3, figsize=(18, 5))
    im1 = axes2[0].imshow(np.log10(Ne_LOS[:, :, z_mid_idx]) , origin='lower',
                          extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                          aspect='equal', cmap='viridis')
    axes2[0].set_xlabel('x (R_sun)')
    axes2[0].set_ylabel('y (R_sun)')
    axes2[0].set_title(f'N_e at z={z_coords[z_mid_idx]/R_sun_m:.2f} R_sun')
    plt.colorbar(im1, ax=axes2[0], label='N_e (cm^-3)')

    im2 = axes2[1].imshow(np.log10(Te_LOS[:, :, z_mid_idx]), origin='lower',
                         extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                         aspect='equal', cmap='plasma',vmax=5.5)
    axes2[1].set_xlabel('x (R_sun)')
    axes2[1].set_ylabel('y (R_sun)')
    axes2[1].set_title(f'T_e at z={z_coords[z_mid_idx]/R_sun_m:.2f} R_sun')
    plt.colorbar(im2, ax=axes2[1], label='T_e (K) [log10]')

    im3 = axes2[2].imshow(np.log10(B_LOS[:, :, z_mid_idx]), origin='lower',
                         extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                         aspect='equal', cmap='hot')
    axes2[2].set_xlabel('x (R_sun)')
    axes2[2].set_ylabel('y (R_sun)')
    axes2[2].set_title(f'|B| at z={z_coords[z_mid_idx]/R_sun_m:.2f} R_sun')
    plt.colorbar(im3, ax=axes2[2], label='|B| (G) [log10]')
    plt.tight_layout()
    plt.savefig('LOS_2D_slices.png', dpi=150, bbox_inches='tight')
    plt.close()
    if verbose:
        print("Test plots saved to LOS_test_profiles.png and LOS_2D_slices.png")


# ============================================================================
# SCRIPT ENTRY POINT
# ============================================================================

def _parse_range(s):
    """Parse 'min,max' string into [min, max] floats."""
    parts = s.split(',')
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"Range must be 'min,max' (e.g. -1.5,1.5), got {s}")
    return [float(p.strip()) for p in parts]


def main():

# example usage:
# python resampling_MAS.py -m ./corona -n 256 -f 2.1 -z 400 -d 3e-4 -v -o LOS_data.npz -p -q

    parser = argparse.ArgumentParser(
        description='Resample MAS model along line-of-sight for emission calculation.')
    parser.add_argument('--model-path', '-m',type=str,default='./corona',
        help='Path to MAS model directory (default: ./corona)')
    parser.add_argument('--N-pix', '-n',type=int,default=128,
        help='Image size N_pix x N_pix (default: 256)')
    parser.add_argument('--X-FOV', '-f', type=float, default=1.44,
        help='Field of view half-extent in R_sun; x,y in [-X-FOV, X-FOV] (default: 1.44)')
    parser.add_argument('--N-z', '-z',type=int,default=400,
        help='Number of points along each LOS (default: 400)')
    parser.add_argument('--dz0', '-d',type=float,default=3e-4,
        help='Initial spacing for irregular z grid (default: 3e-4)')
    parser.add_argument('--no-variable-spacing-z', '-v',action='store_true',
        help='Use regular linear z spacing instead of irregular grid')
    parser.add_argument('--z-range', '-zr',type=_parse_range,default=None,
        help='Z extent in R_sun for linear spacing (used with --no-variable-spacing-z, default: 0,4)')
    parser.add_argument('--out-path', '-o',type=str,default='LOS_data.npz',
        help='Output path for LOS_data.npz (default: LOS_data.npz)')
    parser.add_argument('--no-plots', '-p',action='store_true',
        help='Do not save LOS test profile and 2D slice plots')
    parser.add_argument('--quiet', '-q',action='store_true',
        help='Suppress progress messages')
    args = parser.parse_args()

    fov = args.X_FOV
    resample_MAS(
        model_path=args.model_path,
        N_pix=args.N_pix,
        X_range=[-fov, fov],
        Y_range=[-fov, fov],
        N_z=args.N_z,
        dz0=args.dz0,
        variable_spacing_z=not args.no_variable_spacing_z,
        z_range=args.z_range,
        out_path=args.out_path,
        save_plots=not args.no_plots,
        verbose=not args.quiet,
    )


if __name__ == '__main__':
    main()