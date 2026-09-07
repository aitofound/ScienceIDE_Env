"""
GRFF external voxel parameter layout (PyGET_MW / GET_MW).

Matches GRFF `InSize_ext` (see GRFF/source/MWtransfer.h): 17 doubles per voxel,
Fortran column-major shape (17, Nz).

Indices follow GRFF getparms `arr3` (see GRFF/source/getparms.cpp):
  0  dR (segment length, cm)
  1  T_0 (K)
  2  n_0 (cm^-3)
  3  B (G)
  4  theta (deg)
  5  phi (deg)
  6  mech_flag
  7  s_max
  8  n_p
  9  n_HI
  10 n_HeI
  11 DEM_key_loc
  12 DDM_key_loc
  13 abund_key
  14 S_loc (source area, cm^2)
  15 Dist_E (0 Maxwellian, 1 kappa, 2 n-distribution)
  16 kappa (index for kappa- or n-distribution; Maxwellian: use 0)
"""

from __future__ import annotations

import numpy as np

from .coords import cart_to_sph

GRFF_PARMS_EXT_SIZE = 17
# mech_flag bitmask (GRFF MWtransfer.cpp): bit0=GR off, bit1=FF off, bit2=HHe off, bit3=force isothermal
MECH_FLAG_FF_ONLY = 5.0   # FF on, GR off (legacy LOS default)
MECH_FLAG_FF_GR = 4.0     # FF + gyrosynchrotron on, HHe off


def mas_spherical_b_to_cartesian(x, y, z, br, bt, bp):
    """
    Convert MAS (br, bt, bp) at HCC position (x, y, z) to HCC Cartesian B.

    HCC: +x west, +y north, +z toward the observer. Vector basis uses geometric
    angles from ``cart_to_sph`` (``phi0_offset=0``); Carrington rotation is
    applied only when sampling the model, not when projecting components.
    """
    x = np.asarray(x, dtype=np.float64)
    y = np.asarray(y, dtype=np.float64)
    z = np.asarray(z, dtype=np.float64)
    br = np.asarray(br, dtype=np.float64)
    bt = np.asarray(bt, dtype=np.float64)
    bp = np.asarray(bp, dtype=np.float64)

    _, colat, lon = cart_to_sph(x, y, z, phi0_offset=0.0)
    sin_c = np.sin(colat)
    cos_c = np.cos(colat)
    sin_l = np.sin(lon)
    cos_l = np.cos(lon)

    # ê_r, ê_θ, ê_φ in HCC
    er_x = sin_c * sin_l
    er_y = cos_c
    er_z = sin_c * cos_l
    et_x = cos_c * sin_l
    et_y = -sin_c
    et_z = cos_c * cos_l
    ep_x = cos_l
    ep_y = 0.0
    ep_z = -sin_l

    bx = br * er_x + bt * et_x + bp * ep_x
    by = br * er_y + bt * et_y + bp * ep_y
    bz = br * er_z + bt * et_z + bp * ep_z
    return bx, by, bz


def grff_angles_k_vec_to_b_vec(kx, ky, kz, bx, by, bz):
    """
    GRFF Parms[4]=theta (deg), Parms[5]=phi (deg): viewing angles between ray direction k and B.

    k should point along the radiative-transfer integration direction (toward the observer).
    """
    k = np.stack(
        [np.asarray(kx, dtype=np.float64), np.asarray(ky, dtype=np.float64), np.asarray(kz, dtype=np.float64)],
        axis=-1,
    )
    b = np.stack(
        [np.asarray(bx, dtype=np.float64), np.asarray(by, dtype=np.float64), np.asarray(bz, dtype=np.float64)],
        axis=-1,
    )
    kn = np.linalg.norm(k, axis=-1)
    bn = np.linalg.norm(b, axis=-1)
    theta_deg = np.full(k.shape[:-1], 90.0, dtype=np.float64)
    phi_deg = np.zeros(k.shape[:-1], dtype=np.float64)

    valid = (kn > 1e-30) & (bn > 1e-30)
    if not np.any(valid):
        return theta_deg, phi_deg

    k_hat = np.zeros_like(k)
    b_hat = np.zeros_like(b)
    k_hat[valid] = k[valid] / kn[valid, None]
    b_hat[valid] = b[valid] / bn[valid, None]

    cos_theta = np.clip(np.sum(k_hat * b_hat, axis=-1), -1.0, 1.0)
    theta_deg[valid] = np.degrees(np.arccos(cos_theta[valid]))

    ref = np.broadcast_to(np.array([0.0, 0.0, 1.0]), k_hat.shape).copy()
    align_z = np.abs(k_hat[..., 2]) > 0.9
    ref[align_z] = np.array([0.0, 1.0, 0.0])
    x_ax = np.cross(ref, k_hat)
    x_norm = np.linalg.norm(x_ax, axis=-1)
    ok = valid & (x_norm > 1e-30)
    x_ax[ok] /= x_norm[ok, None]
    y_ax = np.cross(k_hat, x_ax)
    phi_deg[ok] = np.degrees(np.arctan2(
        np.sum(b_hat[ok] * y_ax[ok], axis=-1),
        np.sum(b_hat[ok] * x_ax[ok], axis=-1),
    ))
    return theta_deg, phi_deg


