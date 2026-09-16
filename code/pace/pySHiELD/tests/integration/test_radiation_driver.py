import datetime
from pathlib import Path

import numpy as np
import pytest
import xarray as xr

from ndsl import NullComm, Quantity, QuantityFactory, TileCommunicator
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
from pyshield._config import PhysicsConfig
from pyshield.physics_state import PHYSICS_PACKAGES, PhysicsState
from pyshield.radiation import RTE_RRTMGPConfig, RTE_RRTMGPDriver, RTE_RRTMGPState
from pyshield.stencils.physics import Physics, calc_sigma
from pyshield.stencils.surface import SurfaceState


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


def fortran_restart_to_radstate(
    dycore_datafile: Path,
    phys_datafile: Path,
    tracer_datafile: Path,
    ak: Quantity,
    state: RTE_RRTMGPState,
):
    dycore_data = xr.open_dataset(dycore_datafile)
    phys_data = xr.open_dataset(phys_datafile)
    tracer_data = xr.open_dataset(tracer_datafile)
    buff_3d = np.zeros_like(state.prsi.field)
    for k in range(buff_3d.shape[2]):
        if k == 0:
            buff_3d[:, :, k] = ak[0]
        else:
            buff_3d[:, :, k] = buff_3d[:, :, k - 1] + dycore_data.delp[0, k - 1, :, :]
    state.prsi.field[:] = buff_3d[:, :, ::-1]
    state.prsl.field[:] = (
        state.prsi.field[:, :, 1:] - state.prsi.field[:, :, :-1]
    ) / np.log(state.prsi.field[:, :, 1:] / state.prsi.field[:, :, :-1])
    state.tlyr.field[:] = dycore_data.T[0, ::-1, :, :].transpose(1, 2, 0)
    state.tlvl.field[:, :, 1:-1] = state.tlyr.field[:, :, :-1] + (
        state.tlyr.field[:, :, 1:] - state.tlyr.field[:, :, :-1]
    ) * (np.log(state.prsi.field[:, :, 1:-1]) - np.log(state.prsl.field[:, :, :-1])) / (
        np.log(state.prsl.field[:, :, 1:]) - np.log(state.prsl.field[:, :, :-1])
    )
    state.tlvl.field[:, :, -1] = state.tlyr.field[:, :, -1]
    state.tlvl.field[:, :, 0] = state.tlyr.field[:, :, 0]
    state.tsfc.field[:] = phys_data.ts_clim_iano[0, :, :]
    state.qvapor.field[:] = tracer_data.sphum[0, ::-1, :, :].transpose(1, 2, 0)
    state.qliquid.view[:] = tracer_data.liq_wat[0, ::-1, :, :].transpose(1, 2, 0)
    state.qice.view[:] = tracer_data.ice_wat[0, ::-1, :, :].transpose(1, 2, 0)
    state.qcld.view[:] = tracer_data.cld_amt[0, ::-1, :, :].transpose(1, 2, 0)
    state.qo3mr.view[:] = tracer_data.o3mr[0, ::-1, :, :].transpose(1, 2, 0)


def states_from_fortran_restarts(
    dycore_datafile: Path,
    phys_datafile: Path,
    tracer_datafile: Path,
    ak: Quantity,
    quantity_factory: QuantityFactory,
    schemes: list[PHYSICS_PACKAGES],
):
    dycore_data = xr.open_dataset(dycore_datafile)
    phys_data = xr.open_dataset(phys_datafile)
    tracer_data = xr.open_dataset(tracer_datafile)

    state = PhysicsState.init_zeros(quantity_factory, schemes)
    radstate = RTE_RRTMGPState.init_zeros(quantity_factory)
    sstate = SurfaceState.init_zeros(quantity_factory)

    buff_3d = np.zeros_like(state.prsi.field)
    for k in range(buff_3d.shape[2]):
        if k == 0:
            buff_3d[:, :, k] = ak[0]
        else:
            buff_3d[:, :, k] = buff_3d[:, :, k - 1] + dycore_data.delp[0, k - 1, :, :]
    state.prsi.field[:] = buff_3d[:, :, :]

    state.delp.field[:] = dycore_data.delp[0, :, :, :].transpose(2, 1, 0)
    state.pt.field[:] = dycore_data.T[0, :, :, :].transpose(2, 1, 0)
    state.tsfc.field[:] = phys_data.ts_clim_iano[0, :, :].transpose()
    state.qvapor.field[:] = tracer_data.sphum[0, :, :, :].transpose(2, 1, 0)
    state.qliquid.view[:] = tracer_data.liq_wat[0, :, :, :].transpose(2, 1, 0)
    state.qice.view[:] = tracer_data.ice_wat[0, :, :, :].transpose(2, 1, 0)
    state.qcld.view[:] = tracer_data.cld_amt[0, :, :, :].transpose(2, 1, 0)
    state.qo3mr.view[:] = tracer_data.o3mr[0, :, :, :].transpose(2, 1, 0)
    state.delz.field[:] = dycore_data.DZ[0, :, :, :].transpose(2, 1, 0)
    return state, radstate, sstate


