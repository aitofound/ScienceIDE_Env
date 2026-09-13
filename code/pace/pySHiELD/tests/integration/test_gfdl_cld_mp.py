from pathlib import Path

import numpy as np
import pytest
import xarray as xr

import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl import NullComm, Quantity, QuantityFactory, StencilFactory, TileCommunicator
from ndsl.boilerplate import get_factories_single_tile
from ndsl.config import Backend, backend_python
from ndsl.grid import (
    AngleGridData,
    ContravariantGridData,
    GridData,
    HorizontalGridData,
    MetricTerms,
    VerticalGridData,
)
from pyshield import PHYSICS_PACKAGES, Physics, PhysicsConfig, PhysicsState
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig


def setup_infrastructure(
    nx: int,
    ny: int,
    nz: int,
    nhalo: int,
    etafile: Path,
    backend: Backend = backend_python,
):
    stencil_factory, quantity_factory = get_factories_single_tile(
        nx=nx, ny=ny, nz=nz, nhalo=nhalo, backend=backend
    )
    rank = 0
    comm = NullComm(rank, 1)
    communicator = TileCommunicator.from_layout(comm=comm, layout=(1, 1))

    metric_terms = MetricTerms(
        quantity_factory=quantity_factory,
        communicator=communicator,
        grid_type=4,
        eta_file=etafile,
    )
    horizontal_data = HorizontalGridData.new_from_metric_terms(metric_terms)
    vertical_data = VerticalGridData.new_from_metric_terms(metric_terms)
    contravariant_data = ContravariantGridData.new_from_metric_terms(metric_terms)
    angle_data = AngleGridData.new_from_metric_terms(metric_terms)
    grid_data = GridData(
        horizontal_data=horizontal_data,
        vertical_data=vertical_data,
        contravariant_data=contravariant_data,
        angle_data=angle_data,
    )
    return stencil_factory, quantity_factory, grid_data


def states_from_fortran_restarts(
    dycore_datafile: Path,
    phys_datafile: Path,
    tracer_datafile: Path,
    sfc_datafile: Path,
    ak: Quantity,
    quantity_factory: QuantityFactory,
    stencil_factory: StencilFactory,
    schemes: list[PHYSICS_PACKAGES],
):
    pk0inv = (1.0 / physcons.P00) ** constants.KAPPA
    dycore_data = xr.open_dataset(dycore_datafile)
    phys_data = xr.open_dataset(phys_datafile)
    tracer_data = xr.open_dataset(tracer_datafile)
    sfc_data = xr.open_dataset(sfc_datafile)
    state = PhysicsState.init_zeros(quantity_factory, schemes)
    buff_3d = np.zeros_like(state.prsi.field)
    npz = buff_3d.shape[2]
    for k in range(npz):
        if k == 0:
            buff_3d[:, :, k] = ak[0]
        else:
            buff_3d[:, :, k] = buff_3d[:, :, k - 1] + dycore_data.delp[0, k - 1, :, :]
    state.delz.field[:, :, :] = dycore_data.DZ[0, :, :, :].transpose(2, 1, 0)
    state.phii.field[:, :, -1] = dycore_data.phis[0, :, :].transpose()
    for k in range(npz - 2, -1, -1):
        state.phii.field[:, :, k] = state.phii.field[:, :, k + 1] + (
            state.delz.field[:, :, k] * constants.GRAV
        )
    state.phil.field[:, :, :] = 0.5 * (
        state.phii.field[:, :, :-1] + state.phii.field[:, :, 1:]
    )
    state.prsi.field[:] = buff_3d[:, :, :]
    state.delp.field[:] = dycore_data.delp[0, :, :, :].transpose(2, 1, 0)
    state.prsik.field[:] = np.log(state.prsi.field[:])
    state.prsik.field[:, :, 0] = (ak[0] / physcons.P00) ** constants.KAPPA
    state.prsik.field[:, :, -1] = (
        np.exp(constants.KAPPA * state.prsik.field[:, :, -1]) * pk0inv
    )
    state.prslk.field[:] = np.exp(
        constants.KAPPA * np.log(state.delp.field[:] / physcons.P00)
    )
    state.pt.field[:] = dycore_data.T[0, :, :, :].transpose(2, 1, 0)
    state.qvapor.field[:] = tracer_data.sphum[0, :, :, :].transpose(2, 1, 0)
    state.qliquid.view[:] = tracer_data.liq_wat[0, :, :, :].transpose(2, 1, 0)
    state.qice.view[:] = tracer_data.ice_wat[0, :, :, :].transpose(2, 1, 0)
    state.qcld.view[:] = tracer_data.cld_amt[0, :, :, :].transpose(2, 1, 0)
    state.qo3mr.view[:] = tracer_data.o3mr[0, :, :, :].transpose(2, 1, 0)
    state.delz.field[:] = dycore_data.DZ[0, :, :, :].transpose(2, 1, 0)

    return state


@pytest.mark.parametrize("restart_path", [Path("test_data/RESTART/")])
@pytest.mark.parametrize("backend", [Backend("st:numpy:cpu:IJK")])
def test_pyshield_runswith_gfdl_cld_mp(restart_path: Path, backend: Backend):
    dycore_path = restart_path.joinpath("fv_core.res.tile1.nc")
    physics_path = restart_path.joinpath("phy_data.tile1.nc")
    sfc_path = restart_path.joinpath("sfc_data.tile1.nc")
    tracer_path = restart_path.joinpath("fv_tracer.res.tile1.nc")
    etafile = restart_path.joinpath("eta91.nc")
    config = PhysicsConfig()
    schemes = config.schemes
    nx = 48
    ny = 48
    nz = 91
    dt = 225.0

    stencil_factory, quantity_factory, grid_data = setup_infrastructure(
        nx=nx, ny=ny, nz=nz, nhalo=3, etafile=etafile, backend=backend
    )

    state = states_from_fortran_restarts(
        dycore_path,
        physics_path,
        tracer_path,
        sfc_path,
        grid_data.ak,
        quantity_factory,
        stencil_factory,
        schemes,
    )
    config = PhysicsConfig(
        dt_atmos=dt,
        hydrostatic=False,
        npx=nx + 1,
        npy=ny + 1,
        npz=nz + 1,
        nwat=6,
        schemes=["GFDL_cloud_microphysics"],
    )
    mp_config = GFDLCloudMPConfig(
        dt_full=dt,
        hydrostatic=False,
        npx=nx + 1,
        npy=ny + 1,
        npz=nz + 1,
        layout=(1, 1),
        nwat=6,
        do_sedi_uv=True,
        do_sedi_w=True,
        do_sedi_heat=False,
        rad_snow=True,
        rad_graupel=True,
        rad_rain=True,
        const_vi=False,
        const_vs=False,
        const_vg=False,
        const_vr=False,
        vi_fac=1.0,
        vs_fac=1.0,
        vg_fac=1.0,
        vr_fac=1.0,
        vi_max=1.0,
        vs_max=2.0,
        vg_max=12.0,
        vr_max=12.0,
        qi_lim=1.0,
        prog_ccn=False,
        do_qa=True,
        tau_l2v=225.0,
        tau_v2l=150.0,
        rthresh=10.0e-6,
    )
    physics_driver = Physics(
        stencil_factory,
        quantity_factory,
        grid_data,
        config,
        gfdl_cld_mp_config=mp_config,
    )
    physics_driver(state, config.dt_atmos)
