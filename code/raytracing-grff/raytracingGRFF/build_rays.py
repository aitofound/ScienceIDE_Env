#!/usr/bin/env python
"""
Resample MAS model onto a regular HCC xyz cube, run ray tracing, and plot rays.

Cartesian frame: +x solar west, +y solar north, +z toward the observer.
Rays launch toward -z. See ``raytracingGRFF.coords``.
"""

import argparse
import sys
import warnings
import re
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.interpolate import RegularGridInterpolator

from numpy.linalg import norm
from numpy import cross

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))
from raytracingGRFF.coords import cart_to_mas_lonlat

warnings.filterwarnings('ignore')

R_SUN_M = 6.957e8
R_MIN = 0.9999999
PHI0_OFFSET = 0.0

CONST_C = 2.998e10
R_S = 6.96e10
C = 2.998e10
C_R = C / R_S


def load_mas_var_filtered(model, var_name):
    """Load MAS variable with filtered HDF files (matches resampling_MAS_LOS)."""
    from psipy.io.mas import _read_mas
    import xarray as xr
    from psipy.model.variable import Variable

    directory = Path(model.path)
    all_files = sorted(Path(directory).glob(f"{var_name}*"))
    # Support both 3-digit and 6-digit snapshot suffixes:
    #   rho000.hdf, rho000020.hdf, etc.
    pattern = re.compile(rf"^{var_name}\d+\.hdf$")
    filtered_files = [str(f) for f in all_files if f.name and pattern.match(f.name)]

    if not filtered_files:
        # Fallback for datasets where psipy registers suffixed names
        # (e.g. rho000 instead of rho).
        model_vars = {str(v).lower() for v in getattr(model, "variables", [])}
        if var_name in model_vars:
            return model[var_name]
        suffixed = sorted(v for v in model_vars if v.startswith(var_name))
        if suffixed:
            return model[suffixed[0]]
        return model[var_name]

    data = [_read_mas(f, var_name) for f in filtered_files]
    var_data = data[0] if len(data) == 1 else xr.concat(data, dim="time")
    try:
        unit_info = model.get_unit(var_name)
    except Exception:
        # Try with a suffixed key if base name is not registered.
        model_vars = [str(v).lower() for v in getattr(model, "variables", [])]
        suffixed = [v for v in model_vars if v.startswith(var_name)]
        if not suffixed:
            raise
        unit_info = model.get_unit(suffixed[0])
    var_unit = unit_info[0] * unit_info[1]
    return Variable(var_data, var_name, var_unit, model.get_runit())


def resample_to_xyz_cube(model, var_name, x_grid, y_grid, z_grid, phi0_offset=0.0,
                         fill_nan=0.0, verbose=True):
    """Resample MAS variable onto a regular HCC xyz grid.

    +x west, +y north, +z toward observer. See ``raytracingGRFF.coords``.
    """
    import astropy.units as u

    var = load_mas_var_filtered(model, var_name)

    ny = len(y_grid)
    nz = len(z_grid)
    out = np.full((len(x_grid), ny, nz), np.nan, dtype=float)

    y_mesh, z_mesh = np.meshgrid(y_grid, z_grid, indexing='ij')

    for ix, x_val in enumerate(x_grid):
        if verbose and (ix + 1) % 25 == 0:
            print(f"Resampling x-slice {ix+1}/{len(x_grid)}")

        x_mesh = np.full_like(y_mesh, x_val)

        lon_deg, lat_deg, r = cart_to_mas_lonlat(
            x_mesh, y_mesh, z_mesh, phi0_offset=phi0_offset
        )

        r_mask = np.isfinite(r) & (r >= R_MIN)
        if not np.any(r_mask):
            continue

        vals = np.full_like(r, np.nan, dtype=float)
        r_arr = (r[r_mask] * u.R_sun)
        lat_arr = (lat_deg[r_mask] * u.deg)
        lon_arr = (lon_deg[r_mask] * u.deg)

        try:
            sampled = var.sample_at_coords(lon_arr, lat_arr, r_arr)
            try:
                sampled_vals = np.asarray(sampled.to(u.cm**-3).value)
            except Exception:
                sampled_vals = np.asarray(sampled.value)
            vals[r_mask] = sampled_vals
        except Exception:
            pass

        vals[~r_mask] = np.nan
        out[ix, :, :] = vals

    if fill_nan is not None:
        out = np.where(np.isfinite(out), out, fill_nan)

    return out