@pytest.mark.parametrize("datapath", [Path("test_data/RESTART/")])
def test_rte_rrtmgp(datapath: Path):
    dycore_data = Path(datapath.joinpath("fv_core.res.tile1.nc"))
    phys_data = Path(datapath.joinpath("phy_data.tile1.nc"))
    tracer_data = Path(datapath.joinpath("fv_tracer.res.tile1.nc"))
    etafile = Path(datapath.joinpath("eta91.nc"))

    nx = 48
    ny = 48
    nz = 91
    nhalo = 3

    date = datetime.datetime(2020, 1, 1, 12, tzinfo=datetime.timezone.utc)
    stencil_factory, quantity_factory, grid_data = setup_infrastructure(
        nx, ny, nz, nhalo, etafile
    )
    grid_data.lon_agrid.field[:] = grid_data.lon.field[:-1, :-1]
    grid_data.lat_agrid.field[:] = grid_data.lat.field[:-1, :-1]
    radconf = RTE_RRTMGPConfig(
        deltsw=3600.0,
        delt_rad=3600.0,
        date=date,
        fhswr=1.0,
        fhlwr=1.0,
        isolar=10,
        icmphys=4,
        ico2flg=0,
        ioznflg=1,
        ictmflg=-1,
        ialbflg=-1,
        iemsflg=0,
        ldisable_radiation_quasi_sea_ice=False,
        solar_constant_file=Path("global_solarconstant_noaa_an.txt"),
        input_dir=Path(datapath.joinpath("test_data/")),
        aerosol_file=Path(datapath.joinpath("test_data/")),
        sollat=0.0,
        nstp=6,
        ivflip=1,
        lcnorm=False,
        lcrick=False,
        gfs_cloud_overlap=False,
    )
    sigma = calc_sigma(grid_data.ak[:], grid_data.bk[:], 0)
    gridlon = grid_data.lon_agrid
    gridlat = grid_data.lat_agrid

    state = RTE_RRTMGPState.init_zeros(quantity_factory)
    sstate = SurfaceState.init_zeros(quantity_factory)
    fortran_restart_to_radstate(
        dycore_datafile=dycore_data,
        phys_datafile=phys_data,
        tracer_datafile=tracer_data,
        ak=grid_data.ak,
        state=state,
    )

    rad = RTE_RRTMGPDriver(
        config=radconf,
        gridlon=gridlon,
        gridlat=gridlat,
        sigma=sigma,
        quantity_factory=quantity_factory,
        stencil_factory=stencil_factory,
    )
    rad.step_radiation(state, sstate, date)


@pytest.mark.parametrize("datapath", [Path("test_data/RESTART/")])
def test_rte_rrtmgp_in_physics(datapath: Path):
    dycore_data = Path(datapath.joinpath("fv_core.res.tile1.nc"))
    phys_data = Path(datapath.joinpath("phy_data.tile1.nc"))
    tracer_data = Path(datapath.joinpath("fv_tracer.res.tile1.nc"))
    etafile = Path(datapath.joinpath("eta91.nc"))

    nx = 48
    ny = 48
    nz = 91
    nhalo = 3
    npx = nx + 1
    npy = ny + 1
    npz = nz + 1

    date = datetime.datetime(2020, 1, 1, 12, tzinfo=datetime.timezone.utc)
    stencil_factory, quantity_factory, grid_data = setup_infrastructure(
        nx, ny, nz, nhalo, etafile
    )
    grid_data.lon_agrid.field[:] = grid_data.lon.field[:-1, :-1]
    grid_data.lat_agrid.field[:] = grid_data.lat.field[:-1, :-1]
    conf = PhysicsConfig(
        dt_atmos=225.0,
        fhswr=225.0,
        fhlwr=225.0,
        npx=npx,
        npy=npy,
        npz=npz,
        nwat=6,
        prescribe_sst=False,
        schemes=["GFS_microphysics", "RTE_RRTMGP"],
        hydro_delp=False,
    )
    radconf = RTE_RRTMGPConfig(
        deltsw=3600.0,
        delt_rad=3600.0,
        date=date,
        fhswr=1.0,
        fhlwr=1.0,
        isolar=10,
        icmphys=4,
        ico2flg=0,
        ioznflg=1,
        ictmflg=-1,
        ialbflg=-1,
        iemsflg=0,
        ldisable_radiation_quasi_sea_ice=False,
        solar_constant_file=Path("global_solarconstant_noaa_an.txt"),
        input_dir=Path(datapath.joinpath("test_data/")),
        aerosol_file=Path(datapath.joinpath("test_data/")),
        sollat=0.0,
        nstp=6,
        ivflip=1,
        lcnorm=False,
        lcrick=False,
        gfs_cloud_overlap=False,
    )

    state, radstate, sstate = states_from_fortran_restarts(
        dycore_data,
        phys_data,
        tracer_data,
        grid_data.ak,
        quantity_factory,
        conf.schemes,
    )

    physics = Physics(
        stencil_factory,
        quantity_factory,
        grid_data,
        conf,
        radconf,
    )
    physics(
        state,
        radiation_state=radstate,
        surface_state=sstate,
        date=date,
        timestep=conf.dt_atmos,
    )
