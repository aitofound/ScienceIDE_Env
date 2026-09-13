from pathlib import Path

import numpy as np
import pytest
import xarray as xr

import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl import (
    CompilationConfig,
    GridIndexing,
    NullComm,
    Quantity,
    QuantityFactory,
    StencilConfig,
    StencilFactory,
    SubtileGridSizer,
    TileCommunicator,
)
from ndsl.config import Backend
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.typing import Float, Int
from ndsl.grid import (
    AngleGridData,
    ContravariantGridData,
    GridData,
    HorizontalGridData,
    MetricTerms,
    VerticalGridData,
)
from pyshield import PHYSICS_PACKAGES, Physics, PhysicsConfig, PhysicsState
from pyshield.stencils.surface import SurfaceConfig, SurfaceLayer, SurfaceState


def states_from_fortran_restarts(
    dycore_datafile: Path,
    phys_datafile: Path,
    tracer_datafile: Path,
    sfc_datafile: Path,
    ak: Quantity,
    quantity_factory: QuantityFactory,
    qf_sfc: QuantityFactory,
    stencil_factory: StencilFactory,
    schemes: list[PHYSICS_PACKAGES],
):
    pk0inv = (1.0 / physcons.P00) ** constants.KAPPA
    dycore_data = xr.open_dataset(dycore_datafile)
    phys_data = xr.open_dataset(phys_datafile)
    tracer_data = xr.open_dataset(tracer_datafile)
    sfc_data = xr.open_dataset(sfc_datafile)
    state = PhysicsState.init_zeros(quantity_factory, schemes)
    sstate = SurfaceState.init_zeros(qf_sfc)
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

    sstate.tsfc.field[:] = sfc_data.tsea[0, :, :].transpose()
    sstate.islmsk.field[:] = sfc_data.slmsk[0, :, :].transpose()
    sstate.zorl.field[:] = sfc_data.zorl[0, :, :].transpose()
    sstate.vegtype.field[:] = sfc_data.vtype[0, :, :].transpose()
    sstate.uustar.field[:] = sfc_data.uustar[0, :, :].transpose()
    sstate.sfcemis.field[:] = 0.98
    sstate.vfrac.field[:] = sfc_data.vfrac[0, :, :].transpose()
    sstate.shdmax.field[:] = sfc_data.shdmax[0, :, :].transpose()
    sstate.snowd.field[:] = sfc_data.snwdph[0, :, :].transpose()
    sstate.ffhh.field[:] = sfc_data.ffhh[0, :, :].transpose()
    sstate.ffmm.field[:] = sfc_data.ffmm[0, :, :].transpose()
    sstate.wind.field[:] = np.sqrt(
        state.ua.field[:, :, -1] ** 2.0 + state.va.field[:, :, -1] ** 2.0
    )
    sstate.stc.field[:] = sfc_data.stc[0, :, :, :].transpose(2, 1, 0)
    sstate.srflag.field[:] = sfc_data.srflag[0, :, :].transpose()
    sstate.hice.field[:] = sfc_data.hice[0, :, :].transpose()
    sstate.fice.field[:] = sfc_data.fice[0, :, :].transpose()
    sstate.tisfc.field[:] = sfc_data.tisfc[0, :, :].transpose()
    sstate.tprcp.field[:] = sfc_data.tprcp[0, :, :].transpose()
    sstate.weasd.field[:] = sfc_data.sheleg[0, :, :].transpose()

    return state, sstate


def setup_infrastructure(nx: Int, ny: Int, nz: Int, nzsoil: Int, etafile: Path):
    n_halo = 3
    rank = 0
    backend = Backend("st:numpy:cpu:IJK")

    comm = NullComm(rank, 1)
    communicator = TileCommunicator.from_layout(comm=comm, layout=(1, 1))

    sizer = SubtileGridSizer.from_tile_params(
        nx_tile=nx,
        ny_tile=ny,
        nz=nz,
        n_halo=n_halo,
        data_dimensions={},
        layout=(1, 1),
        tile_partitioner=communicator.partitioner.tile,
        tile_rank=communicator.tile.rank,
        backend=backend,
    )
    quantity_factory = QuantityFactory(sizer, backend=backend)

    soil_sizer = SubtileGridSizer.from_tile_params(
        nx_tile=nx,
        ny_tile=ny,
        nz=nzsoil,
        n_halo=n_halo,
        data_dimensions={},
        layout=(1, 1),
        tile_partitioner=communicator.partitioner.tile,
        tile_rank=communicator.tile.rank,
        backend=backend,
    )
    qf_soil = QuantityFactory(soil_sizer, backend=Backend("st:numpy:cpu:IJK"))

    comconf = CompilationConfig()
    comconf.validate_args = False
    sconf = StencilConfig(compilation_config=comconf)
    grid_indexing = GridIndexing.from_sizer_and_communicator(
        sizer=sizer, comm=communicator
    )
    stencil_factory = StencilFactory(
        config=sconf, grid_indexing=grid_indexing, comm=comm
    )
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
    return quantity_factory, qf_soil, stencil_factory, grid_data


