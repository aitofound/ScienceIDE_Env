import numpy as np

from raytracingGRFF.grff_parms import (
    MECH_FLAG_FF_GR,
    grff_angles_k_vec_to_b_vec,
    mas_spherical_b_to_cartesian,
    prepare_ray_voxels_for_grff,
)


def test_mech_flag_ff_gr_enables_both():
    assert MECH_FLAG_FF_GR == 4.0
    em = int(MECH_FLAG_FF_GR)
    assert (em & 1) == 0  # GR on
    assert (em & 2) == 0  # FF on
    assert (em & 4) != 0  # HHe off


def test_viewing_angle_parallel_and_perpendicular():
    theta, phi = grff_angles_k_vec_to_b_vec(0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
    assert abs(theta - 0.0) < 1e-9
    theta, _ = grff_angles_k_vec_to_b_vec(0.0, 0.0, 1.0, 1.0, 0.0, 0.0)
    assert abs(theta - 90.0) < 1e-9


def test_prepare_ray_voxels_flips_observer_to_sun_order():
    # observer at z=2, sun-side at z=0 along -z propagation
    r_record = np.array(
        [
            [[0.0, 0.0, 2.0]],
            [[0.0, 0.0, 1.0]],
            [[0.0, 0.0, 0.5]],
        ],
        dtype=np.float64,
    )
    sampled = {
        "ne": np.array([[1.0], [2.0], [3.0]], dtype=np.float64),
        "te": np.array([[1e6], [1e6], [1e6]], dtype=np.float64),
        "bx": np.array([[0.0], [0.0], [0.0]], dtype=np.float64),
        "by": np.array([[0.0], [0.0], [0.0]], dtype=np.float64),
        "bz": np.array([[1.0], [1.0], [1.0]], dtype=np.float64),
        "b": np.array([[1.0], [1.0], [1.0]], dtype=np.float64),
        "ds": np.array([[1.0], [1.0], [1.0]], dtype=np.float64),
        "s": np.array([[1.0], [1.0], [1.0]], dtype=np.float64),
        "valid_mask": np.array([[True], [True], [True]]),
    }
    prep = prepare_ray_voxels_for_grff(sampled, r_record, r_sun_cm=1.0)
    assert prep["ne"][0, 0] == 3.0
    assert prep["ne"][-1, 0] == 1.0
    assert prep["ds"][0, 0] > 0.0


def test_mas_spherical_b_radial_on_x_axis():
    # west limb (+x): colat=pi/2, lon=pi/2 => br along +x
    bx, by, bz = mas_spherical_b_to_cartesian(1.0, 0.0, 0.0, 2.0, 0.0, 0.0)
    assert abs(bx - 2.0) < 1e-9
    assert abs(by) < 1e-9
    assert abs(bz) < 1e-9