def ray_trace(omega_pe_3d, x_grid, y_grid, z_grid, freq_hz, x_start, y_start, z_start,
              kvec_in_norm, dt, n_steps, record_stride=10, trace_crosssections=False, cross_section_stride=1,
              perturb_ratio=2):
    """Run simple ray tracing as in ray_tracing_demo.ipynb."""
    dx_g = x_grid[1] - x_grid[0]
    dy_g = y_grid[1] - y_grid[0]
    dz_g = z_grid[1] - z_grid[0]

    domega_pe_dx_3d = np.gradient(omega_pe_3d, dx_g, axis=0)
    domega_pe_dy_3d = np.gradient(omega_pe_3d, dy_g, axis=1)
    domega_pe_dz_3d = np.gradient(omega_pe_3d, dz_g, axis=2)

    omega_pe_interp = RegularGridInterpolator((x_grid, y_grid, z_grid), omega_pe_3d, bounds_error=False, fill_value=np.nan)
    domega_pe_dx_interp = RegularGridInterpolator((x_grid, y_grid, z_grid), domega_pe_dx_3d, bounds_error=False, fill_value=np.nan)
    domega_pe_dy_interp = RegularGridInterpolator((x_grid, y_grid, z_grid), domega_pe_dy_3d, bounds_error=False, fill_value=np.nan)
    domega_pe_dz_interp = RegularGridInterpolator((x_grid, y_grid, z_grid), domega_pe_dz_3d, bounds_error=False, fill_value=np.nan)

    starting_point = np.vstack([x_start, y_start, z_start]).T

    omega0 = 2 * np.pi * freq_hz
    omega_pe_starting_point = omega_pe_interp(starting_point)
    kc0 = np.sqrt(np.maximum(omega0**2 - omega_pe_starting_point**2, 0.0))

    k_vec = kvec_in_norm * kc0[:, np.newaxis]
    r_vec = starting_point.copy()
    kc_cur = np.sqrt(np.sum(k_vec**2, axis=1))

    r_record = []
    crosssection_record = []
    
    def rhs(state):
        r_vec = state[:, 0:3]
        k_vec = state[:, 3:6]
        omega_pe = omega_pe_interp(r_vec)
        omega = np.sqrt(omega_pe**2 + np.sum(k_vec**2, axis=1))
        domega_pe_dxyz = np.array([
            domega_pe_dx_interp(r_vec),
            domega_pe_dy_interp(r_vec),
            domega_pe_dz_interp(r_vec),
        ]).T

        valid = np.isfinite(omega_pe) & np.isfinite(omega) & (omega > 0)
        dr_vec = np.zeros_like(r_vec)
        dk_vec = np.zeros_like(k_vec)
        if np.any(valid):
            dr_vec[valid] = C_R / omega[valid, None] * k_vec[valid]
            dk_vec[valid] = -omega_pe[valid, None] / omega[valid, None] * domega_pe_dxyz[valid] * C_R
        return np.hstack([dr_vec, dk_vec])

    def rk4_step(state, dt):
        k1 = rhs(state)
        k2 = rhs(state + 0.5 * dt * k1)
        k3 = rhs(state + 0.5 * dt * k2)
        k4 = rhs(state + dt * k3)
        return state + (dt / 6.0) * (k1 + 2 * k2 + 2 * k3 + k4)

    state = np.hstack([r_vec, k_vec])

    crosssection0 = 0

    def make_e1e2_from_t(t_hat):
        # t_hat: (N,3) unit vectors
        # pick reference axis least aligned with t_hat for numerical stability
        a = np.zeros_like(t_hat)
        use_z = np.abs(t_hat[:, 2]) < 0.9
        a[use_z] = np.array([0.0, 0.0, 1.0])
        a[~use_z] = np.array([0.0, 1.0, 0.0])

        e1 = np.cross(a, t_hat)
        e1 /= np.linalg.norm(e1, axis=1, keepdims=True) + 1e-30
        e2 = np.cross(t_hat, e1)
        # e2 is already unit if t_hat and e1 are unit+orthogonal; normalize anyway:
        e2 /= np.linalg.norm(e2, axis=1, keepdims=True) + 1e-30
        return e1, e2


    for i in range(n_steps):

        state0= state.copy()
        state = rk4_step(state, dt)

        if trace_crosssections:

            r0 = state0[:, 0:3]
            k0 = state0[:, 3:6]
            r_new = state[:, 0:3]
            r_diff = r_new - r0

            # direction for basis (use r_diff from the central step)
            t_hat = r_diff / (norm(r_diff, axis=1, keepdims=True) + 1e-32)
            e1, e2 = make_e1e2_from_t(t_hat)

            eps = perturb_ratio * norm(r_diff, axis=1)  # (n_rays,)

            # two perturbed rays at the same starting "origin"
            r1 = r0 + eps[:, np.newaxis] * e1
            r2 = r0 + eps[:, np.newaxis] * e2

            state1_0 = np.hstack([r1, k0])
            state2_0 = np.hstack([r2, k0])

            state1_1 = rk4_step(state1_0, dt)
            state2_1 = rk4_step(state2_0, dt)

            r1_1 = state1_1[:, 0:3]
            r2_1 = state2_1[:, 0:3]
            r0_1 = state[:, 0:3]   # central already advanced

            d1 = r1_1 - r0_1
            d2 = r2_1 - r0_1
            # row-wise dot: cross(d1,d2) and t_hat are (n_rays, 3) -> (n_rays,)
            S_ratio = np.abs(np.sum(cross(d1, d2) * t_hat, axis=1)) / eps**2

        if i % record_stride == 0:
            r_record.append(state[:, 0:3].copy())
            if trace_crosssections:
                crosssection_record.append(S_ratio.copy())

    r_vec = state[:, 0:3]

    return np.array(r_record), crosssection_record


