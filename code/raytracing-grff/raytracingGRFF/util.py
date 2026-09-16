"""Utilities for ray-tracing and emission maps."""

from __future__ import annotations

import numpy as np

R_SUN_M = 6.957e8
AU_M = 1.495978707e11
C_LIGHT_M_S = 299792458.0
ARCSEC_PER_RAD = 206264.80624709636
# scipy.ndimage.gaussian_filter sigma vs circular Gaussian FWHM (pixels)
_GAUSS_FWHM_TO_SIGMA = 2.0 * np.sqrt(2.0 * np.log(2.0))
# Solar angular radius at 1 AU; 1 R_sun on map axes = this many arcmin on the sky
SUN_ANGULAR_RADIUS_ARCMIN = 16.0
SUN_ANGULAR_DIAMETER_ARCMIN = 2.0 * SUN_ANGULAR_RADIUS_ARCMIN
# Radians on sky → projected map coordinate R_sun (x,y in units of solar radii)
MAP_RSUN_PER_RADIAN = AU_M / R_SUN_M


def beam_fwhm_from_lambda_over_d(
    freq_hz: float,
    diameter_m: float,
    *,
    fwhm_factor: float = 1.22,
    r_sun_m: float = R_SUN_M,
) -> dict[str, float]:
    """
    Theoretical Gaussian/HPBW-style beam from θ_FWHM ≈ ``fwhm_factor * λ / D``.

    Chain (small-angle at 1 AU):
      λ = c / ν
      θ [rad] on sky = fwhm_factor * λ / D
      FWHM on map [R_sun] = θ × (AU / R_sun) = θ × ``MAP_RSUN_PER_RADIAN``

    Map x,y are **projected solar radii** (1 = photospheric radius ≈ 16′ on the sky).

    ``fwhm_factor`` is often 1.22 (uniform disk HPBW) or 1.0 (Gaussian FWHM ≈ λ/D).
    """
    if freq_hz <= 0 or diameter_m <= 0:
        raise ValueError("freq_hz and diameter_m must be positive")
    wavelength_m = C_LIGHT_M_S / float(freq_hz)
    theta_rad = float(fwhm_factor) * wavelength_m / float(diameter_m)
    fwhm_rsun_map = theta_rad * MAP_RSUN_PER_RADIAN
    fwhm_arcsec = theta_rad * ARCSEC_PER_RAD
    fwhm_arcmin = fwhm_arcsec / 60.0
    return {
        "theta_rad": theta_rad,
        "fwhm_rsun": fwhm_rsun_map,
        "fwhm_arcsec": fwhm_arcsec,
        "fwhm_arcmin": fwhm_arcmin,
        "wavelength_m": wavelength_m,
        "diameter_m": float(diameter_m),
        "fwhm_factor": float(fwhm_factor),
        "map_rsun_per_radian": MAP_RSUN_PER_RADIAN,
    }


def format_beam_summary(beam_meta: dict[str, float], *, x_fov_rsun: float | None = None) -> str:
    """One-line beam summary: map R_sun, arcmin on sky, vs 16′ solar radius."""
    rs = beam_meta["fwhm_rsun"]
    am = beam_meta.get("fwhm_arcmin", beam_meta["fwhm_arcsec"] / 60.0)
    parts = [
        f"FWHM={rs:.4g} R_sun (map)",
        f"θ={am:.2f}′ ({am / SUN_ANGULAR_RADIUS_ARCMIN:.3f}× R_sun=16′)",
    ]
    if x_fov_rsun is not None and x_fov_rsun > 0:
        fov = 2.0 * float(x_fov_rsun)
        parts.append(f"{rs / fov:.3f}× map FOV ({fov:.2g} R_sun)")
    return ", ".join(parts)


def gaussian_beam_sigma_pix(
    beam_fwhm_rsun: float,
    x_coords_m: np.ndarray,
    *,
    r_sun_m: float = R_SUN_M,
) -> float:
    """Gaussian ``sigma`` in pixels for ``ndimage.gaussian_filter`` (FWHM in R_sun)."""
    x_rsun = np.asarray(x_coords_m, dtype=float) / r_sun_m
    if x_rsun.size < 2:
        raise ValueError("x_coords must have at least two points")
    n_pix = x_rsun.size
    fov_rsun = float(x_rsun[-1] - x_rsun[0])
    if fov_rsun <= 0:
        raise ValueError("Invalid FOV from x_coords")
    pix_size_rsun = fov_rsun / (n_pix - 1)
    fwhm_pix = float(beam_fwhm_rsun) / pix_size_rsun
    return fwhm_pix / _GAUSS_FWHM_TO_SIGMA


