"""
Heliocentric Cartesian (HCC) ↔ MAS spherical coordinates.

Cartesian frame used everywhere in this repo (right-handed):

    +x  solar west
    +y  projected solar north
    +z  toward the observer

Sky-plane ``(x, y)`` is helioprojective (Tx, Ty) in the same units as ``z``
(usually ``R_sun``). Rays launch from observer-side ``+z`` toward ``-z`` at
each ``(x, y)``.

MAS / PSI spherical coordinates: radius ``r``, co-latitude ``θ`` (0 at the
north pole), Carrington longitude ``φ``. Ignoring solar B0 (projected north
equals true north):

    x = r sinθ sinφ
    y = r cosθ
    z = r sinθ cosφ

so PSI Cartesian ``(X, Y, Z)`` with ``+Z`` = solar north is ``(z, x, y)``.

``phi0_offset`` (degrees) is added to ``φ``. Disk center ``(x=0, y=0, z>0)``
is sampled at Carrington longitude ``phi0_offset``. Earth-aligned maps use
``phi0_offset ≈ L0`` (SunPy ``sun.L0``).
"""

from __future__ import annotations

import numpy as np

# corona2298 at 2025-06-08 20:07 UTC: L0 ≈ 141.3°.
# Equivalent to the old pub default -129° under the former (x, -z, y) permutation,
# which placed disk center at φ = -90° + phi0.
PHI0_EARTH_CORONA2298 = 141.0


def cart_to_sph(x, y, z, phi0_offset=0.0):
    """Convert HCC Cartesian ``(x, y, z)`` to spherical ``(r, colat, lon)``.

    ``colat`` and ``lon`` are radians; ``lon`` is in ``[0, 2π)`` and already
    includes ``phi0_offset`` (degrees).
    """
    x = np.asarray(x)
    y = np.asarray(y)
    z = np.asarray(z)
    r = np.sqrt(x * x + y * y + z * z)
    colat = np.arccos(np.clip(y / np.maximum(r, 1e-30), -1.0, 1.0))
    lon = np.arctan2(x, z) + np.deg2rad(phi0_offset)
    lon = np.mod(lon, 2.0 * np.pi)
    return r, colat, lon


def sph_to_cart(r, colat, lon, phi0_offset=0.0):
    """Inverse of :func:`cart_to_sph`. ``colat``/``lon`` in radians."""
    r = np.asarray(r)
    colat = np.asarray(colat)
    lon = np.asarray(lon) - np.deg2rad(phi0_offset)
    sin_c = np.sin(colat)
    x = r * sin_c * np.sin(lon)
    y = r * np.cos(colat)
    z = r * sin_c * np.cos(lon)
    return x, y, z


def cart_to_mas_lonlat(x, y, z, phi0_offset=0.0):
    """Return ``(lon_deg, lat_deg, r)`` for ``psipy.Variable.sample_at_coords``."""
    r, colat, lon = cart_to_sph(x, y, z, phi0_offset=phi0_offset)
    lat_deg = np.rad2deg(0.5 * np.pi - colat)
    lon_deg = np.rad2deg(lon)
    return lon_deg, lat_deg, r