def plot_rays(omega_pe_3d, x_grid, y_grid, z_grid, r_record, out_path, y_index=None):
    """Plot x-z slice of omega_pe and overlay ray paths."""
    if y_index is None:
        y_index = len(y_grid) // 2

    plt.figure(dpi=150)
    slice_data = omega_pe_3d[:, y_index, :].T
    slice_plot = np.where(np.isfinite(slice_data), slice_data, 0.0)
    im = plt.imshow(np.log10(slice_plot + 1e-30),
                    extent=[x_grid[0], x_grid[-1], z_grid[0], z_grid[-1]],
                    origin='lower', vmin=6, vmax=9.5)
    plt.colorbar(im, label='log10(omega_pe)')

    for i in range(r_record.shape[1]):
        xr = r_record[:, i, 0]
        zr = r_record[:, i, 2]
        mask = np.isfinite(xr) & np.isfinite(zr)
        if np.any(mask):
            plt.plot(xr[mask], zr[mask], 'w', linewidth=0.8)

    if r_record.shape[1] > 0:
        plt.plot(r_record[:, r_record.shape[1] // 2, 0], r_record[:, r_record.shape[1] // 2, 2], 'r', linewidth=1.2)

    plt.xlabel('x (R_sun)')
    plt.ylabel('z (R_sun)')
    plt.xlim(x_grid[0], x_grid[-1])
    plt.ylim(z_grid[0], z_grid[-1])
    plt.tight_layout()
    plt.savefig(out_path, bbox_inches='tight')
    plt.close()


def main():
    from psipy.model import MASOutput

    parser = argparse.ArgumentParser(description='Resample MAS model to xyz cube, ray trace, and plot rays.')
    parser.add_argument('--model-path', '-m', type=str, default='./corona',
                        help='Path to MAS model directory (default: ./corona)')
    parser.add_argument('--var', type=str, default='rho',
                        help='MAS variable for density (default: rho)')
    parser.add_argument('--grid-min', type=float, default=-4.0,
                        help='Grid minimum in R_sun for x,y,z (default: -4)')
    parser.add_argument('--grid-max', type=float, default=4.0,
                        help='Grid maximum in R_sun for x,y,z (default: 4)')
    parser.add_argument('--grid-n', type=int, default=300,
                        help='Number of grid points per axis (default: 300)')
    parser.add_argument('--freq-mhz', type=float, default=75.0,
                        help='Ray frequency in MHz (default: 40)')
    parser.add_argument('--z-observer', type=float, default=3.999,
                        help='Observer z in R_sun; rays launch toward -z (default: 3.999)')
    parser.add_argument('--y-start', type=float, default=0.0,
                        help='Ray start y in R_sun, solar north (default: 0.0)')
    parser.add_argument('--x-start-min', type=float, default=-1.5,
                        help='Ray start x min in R_sun, solar west (default: -1.5)')
    parser.add_argument('--x-start-max', type=float, default=1.5,
                        help='Ray start x max in R_sun, solar west (default: 1.5)')
    parser.add_argument('--n-rays', type=int, default=18,
                        help='Number of rays (default: 18)')
    parser.add_argument('--dt', type=float, default=10e-3,
                        help='Time step (default: 5e-3)')
    parser.add_argument('--n-steps', type=int, default=6000,
                        help='Number of integration steps (default: 12000)')
    parser.add_argument('--record-stride', type=int, default=10,
                        help='Record stride for rays (default: 10)')
    parser.add_argument('--out-plot', type=str, default='rays.png',
                        help='Output plot path (default: rays.png)')
    parser.add_argument('--fill-nan', type=float, default=0.0,
                        help='Fill value for NaNs after resampling (default: 0.0)')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress progress messages')
    args = parser.parse_args()

    if not args.quiet:
        print(f"Loading MAS model from {args.model_path}...")
    model = MASOutput(args.model_path)
    if not args.quiet:
        try:
            r_coords = np.asarray(model[args.var].data["r"])
            print(f"Model r range: {r_coords.min():.3f} .. {r_coords.max():.3f} R_sun")
        except Exception:
            pass

    x_grid = np.linspace(args.grid_min, args.grid_max, args.grid_n)
    y_grid = np.linspace(args.grid_min, args.grid_max, args.grid_n)
    z_grid = np.linspace(args.grid_min, args.grid_max, args.grid_n)

    if not args.quiet:
        print(f"Resampling {args.var} onto xyz grid: {args.grid_n}^3")
    rhoxyz = resample_to_xyz_cube(
        model=model,
        var_name=args.var,
        x_grid=x_grid,
        y_grid=y_grid,
        z_grid=z_grid,
        phi0_offset=PHI0_OFFSET,
        fill_nan=args.fill_nan,
        verbose=not args.quiet,
    )

    if not args.quiet:
        finite_count = np.isfinite(rhoxyz).sum()
        total_count = rhoxyz.size
        print(f"Resample stats: finite={finite_count}/{total_count}, min={np.nanmin(rhoxyz):.3e}, max={np.nanmax(rhoxyz):.3e}")

    if not args.quiet:
        print("Computing plasma frequency and tracing rays...")
    omega_pe_3d = 8.93e3 * np.sqrt(rhoxyz) * 2 * np.pi

    x_start = np.linspace(args.x_start_min, args.x_start_max, args.n_rays)
    y_start = np.zeros_like(x_start) + args.y_start
    z_start = np.zeros_like(x_start) + args.z_observer
    kvec_in_norm = np.tile(np.array([[0.0, 0.0, -1.0]]), (len(x_start), 1))

    r_record, _ = ray_trace(
        omega_pe_3d=omega_pe_3d,
        x_grid=x_grid,
        y_grid=y_grid,
        z_grid=z_grid,
        freq_hz=args.freq_mhz * 1e6,
        x_start=x_start,
        y_start=y_start,
        z_start=z_start,
        kvec_in_norm=kvec_in_norm,
        dt=args.dt,
        n_steps=args.n_steps,
        record_stride=args.record_stride,
    )

    if not args.quiet:
        print(f"Plotting rays to {args.out_plot}...")
    plot_rays(
        omega_pe_3d=omega_pe_3d,
        x_grid=x_grid,
        y_grid=y_grid,
        z_grid=z_grid,
        r_record=r_record,
        out_path=args.out_plot,
    )

    if not args.quiet:
        print("Done.")


if __name__ == '__main__':
    main()
