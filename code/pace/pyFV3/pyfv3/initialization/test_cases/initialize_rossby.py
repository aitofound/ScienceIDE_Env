"""Test case initialization for Rossby-Haurwitz wave 4

Corresponds to Fortran shallow-water test #6 found in tools/test_cases.F90 of
https://github.com/NOAA-GFDL/GFDL_atmos_cubed_sphere.git
"""

from types import SimpleNamespace

import numpy as np

from ndsl import CubedSphereCommunicator, QuantityFactory, constants
from ndsl.dsl.typing import Float
from ndsl.grid import GridData
from pyfv3.dycore_state import DycoreState
from pyfv3.initialization import init_utils

NHALO = constants.N_HALO_DEFAULT
OMG = Float(7.848e-6)
RK = Float(7.848e-6)
R = Float(4.0)  # Wave Number (likely)
GH0 = Float(8.0e3) * constants.GRAV


def _preinit_for_all_sw(numpy_state: SimpleNamespace, shape):
    """Pre-initialization for all shallow water tests

    Args:
        numpy_state: SimpleNamespace modified to update pe, pt, delp
        shape: tuple
    """
    numpy_state.pe[:] = 0.0
    numpy_state.pt[:] = 1.0

    # Initialize Halo Corners
    nx, ny, _ = init_utils.local_compute_size(shape)
    numpy_state.delp[:NHALO, :NHALO] = 0.0
    numpy_state.delp[:NHALO, NHALO + ny :] = 0.0
    numpy_state.delp[NHALO + nx :, :NHALO] = 0.0
    numpy_state.delp[NHALO + nx :, NHALO + ny :] = 0.0


def _calc_rossby_winds(p1, p2):
    """Calculates initial u or v winds specific to Rossby-Haurwitz wave test

    Args
        p1: np.ndarray
        p2: np.ndarray

    Returns
        np.ndarray: representing u or v (D-winds)
    """
    muv = init_utils._find_midpoint_unit_vectors(
        p1, p2
    )  # TODO: Refactor to non-protected call
    p3 = muv["midpoint"]
    e2 = muv["unit_dir"]
    ex = muv["exv"]
    ey = muv["eyv"]
    utmp = constants.RADIUS * OMG * np.cos(p3[:, :, 1]) + constants.RADIUS * RK * (
        np.cos(p3[:, :, 1]) ** (R - 1)
    ) * (R * np.sin(p3[:, :, 1]) ** 2 - np.cos(p3[:, :, 1]) ** 2) * np.cos(
        R * p3[:, :, 0]
    )
    vtmp = (
        -1
        * constants.RADIUS
        * RK
        * R
        * np.sin(p3[:, :, 1])
        * np.sin(R * p3[:, :, 0])
        * np.cos(p3[:, :, 1]) ** (R - 1)
    )
    return utmp * np.sum(e2 * ex, 2) + vtmp * np.sum(e2 * ey, 2)


def _calc_rossby_delp(grid_data: GridData):
    """Calculates initial delp, specific to Rossby-Haurwitz wave test

    Args
        grid_Data GridData

    Returns
        np.ndarray representing delp values
    """
    agd0 = grid_data.lon_agrid[:]
    agd1 = grid_data.lat_agrid[:]

    a = Float(0.5) * OMG * (2 * constants.OMEGA + OMG) * (np.cos(agd1) ** 2) + Float(
        0.25
    ) * RK * RK * (np.cos(agd1) ** (R + R)) * (
        (R + 1) * (np.cos(agd1) ** 2)
        + (2 * R * R - R - 2)
        - 2 * (R * R) * np.cos(agd1) ** (-2)
    )
    b = (
        (2 * (constants.OMEGA + OMG) * RK / ((R + 1) * (R + 2)))
        * (np.cos(agd1) ** R)
        * ((R * R + 2 * R + 2) - ((R + 1) * np.cos(agd1)) ** 2)
    )
    c = (
        Float(0.25)
        * RK
        * RK
        * (np.cos(agd1) ** (2 * R))
        * ((R + 1) * (np.cos(agd1) ** 2) - (R + 2))
    )
    return GH0 + constants.RADIUS * constants.RADIUS * (
        a + b * np.cos(R * agd0) + c * np.cos(2 * R * agd0)
    )


