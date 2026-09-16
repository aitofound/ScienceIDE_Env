#!/usr/bin/env python
"""
Resample MAS model along ray-traced paths and compute GRFF emission.

Similar to resampling_MAS_LOS.py but uses ray tracing (build_rays.ray_trace)
instead of straight LOS. Cartesian frame is heliocentric: +x solar west,
+y projected solar north, +z toward the observer, so (x, y) is helioprojective.
For a N_pix x N_pix image (default 64x64), each pixel has one ray from the
observer (large +z) toward -z. At each ray point we
sample Ne, Te, and MAS B components (br, bt, bp -> Cartesian Bx, By, Bz), flip
the voxel list for GRFF radiative transfer (deep-sun -> observer), and pass
viewing angles theta/phi from the local ray direction and B field. Default
mechanism is thermal free-free + gyrosynchrotron (``mech_flag=4``).
GRFF 17-parameter external layout includes ``Dist_E`` (row 15) and ``kappa`` (row 16);
see ``raytracingGRFF.grff_parms``.

Output emission_cube is brightness temperature T_b in K: GRFF returns flux in
SFU; we convert via Rayleigh-Jeans (I = 2*k_B*T_b*nu^2/c^2) and solid angle
Omega = pixel_area / AU^2 to get T_b.
"""

import argparse
import warnings
import re
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor
from tqdm import tqdm

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import astropy.units as u
try:
    import sunpy.visualization.colormaps.color_tables as sunpy_ct
except Exception:
    sunpy_ct = None

from psipy.model import MASOutput
from psipy.io.mas import _read_mas
import xarray as xr
from psipy.model.variable import Variable

import sys
from raytracingGRFF.build_rays import ray_trace, resample_to_xyz_cube, load_mas_var_filtered
from raytracingGRFF.coords import cart_to_mas_lonlat
from raytracingGRFF.gpu_raytrace import sample_model_with_rays, trace_ray
from raytracingGRFF.grff_ctypes import default_grff_lib_path, initGET_MW
from raytracingGRFF.grff_parms import (
    GRFF_PARMS_EXT_SIZE,
    MECH_FLAG_FF_GR,
    fill_grff_parms_ext_column,
    prepare_ray_voxels_for_grff,
    rl_stokes_to_tb_vi,
)
from raytracingGRFF.util import (
    beam_fwhm_from_lambda_over_d,
    convolve_tb_gaussian_beam,
    format_beam_summary,
    patch_nan_emission_map,
)

warnings.filterwarnings('ignore')


def _ray_trace_chunk(args):
    """Worker: run ray_trace for a chunk of rays. Used by ProcessPoolExecutor."""
    (chunk_start, chunk_end, x_flat, y_flat, z_start, omega_pe_3d, xg, yg, zg,
     freq_hz, dt, n_steps, record_stride) = args
    x_chunk = x_flat[chunk_start:chunk_end]
    y_chunk = y_flat[chunk_start:chunk_end]
    z_chunk = z_start[chunk_start:chunk_end]
    n_chunk = len(x_chunk)
    kvec = np.tile([[0, 0, -1]], (n_chunk, 1))
    r_record, crosssection_record = ray_trace(
        omega_pe_3d=omega_pe_3d,
        x_grid=xg, y_grid=yg, z_grid=zg,
        freq_hz=freq_hz,
        x_start=x_chunk, y_start=y_chunk, z_start=z_chunk,
        kvec_in_norm=kvec,
        dt=dt, n_steps=n_steps, record_stride=record_stride,
        trace_crosssections=True, perturb_ratio=2,
    )
    S_chunk = np.array(crosssection_record)
    return (r_record, S_chunk)


# ============================================================================
# CONSTANTS
# ============================================================================

R_sun_cm = 6.957e10   # cm
R_sun_m = 6.957e8     # meters
R_MIN = 0.999999

GRFF_LIB = str(default_grff_lib_path())
R_sun = 6.957e10  # cm (for synthetic_FF_map compatibility)
c = 2.998e10     # speed of light, cm/s
kb = 1.38065e-16 # Boltzmann constant, erg/K
sfu2cgs = 1e-19
AU_cm = 1.49599e13
# Brightness temperature T_b (K): GRFF returns flux in SFU; Rayleigh-Jeans gives
# I = 2*k_B*T_b*nu^2/c^2 (I in erg/s/cm^2/Hz/sr). Flux F at 1 AU from one pixel
# subtends Omega = pixel_area_cm2/AU_cm^2, so I = F/Omega => T_b = F_cgs * (AU_cm^2/pixel_area_cm2) * c^2/(2*k_B*nu^2).


