import numpy as np

from raytracingGRFF.coords import cart_to_mas_lonlat, cart_to_sph, sph_to_cart
from raytracingGRFF.grff_parms import mas_spherical_b_to_cartesian


def test_disk_center_is_equator_observer_meridian():
    r, colat, lon = cart_to_sph(0.0, 0.0, 2.0, phi0_offset=0.0)
    assert abs(r - 2.0) < 1e-12
    assert abs(colat - 0.5 * np.pi) < 1e-12
    assert abs(lon) < 1e-12


def test_disk_center_phi0_is_carrington_l0():
    _, _, lon = cart_to_sph(0.0, 0.0, 1.0, phi0_offset=141.0)
    assert abs(np.rad2deg(lon) - 141.0) < 1e-9


def test_north_pole_is_plus_y():
    r, colat, lon = cart_to_sph(0.0, 1.0, 0.0)
    assert abs(r - 1.0) < 1e-12
    assert abs(colat) < 1e-12


def test_west_limb_is_plus_x():
    r, colat, lon = cart_to_sph(1.0, 0.0, 0.0)
    assert abs(r - 1.0) < 1e-12
    assert abs(colat - 0.5 * np.pi) < 1e-12
    assert abs(lon - 0.5 * np.pi) < 1e-12


def test_sph_cart_roundtrip():
    rng = np.random.default_rng(0)
    x, y, z = rng.normal(size=(3, 50))
    r, colat, lon = cart_to_sph(x, y, z, phi0_offset=37.0)
    x2, y2, z2 = sph_to_cart(r, colat, lon, phi0_offset=37.0)
    np.testing.assert_allclose(x2, x, atol=1e-12)
    np.testing.assert_allclose(y2, y, atol=1e-12)
    np.testing.assert_allclose(z2, z, atol=1e-12)


def test_mas_lonlat_disk_center():
    lon_deg, lat_deg, r = cart_to_mas_lonlat(0.0, 0.0, 1.5, phi0_offset=141.0)
    assert abs(r - 1.5) < 1e-12
    assert abs(lat_deg) < 1e-12
    assert abs(lon_deg - 141.0) < 1e-9


def test_radial_b_follows_position():
    bx, by, bz = mas_spherical_b_to_cartesian(0.0, 0.0, 1.0, 3.0, 0.0, 0.0)
    np.testing.assert_allclose((bx, by, bz), (0.0, 0.0, 3.0), atol=1e-12)

    bx, by, bz = mas_spherical_b_to_cartesian(0.0, 1.0, 0.0, 3.0, 0.0, 0.0)
    np.testing.assert_allclose((bx, by, bz), (0.0, 3.0, 0.0), atol=1e-12)

    bx, by, bz = mas_spherical_b_to_cartesian(1.0, 0.0, 0.0, 3.0, 0.0, 0.0)
    np.testing.assert_allclose((bx, by, bz), (3.0, 0.0, 0.0), atol=1e-12)


def test_bp_at_disk_center_is_west():
    # ê_φ (increasing Carrington longitude) is solar west = +x at disk center.
    bx, by, bz = mas_spherical_b_to_cartesian(0.0, 0.0, 1.0, 0.0, 0.0, 2.0)
    np.testing.assert_allclose((bx, by, bz), (2.0, 0.0, 0.0), atol=1e-12)