@pytest.mark.parametrize("restart_path", [Path("test_data/RESTART/")])
def test_sfc_runs(restart_path: Path):
    dycore_path = restart_path.joinpath("fv_core.res.tile1.nc")
    physics_path = restart_path.joinpath("phy_data.tile1.nc")
    sfc_path = restart_path.joinpath("sfc_data.tile1.nc")
    tracer_path = restart_path.joinpath("fv_tracer.res.tile1.nc")
    etafile = restart_path.joinpath("eta91.nc")
    config = PhysicsConfig()
    sfc_config = SurfaceConfig()
    schemes = config.schemes
    quantity_factory, qf_soil, stencil_factory, grid_data = setup_infrastructure(
        nx=48, ny=48, nz=91, nzsoil=4, etafile=etafile
    )
    state, sstate = states_from_fortran_restarts(
        dycore_path,
        physics_path,
        tracer_path,
        sfc_path,
        grid_data.ak,
        quantity_factory,
        qf_soil,
        stencil_factory,
        schemes,
    )

    def make_quantity_2d() -> Quantity:
        return quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )

    def make_quantity_3d() -> Quantity:
        return quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )

    rb = make_quantity_2d()
    stress = make_quantity_2d()
    ps = make_quantity_2d()
    ps.field[:] = state.prsi.field[:, :, -1]
    hflx = make_quantity_2d()
    adjsfcdlw = make_quantity_2d()
    adjsfcdsw = make_quantity_2d()
    adjsfcnsw = make_quantity_2d()
    ua = make_quantity_3d()
    ua.field[:] = state.ua.field[:, :, ::-1]
    va = make_quantity_3d()
    va.field[:] = state.va.field[:, :, ::-1]
    pt = make_quantity_3d()
    pt.field[:] = state.pt.field[:, :, ::-1]
    delp = make_quantity_3d()
    delp.field[:] = state.delp.field[:, :, ::-1]
    prslk = make_quantity_3d()
    prslk.field[:] = state.prslk.field[:, :, ::-1]
    qvapor = make_quantity_3d()
    qvapor.field[:] = state.qvapor.field[:, :, ::-1]
    phil = make_quantity_3d()
    phil.field[:] = state.phil.field[:, :, ::-1]
    prsik = quantity_factory.zeros(
        [I_DIM, J_DIM, K_INTERFACE_DIM],
        units="unknown",
        dtype=Float,
    )
    prsik.field[:] = state.prsik.field[:, :, ::-1]
    sstate.u1[:] = ua[:, :, 0]
    sstate.v1[:] = va[:, :, 0]
    sstate.t1[:] = pt[:, :, 0]
    sstate.prsl1[:] = delp[:, :, 0]
    sstate.prsik[:] = prsik[:, :, 0]
    sstate.prslk[:] = prslk[:, :, 0]
    sstate.qvapor[:] = qvapor[:, :, 0]
    sstate.phil[:] = phil[:, :, 0]
    sstate.ps[:] = ps[:]
    sstate.sfcdlw[:] = adjsfcdlw[:]
    sstate.sfcdsw[:] = adjsfcdsw[:]
    sstate.sfcnsw[:] = adjsfcnsw[:]

    sfc = SurfaceLayer(stencil_factory, quantity_factory, sfc_config)
    sfc(sstate)


@pytest.mark.parametrize("restart_path", [Path("test_data/RESTART/")])
@pytest.mark.parametrize("backend", [Backend("st:numpy:cpu:IJK")])
def test_pyshield_runswith_sfc(restart_path: Path, backend: Backend):
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

    quantity_factory, qf_soil, stencil_factory, grid_data = setup_infrastructure(
        nx=nx,
        ny=ny,
        nz=nz,
        nzsoil=4,
        etafile=etafile,
    )

    state, sstate = states_from_fortran_restarts(
        dycore_path,
        physics_path,
        tracer_path,
        sfc_path,
        grid_data.ak,
        quantity_factory,
        qf_soil,
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
        schemes=["SFC_layer"],
    )

    sfc_config = SurfaceConfig(dt_atmos=dt)

    physics_driver = Physics(
        stencil_factory,
        quantity_factory,
        grid_data,
        config,
        sfc_config=sfc_config,
    )
    physics_driver(state, config.dt_atmos, surface_state=sstate)