def _init_for_rossby(numpy_state: SimpleNamespace, grid_data: GridData, shape):
    """Initialization specific to Rossby-Haurwitz wave test

    Args
        numpy_state: SimpleNamespace, modified to update the phis, delp, u, v
        grid_Data: GridData
    """
    numpy_state.phis[:] = 0.0

    # Calculate helper slices for delp, u+v winds
    # similar to init_utils.compute_slices(nx, ny)
    nx, ny, _ = init_utils.local_compute_size(shape)
    islice = slice(NHALO, NHALO + nx)
    islice_xtra = slice(NHALO, NHALO + nx + 1)
    jslice = slice(NHALO, NHALO + ny)
    jslice_xtra = slice(NHALO, NHALO + ny + 1)

    # Initialize delp
    delp_2d_buffer = (islice_xtra, jslice_xtra)
    delp_buffer_0 = (islice_xtra, jslice_xtra, 0)
    numpy_state.delp[delp_buffer_0] = _calc_rossby_delp(grid_data)[delp_2d_buffer]

    grid = np.transpose(
        np.stack(  # TODO: Refactor to non-protected _horizontal_data
            [grid_data._horizontal_data.lon[:], grid_data._horizontal_data.lat[:]]
        ),
        [1, 2, 0],
    )

    # Initialize u winds
    p1 = grid[:-1, :, :]
    p2 = grid[1:, :, :]
    u_2d_buffer = (islice, jslice_xtra)
    u_buffer_0 = (islice, jslice_xtra, 0)
    numpy_state.u[u_buffer_0] = _calc_rossby_winds(p1, p2)[u_2d_buffer]

    # Initialize v winds
    p1 = grid[:, :-1, :]
    p2 = grid[:, 1:, :]
    v_2d_buffer = (islice_xtra, jslice)
    v_buffer_0 = (islice_xtra, jslice, 0)
    numpy_state.v[v_buffer_0] = _calc_rossby_winds(p1, p2)[v_2d_buffer]

    # NOTE: test_cases.F90 has dtoa and atoc calls, but not implemented here.


def _postinit_for_all_sw(numpy_state: SimpleNamespace):
    """Post-initialization from test_cases.F90 that applies to all shallow water tests

    Args
        numpy_state: SimpleNamespace - modified
    """

    # NOTE: The cl/cl2 tracers from the original test_cases.F90 aren't brought over.
    # NOTE: A-grid and C-grid winds are not initialized here.

    numpy_state.delp[:, :, 1:] = numpy_state.delp[:, :, 0][:, :, np.newaxis]
    numpy_state.u[:, :, 1:] = numpy_state.u[:, :, 0][:, :, np.newaxis]
    numpy_state.v[:, :, 1:] = numpy_state.v[:, :, 0][:, :, np.newaxis]
    numpy_state.ps[:] = numpy_state.delp[:, :, 0]


def init_rossby_state(
    grid_data: GridData,
    quantity_factory: QuantityFactory,
    comm: CubedSphereCommunicator,
) -> DycoreState:
    """
    Create an initial DycoreState for Rossby

    Args:
        grid_data:              current selected grid data values
        quantity_factory:       QuantityFactory
        comm:                   CubedSphereCommunicator

    Returns:
        DycoreState
    """

    # TODO: Check sw_dynamics is True (https://github.com/NOAA-GFDL/PyFV3/pull/50)
    #       May require a change to pass a config here in order to check.

    sample_quantity = grid_data.lat
    shape = (*sample_quantity.shape[0:2], grid_data.ak.shape[0])
    numpy_state = init_utils.empty_numpy_dycore_state(shape)

    _preinit_for_all_sw(numpy_state, shape)
    _init_for_rossby(numpy_state, grid_data, shape)
    _postinit_for_all_sw(numpy_state)

    state = DycoreState.init_from_numpy_arrays(
        numpy_state.__dict__,
        sizer=quantity_factory.sizer,
        backend=sample_quantity.metadata.backend,
    )

    comm.halo_update(state.phis, n_points=NHALO)
    comm.vector_halo_update(state.u, state.v, n_points=NHALO)

    return state