def resample_var_to_cube(model, var_name, x_grid, y_grid, z_grid, target_unit=None,
                         phi0_offset=0.0, fill_nan=0.0, verbose=True):
    """Resample a MAS variable onto an HCC xyz cube. target_unit: e.g. u.cm**-3, u.K, u.G."""

    var = load_mas_var_filtered(model, var_name)
    ny, nz = len(y_grid), len(z_grid)
    out = np.full((len(x_grid), ny, nz), np.nan, dtype=float)
    y_mesh, z_mesh = np.meshgrid(y_grid, z_grid, indexing='ij')

    x_iter = tqdm(list(enumerate(x_grid)), desc=f"Resample {var_name}", disable=not verbose, unit="slice")
    for ix, x_val in x_iter:
        x_mesh = np.full_like(y_mesh, x_val)
        lon_deg, lat_deg, r = cart_to_mas_lonlat(
            x_mesh, y_mesh, z_mesh, phi0_offset=phi0_offset
        )
        r_mask = np.isfinite(r) & (r >= R_MIN)
        if not np.any(r_mask):
            continue
        vals = np.full_like(r, np.nan, dtype=float)
        r_arr = r[r_mask] * u.R_sun
        lat_arr = lat_deg[r_mask] * u.deg
        lon_arr = lon_deg[r_mask] * u.deg
        try:
            sampled = var.sample_at_coords(lon_arr, lat_arr, r_arr)
            if target_unit is not None:
                try:
                    sampled_vals = np.asarray(sampled.to(target_unit).value)
                except Exception:
                    sampled_vals = np.asarray(sampled.value)
            else:
                sampled_vals = np.asarray(sampled.value)
            vals[r_mask] = sampled_vals
        except Exception:
            pass
        vals[~r_mask] = np.nan
        out[ix, :, :] = vals

    if fill_nan is not None:
        out = np.where(np.isfinite(out), out, fill_nan)
    return out