def _recompute_ds_and_k_toward_observer(positions, valid_mask, r_sun_cm):
    """Segment lengths and unit k along deep-sun -> observer voxel order."""
    pos = np.asarray(positions, dtype=np.float64)
    valid = np.asarray(valid_mask, dtype=bool)
    n_steps, n_rays, _ = pos.shape
    ds = np.zeros((n_steps, n_rays), dtype=np.float64)
    k_unit = np.zeros((n_steps, n_rays, 3), dtype=np.float64)

    for r in range(n_rays):
        idx = np.flatnonzero(valid[:, r])
        if idx.size == 0:
            continue
        p = pos[idx, r, :]
        if idx.size == 1:
            ds[idx[0], r] = 0.0
            continue
        seg = np.linalg.norm(np.diff(p, axis=0), axis=1) * float(r_sun_cm)
        ds[idx[1:], r] = seg
        ds[idx[0], r] = seg[0]
        d = np.diff(p, axis=0)
        kn = np.linalg.norm(d, axis=1, keepdims=True)
        ku = np.zeros_like(d)
        m = (kn[:, 0] > 1e-30)
        ku[m] = d[m] / kn[m]
        k_unit[idx[:-1], r, :] = ku
        k_unit[idx[-1], r, :] = ku[-1] if ku.shape[0] else 0.0

    return ds, k_unit


def prepare_ray_voxels_for_grff(sampled, r_record, r_sun_cm):
    """
    Flip along-ray samples so GRFF integrates from the Sun-side toward the observer.

    Expects ``sampled`` to include ``bx``, ``by``, ``bz`` (image-frame Cartesian B).
    Returns a dict with reversed arrays plus ``theta_deg`` and ``phi_deg`` per voxel.
    """
    r_rev = np.asarray(r_record, dtype=np.float64)[::-1, :, :].copy()
    out = {}
    for key in ("ne", "te", "ds", "s", "bx", "by", "bz", "b"):
        if key in sampled:
            out[key] = np.asarray(sampled[key], dtype=np.float64)[::-1, :].copy()
    out["valid_mask"] = np.asarray(sampled["valid_mask"], dtype=bool)[::-1, :].copy()

    out["ds"], k_unit = _recompute_ds_and_k_toward_observer(r_rev, out["valid_mask"], r_sun_cm)
    out["theta_deg"], out["phi_deg"] = grff_angles_k_vec_to_b_vec(
        k_unit[..., 0], k_unit[..., 1], k_unit[..., 2],
        out["bx"], out["by"], out["bz"],
    )
    if "b" not in out:
        out["b"] = np.sqrt(out["bx"] ** 2 + out["by"] ** 2 + out["bz"] ** 2)
    return out


def fill_grff_parms_ext_column(
    Parms: np.ndarray,
    k: int,
    ds_cm: float,
    te_k: float,
    ne_cm3: float,
    b_g: float,
    *,
    s_cm2: float = 0.0,
    dist_e: float = 0.0,
    kappa: float = 0.0,
    theta_deg: float = 90.0,
    phi_deg: float = 0.0,
    mech_flag: float = MECH_FLAG_FF_ONLY,
    s_max: float = 30.0,
) -> None:
    """Fill one column k of Parms (shape (17, Nz), Fortran order)."""
    if Parms.shape[0] != GRFF_PARMS_EXT_SIZE:
        raise ValueError(f"Parms first dim must be {GRFF_PARMS_EXT_SIZE}, got {Parms.shape[0]}")
    Parms[0, k] = ds_cm
    Parms[1, k] = te_k
    Parms[2, k] = ne_cm3
    Parms[3, k] = b_g
    Parms[4, k] = theta_deg
    Parms[5, k] = phi_deg
    Parms[6, k] = mech_flag
    Parms[7, k] = s_max
    Parms[8, k] = 0.0
    Parms[9, k] = 0.0
    Parms[10, k] = 0.0
    Parms[11, k] = 0.0
    Parms[12, k] = 0.0
    Parms[13, k] = 0.0
    Parms[14, k] = s_cm2
    Parms[15, k] = float(dist_e)
    Parms[16, k] = float(kappa)


# Brightness-temperature conversion (matches synthetic_FF_map / ray-tracing workflows)
C_LIGHT_CM_S = 2.998e10
KB_ERG_K = 1.38065e-16
SFU_TO_CGS = 1e-19
AU_CM = 1.49599e13


def rl_stokes_to_tb_vi(
    RL: np.ndarray,
    ifreq: int,
    nu_hz: float,
    pixel_area_cm2: float,
    *,
    distance_cm: float = AU_CM,
) -> tuple[float, float]:
    """
    Convert GRFF ``RL`` Stokes row to brightness temperature (K) and circular V/I.

    Returns ``(tb_k, vi)``; ``vi`` is ``nan`` when the Stokes sum is invalid or |V/I| > 1.
    """
    denom = float(RL[5, ifreq] + RL[6, ifreq])
    if denom <= 0.0 or not np.isfinite(denom):
        return 0.0, np.nan
    conv = (
        SFU_TO_CGS
        * C_LIGHT_CM_S**2
        / (2.0 * KB_ERG_K * nu_hz * nu_hz)
        / pixel_area_cm2
    ) * (distance_cm * distance_cm)
    tb_k = denom * conv
    vi = (RL[5, ifreq] - RL[6, ifreq]) / denom
    if not np.isfinite(vi) or abs(vi) > 1.0 + 1e-9:
        vi = np.nan
    return tb_k, float(vi)


def vi_plot_vmax(pol_vi: np.ndarray, tb: np.ndarray, percentile: float = 99.0) -> float:
    """Robust |V/I| color-scale limit; masks zero-Tb pixels like legacy plotting."""
    plot = pol_vi.copy()
    plot[tb == 0] = np.nan
    abs_vi = np.abs(plot[np.isfinite(plot)])
    if abs_vi.size == 0:
        return 1.0
    vmax = float(np.percentile(abs_vi, percentile))
    return vmax if np.isfinite(vmax) and vmax > 0 else 1.0