def convolve_tb_gaussian_beam(
    tb: np.ndarray,
    x_coords_m: np.ndarray,
    *,
    beam_fwhm_rsun: float,
    r_sun_m: float = R_SUN_M,
) -> np.ndarray:
    """
    Convolve brightness-temperature map(s) with a circular Gaussian beam.

    Parameters
    ----------
    tb : ndarray
        2D (ny, nx) or 3D (ny, nx, nf).
    beam_fwhm_rsun : float
        Beam FWHM in R_sun (image-plane angular width at 1 AU).
    """
    from scipy.ndimage import gaussian_filter

    fwhm_rsun = float(beam_fwhm_rsun)

    sigma_pix = gaussian_beam_sigma_pix(fwhm_rsun, x_coords_m, r_sun_m=r_sun_m)
    if sigma_pix < 1e-6:
        import warnings

        warnings.warn(
            f"Beam FWHM is much smaller than one pixel (sigma_pix={sigma_pix:.2e}); "
            "convolution will have negligible effect. Increase resolution or check units.",
            stacklevel=2,
        )
    out = np.asarray(tb, dtype=np.float64)
    if out.ndim == 2:
        return gaussian_filter(out, sigma=sigma_pix)
    if out.ndim == 3:
        for k in range(out.shape[2]):
            out[:, :, k] = gaussian_filter(out[:, :, k], sigma=sigma_pix)
        return out
    raise ValueError("tb must be 2D or 3D")


def patch_nan_emission_map(emission: np.ndarray, inplace: bool = False) -> np.ndarray:
    """Fill NaN pixels using the nearest non-NaN pixel in each of the four directions.

    For each NaN pixel, finds:
    - pix_left: nearest non-NaN to the left (same row, smaller column)
    - pix_right: nearest non-NaN to the right (same row, larger column)
    - pix_up: nearest non-NaN above (same column, larger row)
    - pix_down: nearest non-NaN below (same column, smaller row)

    Then sets: pix_val_new = (pix_left + pix_right + pix_up + pix_down) / 4.
    If a direction has no non-NaN pixel (e.g. at edges), that direction is omitted
    and the average is over the remaining valid neighbors only.

    Parameters
    ----------
    emission : np.ndarray
        Emission map, 2D (ny, nx) or 3D (ny, nx, nf). NaNs are patched per 2D slice.
    inplace : bool, optional
        If True, modify the array in place. Default False.

    Returns
    -------
    np.ndarray
        Patched emission map (same shape as input).
    """
    out = emission if inplace else np.array(emission, copy=True, dtype=np.float64)
    if out.ndim == 2:
        _patch_nan_2d(out)
        return out
    if out.ndim == 3:
        for k in range(out.shape[2]):
            _patch_nan_2d(out[:, :, k])
        return out
    raise ValueError("emission must be 2D or 3D")


def _patch_nan_2d(a: np.ndarray, max_passes: int = 10) -> None:
    """Patch NaN in 2D array in place. a is (n_row, n_col); row 0 = bottom, row increases = up."""
    ny, nx = a.shape
    for _ in range(max_passes):
        nan_mask = ~np.isfinite(a)
        if not np.any(nan_mask):
            return
        rows, cols = np.where(nan_mask)
        fixed = 0
        for i, j in zip(rows, cols):
            neighbors = []
            # left: same row, smaller col
            for jj in range(j - 1, -1, -1):
                if np.isfinite(a[i, jj]):
                    neighbors.append(a[i, jj])
                    break
            # right: same row, larger col
            for jj in range(j + 1, nx):
                if np.isfinite(a[i, jj]):
                    neighbors.append(a[i, jj])
                    break
            # down: same col, smaller row
            for ii in range(i - 1, -1, -1):
                if np.isfinite(a[ii, j]):
                    neighbors.append(a[ii, j])
                    break
            # up: same col, larger row
            for ii in range(i + 1, ny):
                if np.isfinite(a[ii, j]):
                    neighbors.append(a[ii, j])
                    break
            if neighbors:
                a[i, j] = np.mean(neighbors)
                fixed += 1
        if fixed == 0:
            break