def run_ray_tracing_emission(model_path, N_pix=64, X_fov=1.44, freq_hz=75e6,
                              grid_n=400, grid_extent=3.0, z_observer=3.0,
                              dt=6e-3, n_steps=5000, record_stride=10,
                              n_workers=1, s_input_on=False,
                              out_path='ray_tracing_emission.npz', grff_lib=None,
                              Nfreq=1, freq0=None, freq_log_step=0.0,
                              save_plots=True, verbose=True,
                              device='cpu', fallback_to_cpu=True,
                              raytrace_device='cpu',
                              grff_backend='get_mw',
                              beam_fwhm_rsun=None,
                              beam_diameter_m=None,
                              beam_fwhm_factor=1.22,
                              phi0_offset=0,
                              plot_log_norm=False,
                              plot_vmin=None,
                              plot_vmax=None,
                              plot_beam=True,
                              grff_dist_e=0.0,
                              grff_kappa=0.0,
                              prepared_samples_path=None,
                              mech_flag=MECH_FLAG_FF_GR):
    """
    Run ray tracing for each pixel, sample Ne/Te/B along rays, and compute GRFF emission.

    Parameters
    ----------
    model_path : str
        Path to MAS model (e.g. ./corona).
    N_pix : int
        Image size N_pix x N_pix (default 64).
    X_fov : float
        Half FOV in R_sun; x,y in [-X_fov, X_fov].
    freq_hz : float
        Ray tracing frequency (Hz).
    grid_n : int
        Number of grid points per axis for 3D cubes.
    grid_extent : float
        ȳxyz grid extent in R_sun (e.g. [-grid_extent, grid_extent]).
    z_observer : float
        Ray start z in R_sun (observer side); rays go in -z.
    dt, n_steps, record_stride : float, int, int
        Ray integrator and recording.
    n_workers : int
        Number of processes for parallel ray tracing (1 = serial).
    s_input_on : bool
        If True, pass cross-section ratio S in Parms[14,k]; if False, put 0.
    out_path : str
        Output npz path.
    grff_lib : str or None
        Path to GRFF_DEM_Transfer.so.
    Nfreq, freq0, freq_log_step : int, float, float
        GRFF frequency setup (default single freq at freq_hz).
    save_plots : bool
        Save emission map plot.
    verbose : bool
        Print progress.
    device : str
        LOS sampling device: 'cpu' (default) or 'cuda'.
    fallback_to_cpu : bool
        If True and CUDA sampling is unavailable, fall back to CPU sampler.
    raytrace_device : str
        Ray integration device: 'cpu' (default) or 'cuda'.
    grff_backend : str
        'get_mw' (default CPU library call) or 'fastgrff' (GPU get_mw_slice).
    beam_fwhm_rsun : float or None
        If set, convolve with Gaussian FWHM in R_sun (e.g. 0.1).
    beam_diameter_m : float or None
        If set, convolve with θ_FWHM ≈ ``beam_fwhm_factor * λ / D`` at ``freq_hz``.
        Takes precedence over ``beam_fwhm_rsun`` when both are set.
    beam_fwhm_factor : float
        Multiplier in θ = factor * λ/D (default 1.22 HPBW; use 1.0 for Gaussian λ/D).
    phi0_offset : float
        Longitude offset in degrees added to MAS φ; disk center is Carrington phi0 (default 0).
    plot_log_norm : bool
        If True, emission map PNG uses matplotlib LogNorm (needs plot_vmin > 0).
    plot_vmin, plot_vmax : float or None
        Color scale limits for emission PNG; if None, linear scale uses 0 and data max.
    plot_beam : bool
        If True and a beam FWHM is set, draw a white circle (beam shape) at the lower left.
    grff_dist_e : float
        GRFF external Parms row 15: electron distribution (0 Maxwellian, 1 kappa, 2 n).
    grff_kappa : float
        GRFF external Parms row 16: kappa or n index when ``grff_dist_e`` is 1 or 2; ignored for 0.
    prepared_samples_path : str or None
        If set, save GRFF-order sampled Ne/Te/B/ds along all rays to this ``.npz`` path.
    mech_flag : float
        GRFF mechanism bitmask (Parms row 6). ``5`` = FF only; ``4`` = FF + gyrosynchrotron.

    Returns
    -------
    dict
        emission_cube, emission_polVI_cube, frequencies_Hz, x_coords, y_coords,
        Ne_LOS, Te_LOS, B_LOS, ds_LOS, S_LOS (for first-frequency compatibility).
    """
    if grff_lib is None:
        grff_lib = GRFF_LIB
    if freq0 is None:
        freq0 = freq_hz
    GET_MW = None
    get_mw_slice = None
    cp = None
    backend = grff_backend.lower()
    if backend == 'get_mw':
        lib_path = Path(grff_lib)
        if not lib_path.is_file():
            raise FileNotFoundError(f"GRFF library not found: {grff_lib}")
        if verbose:
            print("Loading GRFF library...")
        GET_MW = initGET_MW(str(lib_path))
    elif backend == 'fastgrff':
        if float(grff_dist_e) != 0.0 or float(grff_kappa) != 0.0:
            raise ValueError(
                "fastGRFF GPU backend uses a fixed 15-parameter layout; non-Maxwellian "
                "Dist_E/kappa requires --grff-backend get_mw with an updated GRFF .so."
            )
        try:
            import cupy as cp
            sys.path.insert(0, str((PROJECT_ROOT / "fastGRFF").resolve()))
            from fastGRFF import get_mw_slice as fast_get_mw_slice
            get_mw_slice = fast_get_mw_slice
        except Exception as e:
            raise RuntimeError("Failed to initialize fastGRFF backend. Ensure fastGRFF and CuPy are available.") from e
        if verbose:
            print("Using fastGRFF GPU backend (get_mw_slice)...")
    else:
        raise ValueError(f"Unsupported grff_backend '{grff_backend}'. Use 'get_mw' or 'fastgrff'.")

    if verbose:
        print(f"Loading MAS model from {model_path}...")
    model = MASOutput(str(model_path))
    # Some CCMC snapshot folders expose variables only via filenames
    # (e.g. rho000020.hdf, t000020.hdf), not via model.variables.
    model_vars = {str(v).lower() for v in getattr(model, "variables", [])}
    file_vars = set()
    for f in Path(model_path).glob("*.hdf"):
        m = re.match(r"^([A-Za-z]+)\d+\.hdf$", f.name)
        if m:
            file_vars.add(m.group(1).lower())
    available_vars = model_vars | file_vars

    if "te" in available_vars:
        temp_var = "te"
    elif "t" in available_vars:
        temp_var = "t"
    else:
        raise ValueError(
            "No electron temperature variable (te or t) found. "
            f"Available vars: {sorted(available_vars)}"
        )
    if not {"br", "bt", "bp"}.issubset(available_vars):
        raise ValueError(
            "Magnetic field components (br, bt, bp) not all found. "
            f"Available vars: {sorted(available_vars)}"
        )

    xg = np.linspace(-grid_extent, grid_extent, grid_n)
    yg = np.linspace(-grid_extent, grid_extent, grid_n)
    zg = np.linspace(-grid_extent, grid_extent, grid_n)

    if verbose:
        print("Resampling rho -> omega_pe (for ray tracing)...")
    rhoxyz = resample_to_xyz_cube(model, 'rho', xg, yg, zg, phi0_offset=phi0_offset,
                                  fill_nan=0.0, verbose=verbose)
    omega_pe_3d = 8.93e3 * np.sqrt(np.maximum(rhoxyz, 0.0)) * 2 * np.pi
    # Avoid NaN in ray tracing (e.g. R<1 or bad grid): ray stalls or kc0=NaN near disk center
    omega_pe_3d = np.nan_to_num(omega_pe_3d, nan=0.0, posinf=0.0, neginf=0.0)
    # Ne in cm^-3: same as resampling_MAS_LOS — sample rho at coords and convert to u.cm**-3
    if verbose:
        print("Resampling rho -> Ne (cm^-3, as in resampling_MAS_LOS)...")
    Ne_xyz = resample_var_to_cube(model, 'rho', xg, yg, zg, target_unit=u.cm**-3,
                                  phi0_offset=phi0_offset, fill_nan=0.0, verbose=verbose)
    Ne_xyz = np.maximum(Ne_xyz, 0.0)
    if verbose:
        print("Resampling Te...")
    Te_xyz = resample_var_to_cube(model, temp_var, xg, yg, zg, target_unit=u.K,
                                  phi0_offset=phi0_offset, fill_nan=np.nan, verbose=verbose)
    Te_xyz = np.where(np.isfinite(Te_xyz), Te_xyz, 1e4)
    if verbose:
        print("Resampling B components...")
    br_xyz = resample_var_to_cube(model, 'br', xg, yg, zg, target_unit=u.G,
                                  phi0_offset=phi0_offset, fill_nan=0.0, verbose=verbose)
    bt_xyz = resample_var_to_cube(model, 'bt', xg, yg, zg, target_unit=u.G,
                                  phi0_offset=phi0_offset, fill_nan=0.0, verbose=verbose)
    bp_xyz = resample_var_to_cube(model, 'bp', xg, yg, zg, target_unit=u.G,
                                  phi0_offset=phi0_offset, fill_nan=0.0, verbose=verbose)
    B_xyz = np.sqrt(br_xyz**2 + bt_xyz**2 + bp_xyz**2)

    # Image grid (R_sun)
    x_coords_Rsun = np.linspace(-X_fov, X_fov, N_pix)
    y_coords_Rsun = np.linspace(-X_fov, X_fov, N_pix)
    X_img, Y_img = np.meshgrid(x_coords_Rsun, y_coords_Rsun)
    x_flat = X_img.ravel()
    y_flat = Y_img.ravel()
    n_rays = len(x_flat)
    z_start = np.sqrt( np.abs((z_observer*2.0)**2 - x_flat**2 - y_flat**2))/2.0
    kvec_in_norm = np.tile([[0, 0, -1]], (n_rays, 1))

    if raytrace_device == 'cuda':
        if verbose:
            print(f"Ray tracing {n_rays} rays on CUDA...")
        r_record, crosssection_record = trace_ray(
            device='cuda',
            omega_pe_3d=omega_pe_3d,
            x_grid=xg, y_grid=yg, z_grid=zg,
            freq_hz=freq_hz,
            x_start=x_flat, y_start=y_flat, z_start=z_start,
            kvec_in_norm=kvec_in_norm,
            dt=dt, n_steps=n_steps, record_stride=record_stride,
            trace_crosssections=True, perturb_ratio=5,
        )
        S_arr = np.array(crosssection_record)
    else:
        if n_workers <= 1:
            if verbose:
                print(f"Ray tracing {n_rays} rays (serial)...")
            r_record, crosssection_record = ray_trace(
                omega_pe_3d=omega_pe_3d,
                x_grid=xg, y_grid=yg, z_grid=zg,
                freq_hz=freq_hz,
                x_start=x_flat, y_start=y_flat, z_start=z_start,
                kvec_in_norm=kvec_in_norm,
                dt=dt, n_steps=n_steps, record_stride=record_stride,
                trace_crosssections=True, perturb_ratio=2,
            )
            S_arr = np.array(crosssection_record)
        else:
            if verbose:
                print(f"Ray tracing {n_rays} rays in parallel ({n_workers} workers)...")
            chunk_size = (n_rays + n_workers - 1) // n_workers
            chunk_args = []
            for w in range(n_workers):
                start = w * chunk_size
                end = min(start + chunk_size, n_rays)
                if start >= end:
                    continue
                chunk_args.append((
                    start, end, x_flat, y_flat, z_start, omega_pe_3d, xg, yg, zg,
                    freq_hz, dt, n_steps, record_stride,
                ))
            with ProcessPoolExecutor(max_workers=n_workers) as executor:
                results = list(executor.map(_ray_trace_chunk, chunk_args))
            r_record_list = [r for r, s in results]
            S_list = [s for r, s in results]
            r_record = np.concatenate(r_record_list, axis=1)
            S_arr = np.concatenate(S_list, axis=1)

    Nf = Nfreq
    frequencies_Hz = freq0 * (10.0 ** (freq_log_step * np.arange(Nf)))
    Lparms = np.zeros(5, dtype='int32')
    Lparms[0] = 0  # set per pixel
    Lparms[1] = Nf
    Rparms = np.zeros(3, dtype='double')
    pixel_size_Rsun = (2 * X_fov) / N_pix
    pixel_size_cm = pixel_size_Rsun * R_sun_cm
    pixel_area_cm2 = pixel_size_cm * pixel_size_cm
    Rparms[0] = pixel_area_cm2
    Rparms[1] = freq0
    Rparms[2] = freq_log_step

    x_coords = x_coords_Rsun * R_sun_m
    y_coords = y_coords_Rsun * R_sun_m
    emission_cube = np.zeros((N_pix, N_pix, Nf), dtype='double')
    emission_polVI_cube = np.zeros((N_pix, N_pix, Nf), dtype='double')

    if verbose:
        print(f"Sampling Ne/Te/B along rays on device='{device}' and calling GRFF...")
    ray_start = np.column_stack([x_flat, y_flat, z_start])  # (n_rays, 3)
    sampled = sample_model_with_rays(
        device=device,
        x_grid=xg,
        y_grid=yg,
        z_grid=zg,
        ne_xyz=Ne_xyz,
        te_xyz=Te_xyz,
        b_xyz=B_xyz,
        br_xyz=br_xyz,
        bt_xyz=bt_xyz,
        bp_xyz=bp_xyz,
        r_record=r_record,
        s_arr=S_arr,
        ray_start=ray_start,
        r_sun_cm=R_sun_cm,
        fill_ne=0.0,
        fill_te=1e4,
        fill_b=0.0,
        fallback_to_cpu=fallback_to_cpu,
        verbose=verbose,
    )
    if "bx" not in sampled:
        raise RuntimeError("B-component sampling failed; br/bt/bp cubes are required for GRFF RT.")
    prepared = prepare_ray_voxels_for_grff(sampled, r_record, R_sun_cm)
    ne_all = prepared['ne']
    te_all = prepared['te']
    b_all = prepared['b']
    ds_all = prepared['ds']
    valid_all = prepared['valid_mask']
    s_all = prepared['s']
    theta_all = prepared['theta_deg']
    phi_all = prepared['phi_deg']

    if prepared_samples_path is not None:
        np.savez_compressed(
            prepared_samples_path,
            ne=ne_all,
            te=te_all,
            b=b_all,
            ds=ds_all,
            valid_mask=valid_all,
            theta_deg=theta_all,
            phi_deg=phi_all,
            x_flat=x_flat,
            y_flat=y_flat,
            N_pix=np.int32(N_pix),
            X_fov=np.float64(X_fov),
            freq_hz=np.float64(freq_hz),
        )
        if verbose:
            print(f"Saved prepared ray samples: {prepared_samples_path}")

    if backend == 'fastgrff':
        n_rec = ne_all.shape[0]
        if verbose:
            print(f"Running fastGRFF get_mw_slice for {n_rays} pixels, Nz={n_rec}, Nf={Nf}...")
        Parms_M = np.zeros((15, n_rec, n_rays), dtype=np.float64, order='F')
        Parms_M[6, :, :] = mech_flag
        Parms_M[7, :, :] = 30
        for p in range(n_rays):
            # Require finite ne/te/b so GRFF and emission stay finite (avoids NaN near disk from R<1 sampling)
            valid = (
                valid_all[:, p]
                & np.isfinite(ne_all[:, p])
                & np.isfinite(te_all[:, p])
                & np.isfinite(b_all[:, p])
            )
            if not np.any(valid):
                continue
            cnt = int(np.count_nonzero(valid))
            Parms_M[0, :cnt, p] = ds_all[:, p][valid]
            Parms_M[1, :cnt, p] = te_all[:, p][valid]
            Parms_M[2, :cnt, p] = ne_all[:, p][valid]
            Parms_M[3, :cnt, p] = b_all[:, p][valid]
            Parms_M[4, :cnt, p] = theta_all[:, p][valid]
            Parms_M[5, :cnt, p] = phi_all[:, p][valid]
            if s_input_on:
                Parms_M[14, :cnt, p] = s_all[:, p][valid] * pixel_area_cm2
            else:
                Parms_M[14, :cnt, p] = 0.0

        Lparms_M = cp.zeros(6, dtype=cp.int32, order='F')
        Lparms_M[0] = n_rays
        Lparms_M[1] = n_rec
        Lparms_M[2] = Nf
        Lparms_M[3] = 1

        Rparms_M = cp.zeros((3, n_rays), dtype=cp.float64, order='F')
        Rparms_M[0, :] = pixel_area_cm2
        Rparms_M[1, :] = freq0
        Rparms_M[2, :] = freq_log_step

        Parms_M_cp = cp.array(np.asfortranarray(Parms_M), dtype=cp.float64, order='F', copy=True)
        dummy = cp.asarray(0, dtype=cp.float64)
        RL_M = cp.zeros((7, Nf, n_rays), dtype=cp.float64, order='F')

        status = get_mw_slice(
            Lparms_M, Rparms_M, Parms_M_cp, dummy, dummy, dummy, RL_M,
            tile_pixels=256, heap_bytes=2 * 1024 * 1024 * 1024,
        )
        if np.any(status != 0) and verbose:
            bad = np.where(status != 0)[0]
            print(f"fastGRFF: warning {bad.size} pixels returned non-zero status")

        RL_M_np = cp.asnumpy(RL_M)
        intensity = (RL_M_np[5] + RL_M_np[6]).T  # (Npix, Nf)
        denom = RL_M_np[5] + RL_M_np[6]
        pol_vi = np.where(denom != 0, (RL_M_np[5] - RL_M_np[6]) / (denom + 1e-30), 0.0).T
        nu_ghz = RL_M_np[0].T  # (Npix, Nf)

        emission_flat = np.zeros((n_rays, Nf), dtype=float)
        for ifreq in range(Nf):
            nu_hz = np.where(nu_ghz[:, ifreq] > 0, nu_ghz[:, ifreq] * 1e9, frequencies_Hz[ifreq])
            conversion_factor = (sfu2cgs * c * c / (2.0 * kb * nu_hz * nu_hz) / pixel_area_cm2) * (AU_cm * AU_cm)
            emission_flat[:, ifreq] = intensity[:, ifreq] * conversion_factor

        emission_cube[:, :, :] = emission_flat.reshape(N_pix, N_pix, Nf)
        emission_polVI_cube[:, :, :] = pol_vi.reshape(N_pix, N_pix, Nf)
        # Avoid NaNs in map (e.g. from GRFF or conversion when ray/sampling near surface)
        emission_cube[:, :, :] = np.nan_to_num(emission_cube, nan=0.0, posinf=0.0, neginf=0.0)
    else:
        p_iter = tqdm(range(n_rays), desc="GRFF pixels", disable=not verbose, unit="px")
        for p in p_iter:
            i, j = p // N_pix, p % N_pix
            # Require finite ne/te/b (avoids NaN near disk center from R<1 or ray-surface sampling)
            valid = (
                valid_all[:, p]
                & np.isfinite(ne_all[:, p])
                & np.isfinite(te_all[:, p])
                & np.isfinite(b_all[:, p])
            )
            if not np.any(valid):
                emission_cube[i, j, :] = 0.0
                continue
            ne_ray = ne_all[:, p][valid]
            te_ray = te_all[:, p][valid]
            b_ray = b_all[:, p][valid]
            ds_ray = ds_all[:, p][valid]
            theta_ray = theta_all[:, p][valid]
            phi_ray = phi_all[:, p][valid]
            S_valid = s_all[:, p][valid]
            n_pts = len(ne_ray)

            N_valid = n_pts
            Parms = np.zeros((GRFF_PARMS_EXT_SIZE, N_valid), dtype='double', order='F')
            for k in range(N_valid):
                s_row = S_valid[k] * pixel_area_cm2 if s_input_on else 0.0
                fill_grff_parms_ext_column(
                    Parms,
                    k,
                    ds_ray[k],
                    te_ray[k],
                    ne_ray[k],
                    b_ray[k],
                    s_cm2=float(s_row),
                    dist_e=grff_dist_e,
                    kappa=grff_kappa,
                    theta_deg=float(theta_ray[k]),
                    phi_deg=float(phi_ray[k]),
                    mech_flag=mech_flag,
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
                    continue
                for ifreq in range(Nf):
                    nu_GHz = RL[0, ifreq]
                    nu_Hz = frequencies_Hz[ifreq] if nu_GHz <= 0 else nu_GHz * 1e9
                    tb_k, vi = rl_stokes_to_tb_vi(RL, ifreq, nu_Hz, Rparms[0])
                    emission_cube[i, j, ifreq] = tb_k
                    emission_polVI_cube[i, j, ifreq] = vi
            except Exception as e:
                if verbose:
                    print(f"  Error pixel ({i},{j}): {e}")
                emission_cube[i, j, :] = 0.0

    if verbose:
        print("Ray-tracing emission complete.")
    # emission_cube: brightness temperature T_b in K (CGS conversion applied)
    # Replace any remaining NaN (e.g. disk center, R<1 sampling) with 0
    emission_cube = np.nan_to_num(emission_cube, nan=0.0, posinf=0.0, neginf=0.0)

    beam_fwhm_rsun_applied = None
    beam_meta = {}
    if beam_diameter_m is not None:
        beam_meta = beam_fwhm_from_lambda_over_d(
            freq_hz, beam_diameter_m, fwhm_factor=beam_fwhm_factor
        )
        beam_fwhm_rsun_applied = beam_meta["fwhm_rsun"]
        if verbose:
            print(
                f"Beam λ/D: D={beam_diameter_m:.3g} m, ν={freq_hz/1e6:.3f} MHz — "
                f"{format_beam_summary(beam_meta, x_fov_rsun=X_fov)}"
            )
    elif beam_fwhm_rsun is not None:
        beam_fwhm_rsun_applied = float(beam_fwhm_rsun)
        if verbose:
            print(f"Convolving T_b with Gaussian beam FWHM {beam_fwhm_rsun_applied:.6g} R_sun")
    if beam_fwhm_rsun_applied is not None:
        emission_cube = convolve_tb_gaussian_beam(
            emission_cube, x_coords, beam_fwhm_rsun=beam_fwhm_rsun_applied
        )

    result = {
        'emission_cube': emission_cube,       # T_b (K), shape (N_pix, N_pix, Nf)
        'emission_polVI_cube': emission_polVI_cube,
        'frequencies_Hz': frequencies_Hz,
        'x_coords': x_coords,
        'y_coords': y_coords,
    }
    if beam_fwhm_rsun_applied is not None:
        result['beam_fwhm_rsun'] = np.float64(beam_fwhm_rsun_applied)
        if beam_diameter_m is not None:
            result['beam_diameter_m'] = np.float64(beam_diameter_m)
            result['beam_fwhm_factor'] = np.float64(beam_fwhm_factor)
            result['beam_fwhm_arcsec'] = np.float64(beam_meta.get("fwhm_arcsec", np.nan))
            result['beam_fwhm_arcmin'] = np.float64(beam_meta.get("fwhm_arcmin", np.nan))
    np.savez_compressed(out_path, **result)
    if verbose:
        print(f"Saved {out_path}")

    if save_plots:
        _save_emission_plot(
            result,
            N_pix,
            X_fov,
            R_sun_m,
            out_path,
            verbose,
            beam_fwhm_rsun_applied,
            plot_log_norm=plot_log_norm,
            plot_vmin=plot_vmin,
            plot_vmax=plot_vmax,
            plot_beam=plot_beam,
        )
        _save_center_pixel_plots(
            prepared, N_pix, out_path, verbose,
        )
    return result


def _save_center_pixel_plots(sampled, N_pix, out_path, verbose):
    """Plot Ne, Te, B, and S along the ray for the center pixel (inspection)."""
    p_center = (int(N_pix*0.7) // 2) * N_pix + ((N_pix-1) // 2)
    valid = sampled['valid_mask'][:, p_center]
    if not np.any(valid):
        if verbose:
            print("Center pixel has no valid ray points; skipping center-pixel plot.")
        return
    ne_c = sampled['ne'][:, p_center][valid]
    te_c = sampled['te'][:, p_center][valid]
    b_c = sampled['b'][:, p_center][valid]
    S_valid = sampled['s'][:, p_center][valid]
    ds_c = sampled['ds'][:, p_center][valid]
    dist_cm = np.cumsum(ds_c.astype(float))
    dist_Rsun = dist_cm / R_sun_cm

    fig, axes = plt.subplots(2, 2, figsize=(10, 8))
    axes[0, 0].plot(dist_Rsun, ne_c, 'b-', linewidth=1.5)
    axes[0, 0].set_xlabel('Distance along ray (R_sun)')
    axes[0, 0].set_ylabel('N_e (cm$^{-3}$)')
    axes[0, 0].set_yscale('log')
    axes[0, 0].set_title('Center pixel: N_e along ray')
    axes[0, 0].grid(True, alpha=0.3)

    axes[0, 1].plot(dist_Rsun, te_c, 'r-', linewidth=1.5)
    axes[0, 1].set_xlabel('Distance along ray (R_sun)')
    axes[0, 1].set_ylabel('T_e (K)')
    axes[0, 1].set_yscale('log')
    axes[0, 1].set_title('Center pixel: T_e along ray')
    axes[0, 1].grid(True, alpha=0.3)

    axes[1, 0].plot(dist_Rsun, b_c, 'green', linewidth=1.5)
    axes[1, 0].set_xlabel('Distance along ray (R_sun)')
    axes[1, 0].set_ylabel('|B| (G)')
    axes[1, 0].set_yscale('log')
    axes[1, 0].set_title('Center pixel: |B| along ray')
    axes[1, 0].grid(True, alpha=0.3)

    axes[1, 1].plot(dist_Rsun, S_valid, 'k-', linewidth=1.5)
    axes[1, 1].axhline(1.0, color='gray', linestyle='--', alpha=0.7)
    axes[1, 1].set_xlabel('Distance along ray (R_sun)')
    axes[1, 1].set_ylabel('S (cross-section ratio)')
    axes[1, 1].set_title('Center pixel: S along ray')
    axes[1, 1].grid(True, alpha=0.3)

    plt.tight_layout()
    plot_path = Path(out_path).with_name(
        Path(out_path).stem + '_center_pixel.png'
    )
    plt.savefig(plot_path, dpi=150, bbox_inches='tight')
    plt.close()
    if verbose:
        print(f"Center-pixel inspection plot saved to {plot_path}")


def _save_emission_plot(
    result,
    N_pix,
    X_fov,
    R_sun_m,
    out_path,
    verbose,
    beam_fwhm_rsun_applied,
    plot_log_norm=False,
    plot_vmin=None,
    plot_vmax=None,
    plot_beam=True,
):
    emission_cube = result['emission_cube']
    x_coords = result['x_coords']
    y_coords = result['y_coords']
    frequencies_Hz = result['frequencies_Hz']
    emission_map = np.array(emission_cube[:, :, 0], dtype=float, copy=True)
    # Interpolate bad points for display (NaN/Inf), preserving valid pixels.
    if np.any(~np.isfinite(emission_map)):
        emission_map[~np.isfinite(emission_map)] = np.nan
        emission_map = patch_nan_emission_map(emission_map, inplace=False)
    emission_map = np.nan_to_num(emission_map, nan=0.0, posinf=0.0, neginf=0.0)

    x_range = [x_coords[0] / R_sun_m, x_coords[-1] / R_sun_m]
    y_range = [y_coords[0] / R_sun_m, y_coords[-1] / R_sun_m]

    fig, ax = plt.subplots(figsize=(6, 4.8))
    if sunpy_ct is not None:
        try:
            cmap_use = sunpy_ct.xrt_color_table()
        except Exception:
            cmap_use = 'inferno'
    else:
        cmap_use = 'inferno'

    if plot_log_norm and plot_vmin is not None and plot_vmax is not None:
        from matplotlib.colors import LogNorm

        lo, hi = float(plot_vmin), float(plot_vmax)
        if lo <= 0 or hi <= lo:
            raise ValueError("plot_log_norm requires plot_vmin > 0 and plot_vmax > plot_vmin")
        disp = np.clip(np.asarray(emission_map, dtype=float), lo, hi)
        norm = LogNorm(vmin=lo, vmax=hi, clip=True)
        im = ax.imshow(
            disp,
            origin="lower",
            extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
            aspect="equal",
            cmap=cmap_use,
            interpolation="bilinear",
            norm=norm,
        )
    else:
        vmax_plot = (
            float(plot_vmax)
            if plot_vmax is not None
            else float(np.nanmax(emission_map) * 1.1)
        )
        vmin_plot = 0.0 if plot_vmin is None else float(plot_vmin)
        im = ax.imshow(
            emission_map,
            origin="lower",
            extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
            aspect="equal",
            cmap=cmap_use,
            interpolation="bilinear",
            vmin=vmin_plot,
            vmax=vmax_plot,
        )
    ax.set_xlabel('x (R_sun)')
    ax.set_ylabel('y (R_sun)')
    ax.set_title(f'Ray-tracing emission T_b at {frequencies_Hz[0]/1e9:.3f} GHz')

    beam_rsun = beam_fwhm_rsun_applied
    if beam_rsun is None and "beam_fwhm_rsun" in result:
        beam_rsun = float(result["beam_fwhm_rsun"])
    if plot_beam and beam_rsun is not None and beam_rsun > 0:
        margin = 0.06 * (x_range[1] - x_range[0])
        cx = x_range[0] + float(beam_rsun) + margin
        cy = y_range[0] + float(beam_rsun) + margin
        ax.add_patch(
            plt.Circle(
                (cx, cy),
                float(beam_rsun),
                edgecolor="white",
                facecolor="none",
                linewidth=1.5,
                linestyle="-",
            )
        )

    plt.colorbar(im, ax=ax, label='T_b (K)')
    plt.tight_layout()
    plot_path = Path(out_path).with_suffix('.png')
    plt.savefig(plot_path, dpi=150, bbox_inches='tight')
    plt.close()
    if verbose:
        print(f"Plot saved to {plot_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Ray-tracing emission map: resample MAS along rays and run GRFF.')
    parser.add_argument('--model-path', '-m', type=str, default='./corona',
                        help='MAS model directory (default: ./corona)')
    parser.add_argument('--N-pix', '-n', type=int, default=32,
                        help='Image size N_pix x N_pix (default: 64)')
    parser.add_argument('--X-FOV', '-f', type=float, default=1.44,
                        help='Half FOV in R_sun (default: 1.44)')
    parser.add_argument('--freq', type=float, default=75e6,
                        help='Ray frequency in Hz (default: 75e6)')
    parser.add_argument('--grid-n', type=int, default=128,
                        help='3D grid points per axis (default: 128)')
    parser.add_argument('--grid-extent', type=float, default=3.0,
                        help='3D grid extent in R_sun (default: 3)')
    parser.add_argument('--z-observer', type=float, default=3.0,
                        help='Ray start z in R_sun (default: 3)')
    parser.add_argument('--dt', type=float, default=6e-3,
                        help='Ray integrator dt (default: 6e-3)')
    parser.add_argument('--n-steps', type=int, default=5000,
                        help='Ray integration steps (default: 5000)')
    parser.add_argument('--record-stride', type=int, default=10,
                        help='Record every N steps (default: 10)')
    parser.add_argument('--workers', '-w', type=int, default=1,
                        help='Number of processes for parallel ray tracing (default: 1)')
    parser.add_argument('--out-path', '-o', type=str, default='ray_tracing_emission.npz',
                        help='Output npz path (default: ray_tracing_emission.npz)')
    parser.add_argument('--grff-lib', type=str, default=GRFF_LIB,
                        help=f'GRFF library path (default: {GRFF_LIB})')
    parser.add_argument('--grff-backend', type=str, default='get_mw', choices=['get_mw', 'fastgrff'],
                        help="GRFF backend: 'get_mw' (default) or 'fastgrff' (GPU)")
    parser.add_argument('--s-input-on', action='store_true',
                        help='Pass cross-section ratio S in Parms[14]; otherwise use 0')
    parser.add_argument('--device', type=str, default='cpu', choices=['cpu', 'cuda'],
                        help="LOS sampling device: 'cpu' (default) or 'cuda'")
    parser.add_argument('--raytrace-device', type=str, default='cpu', choices=['cpu', 'cuda'],
                        help="Ray integration device: 'cpu' (default) or 'cuda'")

    parser.add_argument('--beam-fwhm-rsun', type=float, default=None,
                        help='Gaussian beam FWHM in R_sun (e.g. 0.1)')
    parser.add_argument('--beam-diameter-m', type=float, default=None,
                        help='Telescope diameter D (m): θ=beam-fwhm-factor*λ/D at channel frequency')
    parser.add_argument('--beam-fwhm-factor', type=float, default=1.22,
                        help='θ = factor*λ/D (default 1.22 HPBW; 1.0 for Gaussian λ/D)')
    parser.add_argument('--phi0-offset', type=float, default=0,
                        help='Longitude offset in degrees added to MAS φ; disk center is Carrington phi0 (default: 0)')
    parser.add_argument(
        '--grff-dist-e',
        type=float,
        default=0.0,
        help='GRFF Dist_E: 0 Maxwellian (default), 1 kappa distribution, 2 n-distribution',
    )
    parser.add_argument(
        '--grff-kappa',
        type=float,
        default=0.0,
        help='GRFF kappa index (Parms[16]); used when --grff-dist-e is 1 or 2',
    )
    parser.add_argument('--no-fallback', action='store_true',
                        help='If --device cuda fails, do not fall back to cpu')
    parser.add_argument('--no-plots', action='store_true', help='Do not save plot')
    parser.add_argument(
        '--plot-beam',
        action=argparse.BooleanOptionalAction,
        default=True,
        help='Draw beam FWHM circle at lower left when a beam is set (default: on)',
    )
    parser.add_argument('--quiet', '-q', action='store_true', help='Less output')
    args = parser.parse_args()

    run_ray_tracing_emission(
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
        s_input_on=args.s_input_on,
        out_path=args.out_path,
        grff_lib=args.grff_lib,
        Nfreq=1,
        freq0=args.freq,
        freq_log_step=0.0,
        save_plots=not args.no_plots,
        verbose=not args.quiet,
        device=args.device,
        fallback_to_cpu=not args.no_fallback,
        raytrace_device=args.raytrace_device,
        grff_backend=args.grff_backend,
        beam_fwhm_rsun=args.beam_fwhm_rsun,
        beam_diameter_m=args.beam_diameter_m,
        beam_fwhm_factor=args.beam_fwhm_factor,
        phi0_offset=args.phi0_offset,
        plot_beam=args.plot_beam,
        grff_dist_e=args.grff_dist_e,
        grff_kappa=args.grff_kappa,
    )


if __name__ == '__main__':
    main()
