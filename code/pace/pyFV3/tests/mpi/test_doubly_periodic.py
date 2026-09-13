from datetime import timedelta
from pathlib import Path
from typing import cast

import pyfv3.initialization.test_cases.initialize_baroclinic as baroclinic_init
from ndsl import (
    Backend,
    CompilationConfig,
    CubedSphereCommunicator,
    GridIndexing,
    MPIComm,
    QuantityFactory,
    StencilConfig,
    StencilFactory,
    SubtileGridSizer,
    TileCommunicator,
    TilePartitioner,
)
from ndsl.grid import DampingCoefficients, GridData, MetricTerms
from ndsl.performance import NullTimer
from pyfv3 import DynamicalCore, DynamicalCoreConfig
from pyfv3.tracers import default_ai2_tracers


def test_dycore_runs_one_step() -> None:
    backend = Backend("st:numpy:cpu:IJK")
    layout = (3, 3)
    config = DynamicalCoreConfig(
        layout=layout,
        npx=13,
        npy=13,
        npz=79,
        ntiles=6,
        nwat=6,
        dt_atmos=225,
        a_imp=1.0,
        beta=0.0,
        consv_te=False,  # not implemented, needs allreduce
        d2_bg=0.0,
        d2_bg_k1=0.2,
        d2_bg_k2=0.1,
        d4_bg=0.15,
        d_con=1.0,
        d_ext=0.0,
        dddmp=0.5,
        delt_max=0.002,
        do_sat_adj=True,
        do_vort_damp=True,
        fill=True,
        hord_dp=6,
        hord_mt=6,
        hord_tm=6,
        hord_tr=8,
        hord_vt=6,
        hydrostatic=False,
        k_split=1,
        ke_bg=0.0,
        kord_mt=9,
        kord_tm=-9,
        kord_tr=9,
        kord_wz=9,
        n_split=1,
        nord=3,
        p_fac=0.05,
        rf_fast=True,
        rf_cutoff=3000.0,
        tau=10.0,
        vtdm4=0.06,
        z_tracer=True,
        do_qa=True,
    )
    mpi_comm = MPIComm()
    partitioner = TilePartitioner(config.layout)
    # TODO: cleanup typing of tile vs cubed sphere communicators,
    # currently both have a .tile attribute that reference a TileCommunicator
    # instead both should have the methods specific to a TileCommunicator
    # (to be put on the Communicator abstract base class) and
    # the CubedSphere implementation should defer to the tile.
    communicator = cast(
        CubedSphereCommunicator,
        TileCommunicator(mpi_comm, partitioner),
    )
    stencil_config = StencilConfig(
        compilation_config=CompilationConfig(
            communicator=communicator,
            backend=backend,
            rebuild=False,
            validate_args=True,
        )
    )
    sizer = SubtileGridSizer.from_tile_params(
        nx_tile=config.npx - 1,
        ny_tile=config.npy - 1,
        nz=config.npz,
        n_halo=3,
        layout=config.layout,
        tile_partitioner=partitioner,
        tile_rank=communicator.rank,
        backend=backend,
    )
    grid_indexing = GridIndexing.from_sizer_and_communicator(
        sizer=sizer, comm=communicator
    )
    quantity_factory = QuantityFactory(sizer=sizer, backend=backend)
    metric_terms = MetricTerms(
        quantity_factory=quantity_factory,
        communicator=communicator,
        eta_file=Path(__file__).parent / ".." / "data" / "eta79.nc",
    )
    grid_data = GridData.new_from_metric_terms(metric_terms)

    # create an initial state from the Jablonowski & Williamson Baroclinic
    # test case perturbation. JRMS2006
    state = baroclinic_init.init_baroclinic_state(
        grid_data=grid_data,
        quantity_factory=quantity_factory,
        adiabatic=config.adiabatic,
        hydrostatic=config.hydrostatic,
        moist_phys=config.moist_phys,
        comm=communicator,
    )
    stencil_factory = StencilFactory(
        config=stencil_config,
        grid_indexing=grid_indexing,
    )

    default_ai2_tracers(quantity_factory)

    dycore = DynamicalCore(
        comm=communicator,
        grid_data=grid_data,
        stencil_factory=stencil_factory,
        quantity_factory=quantity_factory,
        damping_coefficients=DampingCoefficients.new_from_metric_terms(metric_terms),
        config=config,
        phis=state.phis,
        state=state,
        timestep=timedelta(seconds=255),
    )

    # run one step
    dycore.step_dynamics(state, NullTimer())
