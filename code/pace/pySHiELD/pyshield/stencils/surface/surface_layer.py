import ndsl.constants as constants
from ndsl import Quantity, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM
from ndsl.dsl.gt4py import FORWARD, computation, interval
from ndsl.dsl.typing import (
    Bool,
    BoolFieldIJ,
    Float,
    FloatField,
    FloatFieldIJ,
    Int,
    IntFieldIJ,
)
from pyshield.stencils.surface._config import SurfaceConfig
from pyshield.stencils.surface.sfc_diff import SurfaceExchange
from pyshield.stencils.surface.sfc_ocean import SurfaceOcean
from pyshield.stencils.surface.sfc_sice import SurfaceSeaIce
from pyshield.stencils.surface.sfc_state import SurfaceState


def init_step_vars(
    tsfc: FloatFieldIJ,
    phil: FloatFieldIJ,
    prsl: FloatFieldIJ,
    prsik: FloatFieldIJ,
    prslk: FloatFieldIJ,
    vfrac: FloatFieldIJ,
    sfcemis: FloatFieldIJ,
    sfcdlw: FloatFieldIJ,
    tsurf: FloatFieldIJ,
    flag_guess: BoolFieldIJ,
    flag_iter: BoolFieldIJ,
    drain: FloatFieldIJ,
    ep1d: FloatFieldIJ,
    runof: FloatFieldIJ,
    hflx: FloatFieldIJ,
    evap: FloatFieldIJ,
    evbs: FloatFieldIJ,
    evcw: FloatFieldIJ,
    trans: FloatFieldIJ,
    sbsno: FloatFieldIJ,
    snowc: FloatFieldIJ,
    snohf: FloatFieldIJ,
    qss: FloatFieldIJ,
    gflx: FloatFieldIJ,
    zlvl: FloatFieldIJ,
    smcwlt2: FloatFieldIJ,
    smcref2: FloatFieldIJ,
    ddvel: FloatFieldIJ,
    work3: FloatFieldIJ,
    sigmaf: FloatFieldIJ,
    gabsbdlw: FloatFieldIJ,
    prsl1: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        tsurf = tsfc
        flag_guess = False
        flag_iter = True
        drain = 0.0
        ep1d = 0.0
        runof = 0.0
        hflx = 0.0
        evap = 0.0
        evbs = 0.0
        evcw = 0.0
        trans = 0.0
        sbsno = 0.0
        snowc = 0.0
        snohf = 0.0
        qss = 0.0
        gflx = 0.0
        ddvel = 0.0
        zlvl = phil * constants.RGRAV
        smcwlt2 = 0.0
        smcref2 = 0.0
        work3 = prsik / prslk
        sigmaf = max(vfrac, 0.01)
        gabsbdlw = sfcemis * sfcdlw
        prsl1 = prsl


def update_guess_and_soil_0(
    wind: FloatFieldIJ,
    iteration: Int,
    flag_guess: BoolFieldIJ,
    stsoil: FloatField,
    stc0: FloatFieldIJ,
    stc1: FloatFieldIJ,
    islmsk: IntFieldIJ,
):
    with computation(FORWARD):
        with interval(0, 1):
            if (iteration == 0) and (wind < 2.0):
                flag_guess[0, 0] = True
            if islmsk > 0:
                stc0 = stsoil[0, 0, 0]
        with interval(1, 2):
            if islmsk > 0:
                stc1 = stsoil[0, 0, 0]


def update_guess_and_soil_1(
    wind: FloatFieldIJ,
    iteration: Int,
    flag_guess: BoolFieldIJ,
    flag_iter: BoolFieldIJ,
    stsoil: FloatField,
    stc0: FloatFieldIJ,
    stc1: FloatFieldIJ,
    islmsk: IntFieldIJ,
):
    from __externals__ import nsstm_coupling

    with computation(FORWARD):
        with interval(0, 1):
            flag_iter = False
            flag_guess = False

            if (iteration == 0) and (wind < 2.0):
                if (islmsk == 1) or ((islmsk == 0) and (nsstm_coupling > 0)):
                    flag_iter = True
            if islmsk > 0:
                stsoil = stc0
        with interval(1, 2):
            if islmsk > 0:
                stsoil = stc1


def post_loop(
    qsfc: FloatFieldIJ,
    qss: FloatFieldIJ,
    ddvel: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        ddvel = 0.0
        qsfc = qss


class SurfaceLayer:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: SurfaceConfig,
    ):
        if config.nstf_name[0] > 0:
            raise NotImplementedError(
                "NSSTM is not implemented, nstf_name[0] must be 0, "
                f"got {config.nstf_name[0]}"
            )
        grid_indexing = stencil_factory.grid_indexing
        origin = grid_indexing.origin_compute()
        domain_atm = grid_indexing.domain_compute()
        domain_soil = (domain_atm[0], domain_atm[1], config.lsoil)

        def make_quantity_2d() -> Quantity:
            return quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=Float,
            )

        self._tsurf = make_quantity_2d()
        self._cdq = make_quantity_2d()
        self._ddvel = make_quantity_2d()
        self._drain = make_quantity_2d()
        self._ep1d = make_quantity_2d()
        self._runof = make_quantity_2d()
        self._evbs = make_quantity_2d()
        self._evcw = make_quantity_2d()
        self._trans = make_quantity_2d()
        self._sbsno = make_quantity_2d()
        self._snowc = make_quantity_2d()
        self._snohf = make_quantity_2d()
        self._qss = make_quantity_2d()
        self._gflx = make_quantity_2d()
        self._zlvl = make_quantity_2d()
        self._smcwlt2 = make_quantity_2d()
        self._smcref2 = make_quantity_2d()
        self._work3 = make_quantity_2d()
        self._sigmaf = make_quantity_2d()
        self._cd = make_quantity_2d()
        self._fm10 = make_quantity_2d()
        self._fh2 = make_quantity_2d()
        self._cmm = make_quantity_2d()
        self._chh = make_quantity_2d()
        self._gabsbdlw = make_quantity_2d()
        self._stc0 = make_quantity_2d()
        self._stc1 = make_quantity_2d()
        self._snowmt = make_quantity_2d()
        self._prsl1 = make_quantity_2d()

        self._flag_guess = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="None",
            dtype=Bool,
        )

        self._flag_iter = quantity_factory.ones(
            [I_DIM, J_DIM],
            units="None",
            dtype=Bool,
        )

        self._init_step_vars = stencil_factory.from_origin_domain(
            init_step_vars,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        self._exchange = SurfaceExchange(
            stencil_factory=stencil_factory,
            ivegsrc=config.ivegsrc,
            do_z0_hwrf15=config.do_z0_hwrf15,
            do_z0_hwrf17=config.do_z0_hwrf17,
            do_z0_hwrf17_hwonly=config.do_z0_hwrf17_hwonly,
            do_z0_moon=config.do_z0_moon,
            redrag=config.redrag,
            wind_th_hwrf=config.wind_th_hwrf,
        )
        self._update_guess_and_soil_0 = stencil_factory.from_origin_domain(
            update_guess_and_soil_0,
            origin=grid_indexing.origin_compute(),
            domain=domain_soil,
        )
        self._sfc_ocean = SurfaceOcean(
            stencil_factory=stencil_factory,
        )
        self._sfc_sice = SurfaceSeaIce(
            stencil_factory=stencil_factory,
            mom4ice=config.mom4ice,
            lsm=config.lsm,
            dt_atmos=config.dt_atmos,
        )
        self._update_guess_and_soil_1 = stencil_factory.from_origin_domain(
            update_guess_and_soil_1,
            externals={"nsstm_coupling": config.nstf_name[0]},
            origin=grid_indexing.origin_compute(),
            domain=domain_soil,
        )
        self._post_loop = stencil_factory.from_origin_domain(
            post_loop,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )

    def __call__(
        self,
        state: SurfaceState,
    ):
        self._init_step_vars(
            state.tsfc,
            state.phil,
            state.prsl1,
            state.prsik,
            state.prslk,
            state.vfrac,
            state.sfcemis,
            state.sfcdlw,
            self._tsurf,
            self._flag_guess,
            self._flag_iter,
            self._drain,
            self._ep1d,
            self._runof,
            state.hflx,
            state.evap,
            self._evbs,
            self._evcw,
            self._trans,
            self._sbsno,
            self._snowc,
            self._snohf,
            self._qss,
            self._gflx,
            self._zlvl,
            self._smcwlt2,
            self._smcref2,
            self._ddvel,
            self._work3,
            self._sigmaf,
            self._gabsbdlw,
            self._prsl1,
        )
        for iteration in range(2):
            self._exchange(
                state.u1,
                state.v1,
                state.t1,
                state.qvapor,
                self._ddvel,
                self._tsurf,
                state.tsfc,
                self._work3,
                self._prsl1,
                state.zorl,
                self._zlvl,
                state.shdmax,
                self._sigmaf,
                state.uustar,
                state.snowd,
                state.ztrl,
                self._cd,
                self._cdq,
                state.rb,
                state.stress,
                state.ffmm,
                state.ffhh,
                state.wind,
                self._fm10,
                self._fh2,
                state.islmsk,
                state.vegtype,
                self._flag_iter,
            )

            self._update_guess_and_soil_0(
                state.wind,
                iteration,
                self._flag_guess,
                state.stc,
                self._stc0,
                self._stc1,
                state.islmsk,
            )

            self._sfc_ocean(
                state.ps,
                state.u1,
                state.v1,
                state.t1,
                state.qvapor,
                state.tsfc,
                self._cd,
                self._cdq,
                self._prsl1,
                self._work3,
                self._ddvel,
                self._qss,
                self._cmm,
                self._chh,
                self._gflx,
                state.evap,
                state.hflx,
                self._ep1d,
                state.islmsk,
                self._flag_iter,
            )

            # TODO: LSM here

            self._sfc_sice(
                state.ps,
                state.wind,
                state.t1,
                state.qvapor,
                state.sfcemis,
                self._gabsbdlw,
                state.sfcnsw,
                state.sfcdsw,
                state.srflag,
                self._cd,
                self._cdq,
                self._prsl1,
                self._work3,
                state.islmsk,
                self._flag_iter,
                state.hice,
                state.fice,
                state.tisfc,
                state.weasd,
                state.tsfc,
                state.tprcp,
                self._stc0,
                self._stc1,
                self._ep1d,
                state.snowd,
                self._qss,
                self._cmm,
                self._chh,
                state.evap,
                state.hflx,
                self._gflx,
                self._snowmt,
            )

            self._update_guess_and_soil_1(
                state.wind,
                iteration,
                self._flag_guess,
                self._flag_iter,
                state.stc,
                self._stc0,
                self._stc1,
                state.islmsk,
            )

        self._post_loop(
            state.qsfc,
            self._qss,
            self._ddvel,
        )
