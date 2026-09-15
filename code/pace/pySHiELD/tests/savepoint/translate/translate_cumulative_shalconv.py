import copy

import numpy as np

from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import FORWARD, computation, interval
from ndsl.dsl.typing import (
    Bool,
    BoolFieldIJ,
    Float,
    FloatField,
    FloatFieldIJ,
    Int,
    IntField,
    IntFieldIJ,
    set_4d_field_size,
)
from pyshield.stencils.shallow_convection._config import ShallowConvectionConfig
from pyshield.stencils.shallow_convection.samfshalconv import (
    ScaleAwareMassFluxShallowConvection,
    col_diffs,
    comp_tendencies,
    exit_routine,
    feedback_control_update_mass_flux,
    init_col_arr,
    init_final,
    init_kbm_kmax,
    init_par_and_arr,
    init_tracers,
    pa_to_cb,
    stencil_ntrstatic0,
    stencil_ntrstatic1,
    stencil_ntrstatic2,
    stencil_static0,
    stencil_static1,
    stencil_static2,
    stencil_static3,
    stencil_static5,
    stencil_static7,
    stencil_static9,
    stencil_static10,
    stencil_static13,
    stencil_update_kbcon1_cnvflg,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py

FloatFieldShalConv = set_4d_field_size(7, Float)

SC_TRACER_DIM = "n_tracers_shal"


def set_pfld_kbcon(
    kbcon: IntFieldIJ, k_mask: IntField, pfld: FloatField, pfld_kbcon: FloatFieldIJ
):
    with computation(FORWARD), interval(...):
        if k_mask == kbcon:
            pfld_kbcon = pfld


class InitCols:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc > 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = SC_TRACER_DIM

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: int(self._ntr + 2),
            }
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        u1,
        v1,
        garea,
        cnvw,
        cnvc,
        cnvflg,
        kbcon,
        kb,
        ktcon,
        ktconn,
        pdot,
        qlko_ktcon,
        edt,
        aa1,
        cina,
        vshear,
        gdx,
        ps,
        prsl,
        del0,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            ps,
            prsl,
            del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            kbcon,
            kb,
            ktcon,
            ktconn,
            pdot,
            rn,
            qlko_ktcon,
            edt,
            aa1,
            cina,
            vshear,
            gdx,
            garea,
        )
        conv_a = copy.deepcopy(cnvflg)
        conv_b = np.full(cnvflg.shape, True)
        cols = col_diffs(cnvflg[3:-4, 3:-4], conv_b[3:-4, 3:-4])


class Static1:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc > 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = SC_TRACER_DIM

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: int(self._ntr + 2),
            }
        )

        # Tracers are kind of borked right now. In Fortran the water vapor is passed in
        # separately, while all others come in via the variable "qtr" which has
        # dimensions (i, j, k, n_tracers - 1), ice and liquid water are stored in
        # qtr[:, :, :, 0] and qtr[:, :, :, 1] and everything is reorganized to
        # accomodate that. Aerosols live in qtr[:, :, :, itc:].
        # If we're being straightforward about it we'd have our 4D tracer array with
        # special handling for ntvapor, ntiw, and ntcw (like we do for TKE), have an
        # `if n not in [ntvap, ntiw, ntcw]:` around the other tracer calls, and then
        # do something similar with the aerosols.
        # A better solution would be to have the attributes we want accessible easily
        # so we can send the aerosols into the aerosol calculations by attribute, and
        # similarly except (or invoke) vapor etc. from the other calculations.

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type=Float):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate arrays

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._heo_kb = make_quantity_2D()
        self._drag = make_quantity()
        self._ps = make_quantity_2D()
        self._prsl = make_quantity()
        self._del0 = make_quantity()
        self._ktcon = make_quantity_2D(Int)
        self._ktconn = make_quantity_2D(Int)
        self._pdot = make_quantity_2D()
        self._qlko_ktcon = make_quantity_2D()
        self._edt = make_quantity_2D()
        self._aa1 = make_quantity_2D()
        self._cina = make_quantity_2D()
        self._vshear = make_quantity_2D()
        self._gdx = make_quantity_2D()
        self._c0 = make_quantity_2D()
        self._c0t = make_quantity()
        self._tx1 = make_quantity_2D()
        self._kpbl = make_quantity_2D(Int)
        self._zo = make_quantity()
        self._zi = make_quantity()
        self._pfld = make_quantity()
        self._eta = make_quantity()
        self._hcko = make_quantity()
        self._qcko = make_quantity()
        self._qrcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._dbyo = make_quantity()
        self._pwo = make_quantity()
        self._dellal = make_quantity()
        self._to = make_quantity()
        self._qo = make_quantity()
        self._uo = make_quantity()
        self._vo = make_quantity()
        self._wu2 = make_quantity()
        self._buo = make_quantity()
        self._cnvwt = make_quantity()
        self._qeso = make_quantity()
        self._hmax = make_quantity_2D()
        self._po = make_quantity()

        self._ctr = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ctro = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ecko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_par_and_arr = stencil_factory.from_dims_halo(
            func=init_par_and_arr,
            externals={
                "asolfac": self._asolfac,
                "c0s": self._c0s,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_kbm_kmax = stencil_factory.from_dims_halo(
            func=init_kbm_kmax,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_final = stencil_factory.from_dims_halo(
            func=init_final,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_tracers = stencil_factory.from_dims_halo(
            func=init_tracers,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static0 = stencil_factory.from_dims_halo(
            func=stencil_static0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static1 = stencil_factory.from_dims_halo(
            func=stencil_static1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic0 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        u1,
        v1,
        garea,
        cnvw,
        cnvc,
        cnvflg,
        flg,
        kbcon,
        kmax,
        kbm,
        kb,
        heo,
        heso,
        hmax,
        kpbl,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            self._del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            kbcon,
            kb,
            self._ktcon,
            self._ktconn,
            self._pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            self._aa1,
            self._cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg):
            return

        conv_a = copy.deepcopy(cnvflg)
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            self._c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            kbm,
            kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            kbm,
            kmax,
            flg,
            cnvflg,
            kpbl,
            self._prsl,
            self._zo,
            phil,
            self._zi,
            self._pfld,
            self._eta,
            self._hcko,
            self._qcko,
            self._qrcko,
            self._ucko,
            self._vcko,
            self._dbyo,
            self._pwo,
            self._dellal,
            self._to,
            self._qo,
            self._uo,
            self._vo,
            self._wu2,
            self._buo,
            self._drag,
            self._cnvwt,
            self._qeso,
            heo,
            heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            hmax,
            heo,
            kb,
            self._k_mask,
            kpbl,
            kmax,
            self._zo,
            self._to,
            self._qeso,
            self._qo,
            self._po,
            self._uo,
            self._vo,
            heso,
            self._pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            flg,
            kbcon,
            kmax,
            self._k_mask,
            kbm,
            kb,
            self._heo_kb,
            heo,
            heso,
        )
        # breakpoint()
        # cnvflg[3:-4,3:-4][16,1] = True
        conv_b = copy.deepcopy(cnvflg)

        columns = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])


class Static2:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc > 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = SC_TRACER_DIM

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: int(self._ntr + 2),
            }
        )

        # Tracers are kind of borked right now. In Fortran the water vapor is passed in
        # separately, while all others come in via the variable "qtr" which has
        # dimensions (i, j, k, n_tracers - 1), ice and liquid water are stored in
        # qtr[:, :, :, 0] and qtr[:, :, :, 1] and everything is reorganized to
        # accomodate that. Aerosols live in qtr[:, :, :, itc:].
        # If we're being straightforward about it we'd have our 4D tracer array with
        # special handling for ntvapor, ntiw, and ntcw (like we do for TKE), have an
        # `if n not in [ntvap, ntiw, ntcw]:` around the other tracer calls, and then
        # do something similar with the aerosols.
        # A better solution would be to have the attributes we want accessible easily
        # so we can send the aerosols into the aerosol calculations by attribute, and
        # similarly except (or invoke) vapor etc. from the other calculations.

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type=Float):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate arrays

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._heo_kb = make_quantity_2D()
        self._drag = make_quantity()
        self._ps = make_quantity_2D()
        self._prsl = make_quantity()
        self._del0 = make_quantity()
        self._kbcon = make_quantity_2D(Int)
        self._kbcon1 = make_quantity_2D(Int)
        self._kb = make_quantity_2D(Int)
        self._ktcon = make_quantity_2D(Int)
        self._ktconn = make_quantity_2D(Int)
        self._pdot = make_quantity_2D()
        self._qlko_ktcon = make_quantity_2D()
        self._edt = make_quantity_2D()
        self._aa1 = make_quantity_2D()
        self._cina = make_quantity_2D()
        self._vshear = make_quantity_2D()
        self._gdx = make_quantity_2D()
        self._c0 = make_quantity_2D()
        self._c0t = make_quantity()
        self._kbm = make_quantity_2D(Int)
        self._kmax = make_quantity_2D(Int)
        self._tx1 = make_quantity_2D()
        self._kpbl = make_quantity_2D(Int)
        self._flg = make_quantity_2D(Bool)
        self._zo = make_quantity()
        self._zi = make_quantity()
        self._pfld = make_quantity()
        self._eta = make_quantity()
        self._hcko = make_quantity()
        self._qcko = make_quantity()
        self._qrcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._dbyo = make_quantity()
        self._pwo = make_quantity()
        self._dellal = make_quantity()
        self._to = make_quantity()
        self._qo = make_quantity()
        self._uo = make_quantity()
        self._vo = make_quantity()
        self._wu2 = make_quantity()
        self._buo = make_quantity()
        self._cnvwt = make_quantity()
        self._qeso = make_quantity()
        self._heo = make_quantity()
        self._heso = make_quantity()
        self._hmax = make_quantity_2D()
        self._po = make_quantity()
        self._pfld_kb = make_quantity_2D()
        self._pfld_kbcon = make_quantity_2D()
        self._pfld_kbcon1 = make_quantity_2D()
        self._sumx = make_quantity_2D()
        self._wc = make_quantity_2D()
        self._tkemean = make_quantity_2D()
        self._clamt = make_quantity_2D()
        self._xlamue = make_quantity()
        self._xlamud = make_quantity_2D()
        self._xmbmax = make_quantity_2D()
        self._ktcon1 = make_quantity_2D(Int)
        self._zi_kb = make_quantity_2D()
        self._zi_ktcon = make_quantity_2D()
        self._zi_kbcon = make_quantity_2D()
        self._dellah = make_quantity()
        self._dellaq = make_quantity()
        self._dellau = make_quantity()
        self._dellav = make_quantity()
        self._dtconv = make_quantity_2D()
        self._tauadv = make_quantity_2D()
        self._xmb = make_quantity_2D()
        self._sigmagfm = make_quantity_2D()
        self._scaldfunc = make_quantity_2D()
        self._umean = make_quantity_2D()
        self._delhbar = make_quantity_2D()
        self._delqbar = make_quantity_2D()
        self._deltbar = make_quantity_2D()
        self._delubar = make_quantity_2D()
        self._delvbar = make_quantity_2D()
        self._qcond = make_quantity_2D()
        self._rntot = make_quantity_2D()
        self._delqev = make_quantity_2D()
        self._delq2 = make_quantity_2D()
        self._deltv = make_quantity_2D()
        self._delq = make_quantity_2D()
        self._qevap = make_quantity_2D()

        self._ctr = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ctro = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ecko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._dellae = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._delebar = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_par_and_arr = stencil_factory.from_dims_halo(
            func=init_par_and_arr,
            externals={
                "asolfac": self._asolfac,
                "c0s": self._c0s,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_kbm_kmax = stencil_factory.from_dims_halo(
            func=init_kbm_kmax,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_final = stencil_factory.from_dims_halo(
            func=init_final,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_tracers = stencil_factory.from_dims_halo(
            func=init_tracers,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static0 = stencil_factory.from_dims_halo(
            func=stencil_static0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic0 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static1 = stencil_factory.from_dims_halo(
            func=stencil_static1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static2 = stencil_factory.from_dims_halo(
            func=stencil_static2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        u1,
        v1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        garea,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        cnvw,
        cnvc,
        cnvflg,
        pdot,
        kbcon,
        kb,
        pfld,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            self._del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            kbcon,
            kb,
            self._ktcon,
            self._ktconn,
            pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            self._aa1,
            self._cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg):
            return

        conv_a = copy.deepcopy(cnvflg)
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            self._c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            self._kbm,
            self._kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            self._kbm,
            self._kmax,
            self._flg,
            cnvflg,
            self._kpbl,
            self._prsl,
            self._zo,
            phil,
            self._zi,
            pfld,
            self._eta,
            self._hcko,
            self._qcko,
            self._qrcko,
            self._ucko,
            self._vcko,
            self._dbyo,
            self._pwo,
            self._dellal,
            self._to,
            self._qo,
            self._uo,
            self._vo,
            self._wu2,
            self._buo,
            self._drag,
            self._cnvwt,
            self._qeso,
            self._heo,
            self._heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            self._hmax,
            self._heo,
            kb,
            self._k_mask,
            self._kpbl,
            self._kmax,
            self._zo,
            self._to,
            self._qeso,
            self._qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            self._flg,
            kbcon,
            self._kmax,
            self._k_mask,
            self._kbm,
            kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )

        if exit_routine(cnvflg):
            return

        conv_b = copy.deepcopy(cnvflg)

        columns = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._stencil_static2(
            cnvflg,
            pdot,
            dot,
            islimsk,
            self._k_mask,
            kbcon,
            kb,
            pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )


class UpdateKB9:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc > 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = SC_TRACER_DIM

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: int(self._ntr + 2),
            }
        )

        # Tracers are kind of borked right now. In Fortran the water vapor is passed in
        # separately, while all others come in via the variable "qtr" which has
        # dimensions (i, j, k, n_tracers - 1), ice and liquid water are stored in
        # qtr[:, :, :, 0] and qtr[:, :, :, 1] and everything is reorganized to
        # accomodate that. Aerosols live in qtr[:, :, :, itc:].
        # If we're being straightforward about it we'd have our 4D tracer array with
        # special handling for ntvapor, ntiw, and ntcw (like we do for TKE), have an
        # `if n not in [ntvap, ntiw, ntcw]:` around the other tracer calls, and then
        # do something similar with the aerosols.
        # A better solution would be to have the attributes we want accessible easily
        # so we can send the aerosols into the aerosol calculations by attribute, and
        # similarly except (or invoke) vapor etc. from the other calculations.

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type=Float):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate arrays

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._heo_kb = make_quantity_2D()
        self._drag = make_quantity()
        self._ps = make_quantity_2D()
        self._prsl = make_quantity()
        self._del0 = make_quantity()
        self._kbcon = make_quantity_2D(Int)
        self._kbcon1 = make_quantity_2D(Int)
        self._kb = make_quantity_2D(Int)
        self._ktcon = make_quantity_2D(Int)
        self._ktconn = make_quantity_2D(Int)
        self._pdot = make_quantity_2D()
        self._qlko_ktcon = make_quantity_2D()
        self._edt = make_quantity_2D()
        self._aa1 = make_quantity_2D()
        self._cina = make_quantity_2D()
        self._vshear = make_quantity_2D()
        self._gdx = make_quantity_2D()
        self._c0 = make_quantity_2D()
        self._c0t = make_quantity()
        self._kbm = make_quantity_2D(Int)
        self._kmax = make_quantity_2D(Int)
        self._tx1 = make_quantity_2D()
        self._kpbl = make_quantity_2D(Int)
        self._flg = make_quantity_2D(Bool)
        self._zo = make_quantity()
        self._zi = make_quantity()
        self._pfld = make_quantity()
        self._eta = make_quantity()
        self._hcko = make_quantity()
        self._qcko = make_quantity()
        self._qrcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._dbyo = make_quantity()
        self._pwo = make_quantity()
        self._dellal = make_quantity()
        self._to = make_quantity()
        self._qo = make_quantity()
        self._uo = make_quantity()
        self._vo = make_quantity()
        self._wu2 = make_quantity()
        self._buo = make_quantity()
        self._cnvwt = make_quantity()
        self._qeso = make_quantity()
        self._heo = make_quantity()
        self._heso = make_quantity()
        self._hmax = make_quantity_2D()
        self._po = make_quantity()
        self._pfld_kb = make_quantity_2D()
        self._pfld_kbcon = make_quantity_2D()
        self._pfld_kbcon1 = make_quantity_2D()
        self._sumx = make_quantity_2D()
        self._wc = make_quantity_2D()
        self._tkemean = make_quantity_2D()
        self._clamt = make_quantity_2D()
        self._xlamue = make_quantity()
        self._xlamud = make_quantity_2D()
        self._xmbmax = make_quantity_2D()
        self._ktcon1 = make_quantity_2D(Int)
        self._zi_kb = make_quantity_2D()
        self._zi_ktcon = make_quantity_2D()
        self._zi_kbcon = make_quantity_2D()
        self._dellah = make_quantity()
        self._dellaq = make_quantity()
        self._dellau = make_quantity()
        self._dellav = make_quantity()
        self._dtconv = make_quantity_2D()
        self._tauadv = make_quantity_2D()
        self._xmb = make_quantity_2D()
        self._sigmagfm = make_quantity_2D()
        self._scaldfunc = make_quantity_2D()
        self._umean = make_quantity_2D()
        self._delhbar = make_quantity_2D()
        self._delqbar = make_quantity_2D()
        self._deltbar = make_quantity_2D()
        self._delubar = make_quantity_2D()
        self._delvbar = make_quantity_2D()
        self._qcond = make_quantity_2D()
        self._rntot = make_quantity_2D()
        self._delqev = make_quantity_2D()
        self._delq2 = make_quantity_2D()
        self._deltv = make_quantity_2D()
        self._delq = make_quantity_2D()
        self._qevap = make_quantity_2D()
        self._ptem = make_quantity_2D()

        self._ctr = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ctro = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ecko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._dellae = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._delebar = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_par_and_arr = stencil_factory.from_dims_halo(
            func=init_par_and_arr,
            externals={
                "asolfac": self._asolfac,
                "c0s": self._c0s,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_kbm_kmax = stencil_factory.from_dims_halo(
            func=init_kbm_kmax,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_final = stencil_factory.from_dims_halo(
            func=init_final,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_tracers = stencil_factory.from_dims_halo(
            func=init_tracers,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static0 = stencil_factory.from_dims_halo(
            func=stencil_static0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static1 = stencil_factory.from_dims_halo(
            func=stencil_static1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static2 = stencil_factory.from_dims_halo(
            func=stencil_static2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static3 = stencil_factory.from_dims_halo(
            func=stencil_static3,
            externals={
                "ntk": self._ntk,
                "clam": self._clam,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic0 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static5 = stencil_factory.from_dims_halo(
            func=stencil_static5,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic1 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static7 = stencil_factory.from_dims_halo(
            func=stencil_static7,
            externals={"pgcon": self._pgcon},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic2 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_update_kbcon1_cnvflg = stencil_factory.from_dims_halo(
            func=stencil_update_kbcon1_cnvflg,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static9 = stencil_factory.from_dims_halo(
            func=stencil_static9,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        u1,
        v1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        garea,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        cnvw,
        cnvc,
        dbyo,
        cnvflg,
        kmax,
        kbm,
        kbcon,
        kbcon1,
        flg,
        pfld,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            self._del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            kbcon,
            self._kb,
            self._ktcon,
            self._ktconn,
            self._pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            self._aa1,
            self._cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg)
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            self._c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            kbm,
            kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            kbm,
            kmax,
            flg,
            cnvflg,
            self._kpbl,
            self._prsl,
            self._zo,
            phil,
            self._zi,
            pfld,
            self._eta,
            self._hcko,
            self._qcko,
            self._qrcko,
            self._ucko,
            self._vcko,
            dbyo,
            self._pwo,
            self._dellal,
            self._to,
            self._qo,
            self._uo,
            self._vo,
            self._wu2,
            self._buo,
            self._drag,
            self._cnvwt,
            self._qeso,
            self._heo,
            self._heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            self._hmax,
            self._heo,
            self._kb,
            self._k_mask,
            self._kpbl,
            kmax,
            self._zo,
            self._to,
            self._qeso,
            self._qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            flg,
            kbcon,
            kmax,
            self._k_mask,
            kbm,
            self._kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg)

        columns = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._stencil_static2(
            cnvflg,
            self._pdot,
            dot,
            islimsk,
            self._k_mask,
            kbcon,
            self._kb,
            pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg)

        columns = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._stencil_static3(
            self._sumx,
            self._tkemean,
            cnvflg,
            self._k_mask,
            self._kb,
            kbcon,
            self._zo,
            qtr,
            self._clamt,
        )

        self._stencil_static5(
            cnvflg,
            self._xlamue,
            self._clamt,
            self._zi,
            self._xlamud,
            self._k_mask,
            kbcon,
            self._kb,
            self._eta,
            self._ktconn,
            kmax,
            kbm,
            self._hcko,
            self._ucko,
            self._vcko,
            self._heo,
            self._uo,
            self._vo,
            self._ptem,
            flg,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic1(
                    cnvflg,
                    self._k_mask,
                    self._kb,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static7(
            cnvflg,
            self._k_mask,
            self._kb,
            kmax,
            self._zi,
            self._xlamue,
            self._xlamud,
            self._hcko,
            self._heo,
            dbyo,
            self._heso,
            self._ucko,
            self._uo,
            self._vcko,
            self._vo,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic2(
                    cnvflg,
                    self._k_mask,
                    self._kb,
                    kmax,
                    self._zi,
                    self._xlamue,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_update_kbcon1_cnvflg(
            dbyo,
            cnvflg,
            kmax,
            kbm,
            kbcon,
            kbcon1,
            flg,
            self._k_mask,
        )
        conv_b = copy.deepcopy(cnvflg)

        columns = col_diffs(conv_a[3:-4, 3:-4], conv_b[3:-4, 3:-4])

        self._stencil_static9(
            cnvflg,
            pfld,
            self._pfld_kbcon,
            self._pfld_kbcon1,
            self._k_mask,
            kbcon1,
        )


class Static10:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc > 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = SC_TRACER_DIM

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: int(self._ntr + 2),
            }
        )

        # Tracers are kind of borked right now. In Fortran the water vapor is passed in
        # separately, while all others come in via the variable "qtr" which has
        # dimensions (i, j, k, n_tracers - 1), ice and liquid water are stored in
        # qtr[:, :, :, 0] and qtr[:, :, :, 1] and everything is reorganized to
        # accomodate that. Aerosols live in qtr[:, :, :, itc:].
        # If we're being straightforward about it we'd have our 4D tracer array with
        # special handling for ntvapor, ntiw, and ntcw (like we do for TKE), have an
        # `if n not in [ntvap, ntiw, ntcw]:` around the other tracer calls, and then
        # do something similar with the aerosols.
        # A better solution would be to have the attributes we want accessible easily
        # so we can send the aerosols into the aerosol calculations by attribute, and
        # similarly except (or invoke) vapor etc. from the other calculations.

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type=Float):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate arrays

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._heo_kb = make_quantity_2D()
        self._drag = make_quantity()
        self._ps = make_quantity_2D()
        self._prsl = make_quantity()
        self._del0 = make_quantity()
        self._kbcon = make_quantity_2D(Int)
        self._ktcon = make_quantity_2D(Int)
        self._ktconn = make_quantity_2D(Int)
        self._qlko_ktcon = make_quantity_2D()
        self._edt = make_quantity_2D()
        self._aa1 = make_quantity_2D()
        self._vshear = make_quantity_2D()
        self._gdx = make_quantity_2D()
        self._c0 = make_quantity_2D()
        self._c0t = make_quantity()
        self._kbm = make_quantity_2D(Int)
        self._kmax = make_quantity_2D(Int)
        self._tx1 = make_quantity_2D()
        self._kpbl = make_quantity_2D(Int)
        self._flg = make_quantity_2D(Bool)
        self._zi = make_quantity()
        self._pfld = make_quantity()
        self._eta = make_quantity()
        self._hcko = make_quantity()
        self._qcko = make_quantity()
        self._qrcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._pwo = make_quantity()
        self._dellal = make_quantity()
        self._uo = make_quantity()
        self._vo = make_quantity()
        self._wu2 = make_quantity()
        self._buo = make_quantity()
        self._cnvwt = make_quantity()
        self._heo = make_quantity()
        self._heso = make_quantity()
        self._hmax = make_quantity_2D()
        self._po = make_quantity()
        self._pfld_kb = make_quantity_2D()
        self._pfld_kbcon = make_quantity_2D()
        self._pfld_kbcon1 = make_quantity_2D()
        self._sumx = make_quantity_2D()
        self._wc = make_quantity_2D()
        self._tkemean = make_quantity_2D()
        self._clamt = make_quantity_2D()
        self._xlamue = make_quantity()
        self._xlamud = make_quantity_2D()
        self._xmbmax = make_quantity_2D()
        self._ktcon1 = make_quantity_2D(Int)
        self._zi_kb = make_quantity_2D()
        self._zi_ktcon = make_quantity_2D()
        self._zi_kbcon = make_quantity_2D()
        self._dellah = make_quantity()
        self._dellaq = make_quantity()
        self._dellau = make_quantity()
        self._dellav = make_quantity()
        self._dtconv = make_quantity_2D()
        self._tauadv = make_quantity_2D()
        self._xmb = make_quantity_2D()
        self._sigmagfm = make_quantity_2D()
        self._scaldfunc = make_quantity_2D()
        self._umean = make_quantity_2D()
        self._delhbar = make_quantity_2D()
        self._delqbar = make_quantity_2D()
        self._deltbar = make_quantity_2D()
        self._delubar = make_quantity_2D()
        self._delvbar = make_quantity_2D()
        self._qcond = make_quantity_2D()
        self._rntot = make_quantity_2D()
        self._delqev = make_quantity_2D()
        self._delq2 = make_quantity_2D()
        self._deltv = make_quantity_2D()
        self._delq = make_quantity_2D()
        self._qevap = make_quantity_2D()
        self._ptem = make_quantity_2D()

        self._ctr = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ctro = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ecko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._dellae = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._delebar = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_par_and_arr = stencil_factory.from_dims_halo(
            func=init_par_and_arr,
            externals={
                "asolfac": self._asolfac,
                "c0s": self._c0s,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_kbm_kmax = stencil_factory.from_dims_halo(
            func=init_kbm_kmax,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_final = stencil_factory.from_dims_halo(
            func=init_final,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_tracers = stencil_factory.from_dims_halo(
            func=init_tracers,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static0 = stencil_factory.from_dims_halo(
            func=stencil_static0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static1 = stencil_factory.from_dims_halo(
            func=stencil_static1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static2 = stencil_factory.from_dims_halo(
            func=stencil_static2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static3 = stencil_factory.from_dims_halo(
            func=stencil_static3,
            externals={
                "ntk": self._ntk,
                "clam": self._clam,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic0 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static5 = stencil_factory.from_dims_halo(
            func=stencil_static5,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic1 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static7 = stencil_factory.from_dims_halo(
            func=stencil_static7,
            externals={"pgcon": self._pgcon},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic2 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_update_kbcon1_cnvflg = stencil_factory.from_dims_halo(
            func=stencil_update_kbcon1_cnvflg,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static9 = stencil_factory.from_dims_halo(
            func=stencil_static9,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static10 = stencil_factory.from_dims_halo(
            func=stencil_static10,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        u1,
        v1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        garea,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        cnvw,
        cnvc,
        cina,
        cnvflg,
        kb,
        kbcon1,
        zo,
        qeso,
        to,
        dbyo,
        qo,
        pdot,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            self._del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            self._kbcon,
            kb,
            self._ktcon,
            self._ktconn,
            pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            self._aa1,
            cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a, conv_b)

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            self._c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            self._kbm,
            self._kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            self._kbm,
            self._kmax,
            self._flg,
            cnvflg,
            self._kpbl,
            self._prsl,
            zo,
            phil,
            self._zi,
            self._pfld,
            self._eta,
            self._hcko,
            self._qcko,
            self._qrcko,
            self._ucko,
            self._vcko,
            dbyo,
            self._pwo,
            self._dellal,
            to,
            qo,
            self._uo,
            self._vo,
            self._wu2,
            self._buo,
            self._drag,
            self._cnvwt,
            qeso,
            self._heo,
            self._heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            self._hmax,
            self._heo,
            kb,
            self._k_mask,
            self._kpbl,
            self._kmax,
            zo,
            to,
            qeso,
            qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            self._pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            self._flg,
            self._kbcon,
            self._kmax,
            self._k_mask,
            self._kbm,
            kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static2(
            cnvflg,
            pdot,
            dot,
            islimsk,
            self._k_mask,
            self._kbcon,
            kb,
            self._pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static3(
            self._sumx,
            self._tkemean,
            cnvflg,
            self._k_mask,
            kb,
            self._kbcon,
            zo,
            qtr,
            self._clamt,
        )

        self._stencil_static5(
            cnvflg,
            self._xlamue,
            self._clamt,
            self._zi,
            self._xlamud,
            self._k_mask,
            self._kbcon,
            kb,
            self._eta,
            self._ktconn,
            self._kmax,
            self._kbm,
            self._hcko,
            self._ucko,
            self._vcko,
            self._heo,
            self._uo,
            self._vo,
            self._ptem,
            self._flg,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic1(
                    cnvflg,
                    self._k_mask,
                    kb,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static7(
            cnvflg,
            self._k_mask,
            kb,
            self._kmax,
            self._zi,
            self._xlamue,
            self._xlamud,
            self._hcko,
            self._heo,
            dbyo,
            self._heso,
            self._ucko,
            self._uo,
            self._vcko,
            self._vo,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic2(
                    cnvflg,
                    self._k_mask,
                    kb,
                    self._kmax,
                    self._zi,
                    self._xlamue,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_update_kbcon1_cnvflg(
            dbyo,
            cnvflg,
            self._kmax,
            self._kbm,
            self._kbcon,
            kbcon1,
            self._flg,
            self._k_mask,
        )
        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static9(
            cnvflg,
            self._pfld,
            self._pfld_kbcon,
            self._pfld_kbcon1,
            self._k_mask,
            kbcon1,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static10(
            cina,
            cnvflg,
            self._k_mask,
            kb,
            kbcon1,
            zo,
            qeso,
            to,
            dbyo,
            qo,
            pdot,
            islimsk,
        )
        if exit_routine(cnvflg[:]):
            return


class Static11(ScaleAwareMassFluxShallowConvection):
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        super().__init__(stencil_factory, quantity_factory, config)

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        u1,
        v1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        garea,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        cnvw,
        cnvc,
        flg,
        cnvflg,
        ktcon,
        kbm,
        kbcon1,
        dbyo,
        kbcon,
        del0,
        xmbmax,
        aa1,
        kb,
        qcko,
        qo,
        qrcko,
        zi,
        qeso,
        to,
        xlamue,
        xlamud,
        eta,
        c0t,
        dellal,
        buo,
        drag,
        zo,
        pwo,
        cnvwt,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            kbcon,
            kb,
            ktcon,
            self._ktconn,
            self._pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            aa1,
            self._cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a, conv_b)

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            kbm,
            self._kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            kbm,
            self._kmax,
            flg,
            cnvflg,
            self._kpbl,
            self._prsl,
            zo,
            phil,
            zi,
            self._pfld,
            eta,
            self._hcko,
            qcko,
            qrcko,
            self._ucko,
            self._vcko,
            dbyo,
            pwo,
            dellal,
            to,
            qo,
            self._uo,
            self._vo,
            self._wu2,
            buo,
            drag,
            cnvwt,
            qeso,
            self._heo,
            self._heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            self._hmax,
            self._heo,
            kb,
            self._k_mask,
            self._kpbl,
            self._kmax,
            zo,
            to,
            qeso,
            qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            self._pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            flg,
            kbcon,
            self._kmax,
            self._k_mask,
            kbm,
            kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static2(
            cnvflg,
            self._pdot,
            dot,
            islimsk,
            self._k_mask,
            kbcon,
            kb,
            self._pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static3(
            self._sumx,
            self._tkemean,
            cnvflg,
            self._k_mask,
            kb,
            kbcon,
            zo,
            qtr,
            self._clamt,
        )

        self._stencil_static5(
            cnvflg,
            xlamue,
            self._clamt,
            zi,
            xlamud,
            self._k_mask,
            kbcon,
            kb,
            eta,
            self._ktconn,
            self._kmax,
            kbm,
            self._hcko,
            self._ucko,
            self._vcko,
            self._heo,
            self._uo,
            self._vo,
            self._ptem,
            flg,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic1(
                    cnvflg,
                    self._k_mask,
                    kb,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static7(
            cnvflg,
            self._k_mask,
            kb,
            self._kmax,
            zi,
            xlamue,
            xlamud,
            self._hcko,
            self._heo,
            dbyo,
            self._heso,
            self._ucko,
            self._uo,
            self._vcko,
            self._vo,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic2(
                    cnvflg,
                    self._k_mask,
                    kb,
                    self._kmax,
                    zi,
                    xlamue,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_update_kbcon1_cnvflg(
            dbyo,
            cnvflg,
            self._kmax,
            kbm,
            kbcon,
            kbcon1,
            flg,
            self._k_mask,
        )
        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static9(
            cnvflg,
            self._pfld,
            self._pfld_kbcon,
            self._pfld_kbcon1,
            self._k_mask,
            kbcon1,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static10(
            self._cina,
            cnvflg,
            self._k_mask,
            kb,
            kbcon1,
            zo,
            qeso,
            to,
            dbyo,
            qo,
            self._pdot,
            islimsk,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static11(
            flg,
            cnvflg,
            ktcon,
            kbm,
            kbcon1,
            dbyo,
            kbcon,
            del0,
            xmbmax,
            aa1,
            kb,
            qcko,
            qo,
            qrcko,
            zi,
            qeso,
            to,
            xlamue,
            xlamud,
            eta,
            c0t,
            dellal,
            buo,
            drag,
            zo,
            self._k_mask,
            pwo,
            cnvwt,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)


class Static12(ScaleAwareMassFluxShallowConvection):
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        super().__init__(stencil_factory, quantity_factory, config)

    def __call__(
        self,
        delp,
        prslp,
        psp,
        phil,
        qtr,
        q1,
        t1,
        u1,
        v1,
        rn,
        kbot,
        ktop,
        kcnv,
        islimsk,
        garea,
        dot,
        hpbl,
        ud_mf,
        dt_mf,
        cnvw,
        cnvc,
        flg,
        cnvflg,
        ktcon,
        kbm,
        kbcon1,
        dbyo,
        del0,
        aa1,
        qcko,
        qo,
        qrcko,
        zi,
        qeso,
        to,
        xlamue,
        xlamud,
        eta,
        c0t,
        dellal,
        buo,
        drag,
        zo,
        pwo,
        cnvwt,
        ktcon1,
        wu2,
        wc,
        sumx,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            psp,
            prslp,
            delp,
            self._ps,
            self._prsl,
            del0,
        )

        self._init_col_arr(
            kcnv,
            cnvflg,
            kbot,
            ktop,
            self._kbcon,
            self._kb,
            ktcon,
            self._ktconn,
            self._pdot,
            rn,
            self._qlko_ktcon,
            self._edt,
            aa1,
            self._cina,
            self._vshear,
            self._gdx,
            garea,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])
        conv_b = np.ones_like(conv_a)

        cols = col_diffs(conv_a, conv_b)

        self._init_par_and_arr(
            islimsk,
            self._c0,
            t1,
            c0t,
            cnvw,
            cnvc,
            ud_mf,
            dt_mf,
        )
        self._init_kbm_kmax(
            kbm,
            self._kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            kbm,
            self._kmax,
            flg,
            cnvflg,
            self._kpbl,
            self._prsl,
            zo,
            phil,
            zi,
            self._pfld,
            eta,
            self._hcko,
            qcko,
            qrcko,
            self._ucko,
            self._vcko,
            dbyo,
            pwo,
            dellal,
            to,
            qo,
            self._uo,
            self._vo,
            wu2,
            buo,
            drag,
            cnvwt,
            qeso,
            self._heo,
            self._heso,
            hpbl,
            t1,
            q1,
            u1,
            v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._init_tracers(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    qtr,
                    n_tracer,
                )

        self._stencil_static0(
            cnvflg,
            self._hmax,
            self._heo,
            self._kb,
            self._k_mask,
            self._kpbl,
            self._kmax,
            zo,
            to,
            qeso,
            qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            self._pfld,
        )
        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic0(
                    cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            cnvflg,
            flg,
            self._kbcon,
            self._kmax,
            self._k_mask,
            kbm,
            self._kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static2(
            cnvflg,
            self._pdot,
            dot,
            islimsk,
            self._k_mask,
            self._kbcon,
            self._kb,
            self._pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static3(
            sumx,
            self._tkemean,
            cnvflg,
            self._k_mask,
            self._kb,
            self._kbcon,
            zo,
            qtr,
            self._clamt,
        )

        self._stencil_static5(
            cnvflg,
            xlamue,
            self._clamt,
            zi,
            xlamud,
            self._k_mask,
            self._kbcon,
            self._kb,
            eta,
            self._ktconn,
            self._kmax,
            kbm,
            self._hcko,
            self._ucko,
            self._vcko,
            self._heo,
            self._uo,
            self._vo,
            self._ptem,
            flg,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic1(
                    cnvflg,
                    self._k_mask,
                    self._kb,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static7(
            cnvflg,
            self._k_mask,
            self._kb,
            self._kmax,
            zi,
            xlamue,
            xlamud,
            self._hcko,
            self._heo,
            dbyo,
            self._heso,
            self._ucko,
            self._uo,
            self._vcko,
            self._vo,
        )

        for n_tracer in range(self._ntr + 2):
            if (n_tracer != self._ntiw) and (n_tracer != self._ntcw):
                self._stencil_ntrstatic2(
                    cnvflg,
                    self._k_mask,
                    self._kb,
                    self._kmax,
                    zi,
                    xlamue,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_update_kbcon1_cnvflg(
            dbyo,
            cnvflg,
            self._kmax,
            kbm,
            self._kbcon,
            kbcon1,
            flg,
            self._k_mask,
        )
        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static9(
            cnvflg,
            self._pfld,
            self._pfld_kbcon,
            self._pfld_kbcon1,
            self._k_mask,
            kbcon1,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static10(
            self._cina,
            cnvflg,
            self._k_mask,
            self._kb,
            kbcon1,
            zo,
            qeso,
            to,
            dbyo,
            qo,
            self._pdot,
            islimsk,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_b = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._stencil_static11(
            flg,
            cnvflg,
            ktcon,
            kbm,
            kbcon1,
            dbyo,
            self._kbcon,
            del0,
            self._xmbmax,
            aa1,
            self._kb,
            qcko,
            qo,
            qrcko,
            zi,
            qeso,
            to,
            xlamue,
            xlamud,
            eta,
            c0t,
            dellal,
            buo,
            drag,
            zo,
            self._k_mask,
            pwo,
            cnvwt,
        )
        if exit_routine(cnvflg[:]):
            return

        conv_a = copy.deepcopy(cnvflg[:])

        columns = col_diffs(conv_a, conv_b)

        self._static12(
            cnvflg,
            aa1,
            flg,
            ktcon1,
            kbm,
            self._k_mask,
            ktcon,
            zo,
            qeso,
            to,
            dbyo,
            zi,
            xlamue,
            xlamud,
            qcko,
            qrcko,
            qo,
            eta,
            del0,
            c0t,
            pwo,
            cnvwt,
            buo,
            wu2,
            wc,
            sumx,
            kbcon1,
            drag,
            dellal,
        )


class FeedbackCtrl:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        dt2: Float,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ud_mf = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._dt_mf = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._feedback_control_update_mass_flux = stencil_factory.from_dims_halo(
            func=feedback_control_update_mass_flux,
            externals={"dt2": dt2},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        cnvflg: BoolFieldIJ,
        kmax: IntFieldIJ,
        kb: IntFieldIJ,
        ktcon: IntFieldIJ,
        flg: BoolFieldIJ,
        islimsk: IntFieldIJ,
        ktop: IntFieldIJ,
        kbot: IntFieldIJ,
        kbcon: IntFieldIJ,
        kcnv: IntFieldIJ,
        qeso: FloatField,
        pfld: FloatField,
        delhbar: FloatFieldIJ,
        delqbar: FloatFieldIJ,
        deltbar: FloatFieldIJ,
        delubar: FloatFieldIJ,
        delvbar: FloatFieldIJ,
        qcond: FloatFieldIJ,
        dellah: FloatField,
        dellaq: FloatField,
        t1: FloatField,
        xmb: FloatFieldIJ,
        q1: FloatField,
        u1: FloatField,
        dellau: FloatField,
        v1: FloatField,
        dellav: FloatField,
        del0: FloatField,
        rntot: FloatFieldIJ,
        delqev: FloatFieldIJ,
        delq2: FloatFieldIJ,
        pwo: FloatField,
        deltv: FloatFieldIJ,
        delq: FloatFieldIJ,
        qevap: FloatFieldIJ,
        rn: FloatFieldIJ,
        edt: FloatFieldIJ,
        cnvw: FloatField,
        cnvwt: FloatField,
        cnvc: FloatField,
        eta: FloatField,
    ):
        self._feedback_control_update_mass_flux(
            cnvflg,
            self._k_mask,
            kmax,
            kb,
            ktcon,
            flg,
            islimsk,
            ktop,
            kbot,
            kbcon,
            kcnv,
            qeso,
            pfld,
            delhbar,
            delqbar,
            deltbar,
            delubar,
            delvbar,
            qcond,
            dellah,
            dellaq,
            t1,
            xmb,
            q1,
            u1,
            dellau,
            v1,
            dellav,
            del0,
            rntot,
            delqev,
            delq2,
            pwo,
            deltv,
            delq,
            qevap,
            rn,
            edt,
            cnvw,
            cnvwt,
            cnvc,
            self._ud_mf,
            self._dt_mf,
            eta,
        )


class SC13:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        ncloud: Int,
    ):
        grid_indexing = stencil_factory.grid_indexing
        self._ncloud = ncloud
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k
        self._stencil_static13 = stencil_factory.from_dims_halo(
            func=stencil_static13,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        cnvflg,
        ktcon,
        qeso,
        to,
        dbyo,
        qcko,
        qlko_ktcon,
    ):
        if self._ncloud > 0:
            self._stencil_static13(
                cnvflg,
                self._k_mask,
                ktcon,
                qeso,
                to,
                dbyo,
                qcko,
                qlko_ktcon,
            )


class CompTendencies:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        dt2: Float,
    ):
        grid_indexing = stencil_factory.grid_indexing
        self._dt2 = dt2
        self._zi_ktcon = quantity_factory.zeros(
            [I_DIM, J_DIM], units="unknown", dtype=Float
        )
        self._zi_kbcon = quantity_factory.zeros(
            [I_DIM, J_DIM], units="unknown", dtype=Float
        )
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k
        self._comp_tendencies = stencil_factory.from_dims_halo(
            func=comp_tendencies,
            externals={"dt2": self._dt2},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        cnvflg,
        kbcon1,
        kbcon,
        ktcon1,
        ktcon,
        kb,
        kmax,
        dellah,
        dellaq,
        dellau,
        dellav,
        del0,
        zi,
        heo,
        qo,
        xlamue,
        xlamud,
        eta,
        hcko,
        qrcko,
        uo,
        ucko,
        vo,
        vcko,
        qcko,
        dellal,
        qlko_ktcon,
        wc,
        gdx,
        dtconv,
        u1,
        v1,
        po,
        to,
        tauadv,
        xmb,
        sigmagfm,
        garea,
        scaldfunc,
        xmbmax,
        sumx,
        umean,
    ):
        self._comp_tendencies(
            cnvflg,
            self._k_mask,
            kmax,
            kb,
            ktcon,
            ktcon1,
            kbcon1,
            kbcon,
            dellah,
            dellaq,
            dellau,
            dellav,
            del0,
            zi,
            self._zi_ktcon,
            self._zi_kbcon,
            heo,
            qo,
            xlamue,
            xlamud,
            eta,
            hcko,
            qrcko,
            uo,
            ucko,
            vo,
            vcko,
            qcko,
            dellal,
            qlko_ktcon,
            wc,
            gdx,
            dtconv,
            u1,
            v1,
            po,
            to,
            tauadv,
            xmb,
            sigmagfm,
            garea,
            scaldfunc,
            xmbmax,
            sumx,
            umean,
        )


class TranslateInitCol(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "icol_delp", "shield": True},
            "prslp": {"serialname": "icol_prslp", "shield": True},
            "psp": {"serialname": "icol_psp", "shield": True},
            "phil": {"serialname": "icol_phil", "shield": True},
            "qtr": {"serialname": "icol_qtr", "shield": True},
            "q1": {"serialname": "icol_q1", "shield": True},
            "t1": {"serialname": "icol_t1", "shield": True},
            "u1": {"serialname": "icol_u1", "shield": True},
            "v1": {"serialname": "icol_v1", "shield": True},
            "rn": {"serialname": "icol_rn", "shield": True},
            "kbot": {"serialname": "icol_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "icol_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "icol_kcnv", "shield": True},
            "islimsk": {"serialname": "icol_islimsk", "shield": True},
            "garea": {"serialname": "icol_garea", "shield": True},
            "dot": {"serialname": "icol_dot", "shield": True},
            "hpbl": {"serialname": "icol_hpbl", "shield": True},
            "ud_mf": {"serialname": "icol_ud_mf", "shield": True},
            "dt_mf": {"serialname": "icol_dt_mf", "shield": True},
            "cnvw": {"serialname": "icol_cnvw", "shield": True},
            "cnvc": {"serialname": "icol_cnvc", "shield": True},
            "cnvflg": {"serialname": "icol_cnvflg", "shield": True},
            "kbcon": {
                "serialname": "icol_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "ktcon": {
                "serialname": "icol_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "ktconn": {
                "serialname": "icol_ktconn",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "icol_kb", "shield": True, "index_variable": True},
            "pdot": {"serialname": "icol_pdot", "shield": True},
            "qlko_ktcon": {"serialname": "icol_qlko_ktcon", "shield": True},
            "edt": {"serialname": "icol_edt", "shield": True},
            "aa1": {"serialname": "icol_aa1", "shield": True},
            "cina": {"serialname": "icol_cina", "shield": True},
            "vshear": {"serialname": "icol_vshear", "shield": True},
            "gdx": {"serialname": "icol_gdx", "shield": True},
            "ps": {"serialname": "icol_ps", "shield": True},
            "prsl": {"serialname": "icol_prsl", "shield": True},
            "del0": {"serialname": "icol_del", "shield": True},
        }
        self.in_vars["parameters"] = [
            "icol_clam",
            "icol_c0s",
            "icol_c1",
            "icol_ncloud",
            "icol_pgcon",
            "icol_asolfac",
            "icol_delt",
            "icol_itc",
            "icol_ntc",
            "icol_ntk",
            "icol_ntr",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "icol_cnvflg", "shield": True},
            "kbcon": {
                "serialname": "icol_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "ktcon": {
                "serialname": "icol_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "ktconn": {
                "serialname": "icol_ktconn",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "icol_kb", "shield": True, "index_variable": True},
            "pdot": {"serialname": "icol_pdot", "shield": True},
            "qlko_ktcon": {"serialname": "icol_qlko_ktcon", "shield": True},
            "edt": {"serialname": "icol_edt", "shield": True},
            "aa1": {"serialname": "icol_aa1", "shield": True},
            "cina": {"serialname": "icol_cina", "shield": True},
            "vshear": {"serialname": "icol_vshear", "shield": True},
            "gdx": {"serialname": "icol_gdx", "shield": True},
            "ps": {"serialname": "icol_ps", "shield": True},
            "prsl": {"serialname": "icol_prsl", "shield": True},
            "del0": {"serialname": "icol_del", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("icol_delt"),
            ntke=int(inputs.pop("icol_ntk") - 1),
            nsamftrac=int(inputs.pop("icol_ntr")),
            ncld=int(inputs.pop("icol_ncloud")),
            ntchm=int(inputs.pop("icol_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("icol_itc") - 1),
            clam_shal=inputs.pop("icol_clam"),
            c0s_shal=inputs.pop("icol_c0s"),
            c1_shal=inputs.pop("icol_c1"),
            pgcon_shal=inputs.pop("icol_pgcon"),
            asolfac_shal=inputs.pop("icol_asolfac"),
        )
        self.compute_func = InitCols(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateStatic1(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "cnvflg": {"serialname": "sc1_cnvflg", "shield": True},
            "flg": {"serialname": "sc1_flg", "shield": True},
            "kbcon": {
                "serialname": "sc1_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kmax": {"serialname": "sc1_kmax", "shield": True, "index_variable": True},
            "kbm": {"serialname": "sc1_kbm", "shield": True, "index_variable": True},
            "kb": {"serialname": "sc1_kb", "shield": True, "index_variable": True},
            "heo": {"serialname": "sc1_heo", "shield": True},
            "heso": {"serialname": "sc1_heso", "shield": True},
            "hmax": {"serialname": "sc1_hmax", "shield": True},
            "kpbl": {"serialname": "sc1_kpbl", "shield": True, "index_variable": True},
            "delp": {"serialname": "sc1_delp", "shield": True},
            "prslp": {"serialname": "sc1_prslp", "shield": True},
            "psp": {"serialname": "sc1_psp", "shield": True},
            "phil": {"serialname": "sc1_phil", "shield": True},
            "qtr": {"serialname": "sc1_qtr", "shield": True},
            "q1": {"serialname": "sc1_q1", "shield": True},
            "t1": {"serialname": "sc1_t1", "shield": True},
            "u1": {"serialname": "sc1_u1", "shield": True},
            "v1": {"serialname": "sc1_v1", "shield": True},
            "rn": {"serialname": "sc1_rn", "shield": True},
            "kbot": {"serialname": "sc1_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc1_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc1_kcnv", "shield": True},
            "islimsk": {"serialname": "sc1_islimsk", "shield": True},
            "garea": {"serialname": "sc1_garea", "shield": True},
            "dot": {"serialname": "sc1_dot", "shield": True},
            "hpbl": {"serialname": "sc1_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc1_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc1_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc1_cnvw", "shield": True},
            "cnvc": {"serialname": "sc1_cnvc", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sc1_clam",
            "sc1_c0s",
            "sc1_c1",
            "sc1_ncloud",
            "sc1_pgcon",
            "sc1_asolfac",
            "sc1_delt",
            "sc1_itc",
            "sc1_ntc",
            "sc1_ntk",
            "sc1_ntr",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "sc1_cnvflg", "shield": True},
            "flg": {"serialname": "sc1_flg", "shield": True},
            "kbcon": {
                "serialname": "sc1_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kmax": {"serialname": "sc1_kmax", "shield": True, "index_variable": True},
            "kbm": {"serialname": "sc1_kbm", "shield": True, "index_variable": True},
            "kb": {"serialname": "sc1_kb", "shield": True, "index_variable": True},
            "heo": {"serialname": "sc1_heo", "shield": True},
            "heso": {"serialname": "sc1_heso", "shield": True},
            "hmax": {"serialname": "sc1_hmax", "shield": True},
            "kpbl": {"serialname": "sc1_kpbl", "shield": True, "index_variable": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("sc1_delt"),
            ntke=int(inputs.pop("sc1_ntk") - 1),
            nsamftrac=int(inputs.pop("sc1_ntr")),
            ncld=int(inputs.pop("sc1_ncloud")),
            ntchm=int(inputs.pop("sc1_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("sc1_itc") - 1),
            clam_shal=inputs.pop("sc1_clam"),
            c0s_shal=inputs.pop("sc1_c0s"),
            c1_shal=inputs.pop("sc1_c1"),
            pgcon_shal=inputs.pop("sc1_pgcon"),
            asolfac_shal=inputs.pop("sc1_asolfac"),
        )
        self.compute_func = Static1(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateStatic2(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "sc2_delp", "shield": True},
            "prslp": {"serialname": "sc2_prslp", "shield": True},
            "psp": {"serialname": "sc2_psp", "shield": True},
            "phil": {"serialname": "sc2_phil", "shield": True},
            "qtr": {"serialname": "sc2_qtr", "shield": True},
            "q1": {"serialname": "sc2_q1", "shield": True},
            "t1": {"serialname": "sc2_t1", "shield": True},
            "u1": {"serialname": "sc2_u1", "shield": True},
            "v1": {"serialname": "sc2_v1", "shield": True},
            "rn": {"serialname": "sc2_rn", "shield": True},
            "kbot": {"serialname": "sc2_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc2_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc2_kcnv", "shield": True},
            "islimsk": {"serialname": "sc2_islimsk", "shield": True},
            "garea": {"serialname": "sc2_garea", "shield": True},
            "dot": {"serialname": "sc2_dot", "shield": True},
            "hpbl": {"serialname": "sc2_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc2_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc2_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc2_cnvw", "shield": True},
            "cnvc": {"serialname": "sc2_cnvc", "shield": True},
            "cnvflg": {"serialname": "sc2_cnvflg", "shield": True},
            "pdot": {"serialname": "sc2_pdot", "shield": True},
            "kbcon": {
                "serialname": "sc2_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "sc2_kb", "shield": True, "index_variable": True},
            "pfld": {"serialname": "sc2_pfld", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sc2_clam",
            "sc2_c0s",
            "sc2_c1",
            "sc2_ncloud",
            "sc2_pgcon",
            "sc2_asolfac",
            "sc2_delt",
            "sc2_itc",
            "sc2_ntc",
            "sc2_ntk",
            "sc2_ntr",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "sc2_cnvflg", "shield": True},
            "pdot": {"serialname": "sc2_pdot", "shield": True},
            "dot": {"serialname": "sc2_dot", "shield": True},
            "islimsk": {"serialname": "sc2_islimsk", "shield": True},
            "kbcon": {
                "serialname": "sc2_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "sc2_kb", "shield": True, "index_variable": True},
            "pfld": {"serialname": "sc2_pfld", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("sc2_delt"),
            ntke=int(inputs.pop("sc2_ntk") - 1),
            nsamftrac=int(inputs.pop("sc2_ntr")),
            ncld=int(inputs.pop("sc2_ncloud")),
            ntchm=int(inputs.pop("sc2_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("sc2_itc") - 1),
            clam_shal=inputs.pop("sc2_clam"),
            c0s_shal=inputs.pop("sc2_c0s"),
            c1_shal=inputs.pop("sc2_c1"),
            pgcon_shal=inputs.pop("sc2_pgcon"),
            asolfac_shal=inputs.pop("sc2_asolfac"),
        )
        self.compute_func = Static2(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateUpdateKb9(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "uk9_delp", "shield": True},
            "prslp": {"serialname": "uk9_prslp", "shield": True},
            "psp": {"serialname": "uk9_psp", "shield": True},
            "phil": {"serialname": "uk9_phil", "shield": True},
            "qtr": {"serialname": "uk9_qtr", "shield": True},
            "q1": {"serialname": "uk9_q1", "shield": True},
            "t1": {"serialname": "uk9_t1", "shield": True},
            "u1": {"serialname": "uk9_u1", "shield": True},
            "v1": {"serialname": "uk9_v1", "shield": True},
            "rn": {"serialname": "uk9_rn", "shield": True},
            "kbot": {"serialname": "uk9_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "uk9_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "uk9_kcnv", "shield": True},
            "islimsk": {"serialname": "uk9_islimsk", "shield": True},
            "garea": {"serialname": "uk9_garea", "shield": True},
            "dot": {"serialname": "uk9_dot", "shield": True},
            "hpbl": {"serialname": "uk9_hpbl", "shield": True},
            "ud_mf": {"serialname": "uk9_ud_mf", "shield": True},
            "dt_mf": {"serialname": "uk9_dt_mf", "shield": True},
            "cnvw": {"serialname": "uk9_cnvw", "shield": True},
            "cnvc": {"serialname": "uk9_cnvc", "shield": True},
            "dbyo": {"serialname": "uk9_dbyo", "shield": True},
            "cnvflg": {"serialname": "uk9_cnvflg", "shield": True},
            "kmax": {"serialname": "uk9_kmax", "shield": True, "index_variable": True},
            "kbm": {"serialname": "uk9_kbm", "shield": True, "index_variable": True},
            "kbcon": {
                "serialname": "uk9_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kbcon1": {
                "serialname": "uk9_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "flg": {"serialname": "uk9_flg", "shield": True},
            "pfld": {"serialname": "uk9_pfld", "shield": True},
        }
        self.in_vars["parameters"] = [
            "uk9_clam",
            "uk9_c0s",
            "uk9_c1",
            "uk9_ncloud",
            "uk9_pgcon",
            "uk9_asolfac",
            "uk9_delt",
            "uk9_itc",
            "uk9_ntc",
            "uk9_ntk",
            "uk9_ntr",
        ]
        self.out_vars = {
            "dbyo": {"serialname": "uk9_dbyo", "shield": True},
            "cnvflg": {"serialname": "uk9_cnvflg", "shield": True},
            "kmax": {"serialname": "uk9_kmax", "shield": True, "index_variable": True},
            "kbm": {"serialname": "uk9_kbm", "shield": True, "index_variable": True},
            "kbcon": {
                "serialname": "uk9_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "kbcon1": {
                "serialname": "uk9_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "flg": {"serialname": "uk9_flg", "shield": True},
            "pfld": {"serialname": "uk9_pfld", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("uk9_delt"),
            ntke=int(inputs.pop("uk9_ntk") - 1),
            nsamftrac=int(inputs.pop("uk9_ntr")),
            ncld=int(inputs.pop("uk9_ncloud")),
            ntchm=int(inputs.pop("uk9_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("uk9_itc") - 1),
            clam_shal=inputs.pop("uk9_clam"),
            c0s_shal=inputs.pop("uk9_c0s"),
            c1_shal=inputs.pop("uk9_c1"),
            pgcon_shal=inputs.pop("uk9_pgcon"),
            asolfac_shal=inputs.pop("uk9_asolfac"),
        )
        self.compute_func = UpdateKB9(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateStatic10(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "sc10_delp", "shield": True},
            "prslp": {"serialname": "sc10_prslp", "shield": True},
            "psp": {"serialname": "sc10_psp", "shield": True},
            "phil": {"serialname": "sc10_phil", "shield": True},
            "qtr": {"serialname": "sc10_qtr", "shield": True},
            "q1": {"serialname": "sc10_q1", "shield": True},
            "t1": {"serialname": "sc10_t1", "shield": True},
            "u1": {"serialname": "sc10_u1", "shield": True},
            "v1": {"serialname": "sc10_v1", "shield": True},
            "rn": {"serialname": "sc10_rn", "shield": True},
            "kbot": {"serialname": "sc10_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc10_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc10_kcnv", "shield": True},
            "islimsk": {"serialname": "sc10_islimsk", "shield": True},
            "garea": {"serialname": "sc10_garea", "shield": True},
            "dot": {"serialname": "sc10_dot", "shield": True},
            "hpbl": {"serialname": "sc10_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc10_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc10_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc10_cnvw", "shield": True},
            "cnvc": {"serialname": "sc10_cnvc", "shield": True},
            "cina": {"serialname": "sc10_cina", "shield": True},
            "cnvflg": {"serialname": "sc10_cnvflg", "shield": True},
            "kb": {"serialname": "sc10_kb", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "sc10_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "zo": {"serialname": "sc10_zo", "shield": True},
            "qeso": {"serialname": "sc10_qeso", "shield": True},
            "to": {"serialname": "sc10_to", "shield": True},
            "dbyo": {"serialname": "sc10_dbyo", "shield": True},
            "qo": {"serialname": "sc10_qo", "shield": True},
            "pdot": {"serialname": "sc10_pdot", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sc10_clam",
            "sc10_c0s",
            "sc10_c1",
            "sc10_ncloud",
            "sc10_pgcon",
            "sc10_asolfac",
            "sc10_delt",
            "sc10_itc",
            "sc10_ntc",
            "sc10_ntk",
            "sc10_ntr",
        ]
        self.out_vars = {
            "cina": {"serialname": "sc10_cina", "shield": True},
            "cnvflg": {"serialname": "sc10_cnvflg", "shield": True},
            "kb": {"serialname": "sc10_kb", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "sc10_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "zo": {"serialname": "sc10_zo", "shield": True},
            "qeso": {"serialname": "sc10_qeso", "shield": True},
            "to": {"serialname": "sc10_to", "shield": True},
            "dbyo": {"serialname": "sc10_dbyo", "shield": True},
            "qo": {"serialname": "sc10_qo", "shield": True},
            "pdot": {"serialname": "sc10_pdot", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("sc10_delt"),
            ntke=int(inputs.pop("sc10_ntk") - 1),
            nsamftrac=int(inputs.pop("sc10_ntr")),
            ncld=int(inputs.pop("sc10_ncloud")),
            ntchm=int(inputs.pop("sc10_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("sc10_itc") - 1),
            clam_shal=inputs.pop("sc10_clam"),
            c0s_shal=inputs.pop("sc10_c0s"),
            c1_shal=inputs.pop("sc10_c1"),
            pgcon_shal=inputs.pop("sc10_pgcon"),
            asolfac_shal=inputs.pop("sc10_asolfac"),
        )
        self.compute_func = Static10(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateStatic11(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "sc11_delp", "shield": True},
            "prslp": {"serialname": "sc11_prslp", "shield": True},
            "psp": {"serialname": "sc11_psp", "shield": True},
            "phil": {"serialname": "sc11_phil", "shield": True},
            "qtr": {"serialname": "sc11_qtr", "shield": True},
            "q1": {"serialname": "sc11_q1", "shield": True},
            "t1": {"serialname": "sc11_t1", "shield": True},
            "u1": {"serialname": "sc11_u1", "shield": True},
            "v1": {"serialname": "sc11_v1", "shield": True},
            "rn": {"serialname": "sc11_rn", "shield": True},
            "kbot": {"serialname": "sc11_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc11_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc11_kcnv", "shield": True},
            "islimsk": {"serialname": "sc11_islimsk", "shield": True},
            "garea": {"serialname": "sc11_garea", "shield": True},
            "dot": {"serialname": "sc11_dot", "shield": True},
            "hpbl": {"serialname": "sc11_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc11_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc11_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc11_cnvw", "shield": True},
            "cnvc": {"serialname": "sc11_cnvc", "shield": True},
            "flg": {"serialname": "sc11_flg", "shield": True},
            "cnvflg": {"serialname": "sc11_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "sc11_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kbm": {"serialname": "sc11_kbm", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "sc11_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "dbyo": {"serialname": "sc11_dbyo", "shield": True},
            "kbcon": {
                "serialname": "sc11_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "del0": {"serialname": "sc11_del0", "shield": True},
            "xmbmax": {"serialname": "sc11_xmbmax", "shield": True},
            "aa1": {"serialname": "sc11_aa1", "shield": True},
            "kb": {
                "serialname": "sc11_kb",
                "shield": True,
                "index_variable": True,
            },
            "qcko": {"serialname": "sc11_qcko", "shield": True},
            "qo": {"serialname": "sc11_qo", "shield": True},
            "qrcko": {"serialname": "sc11_qrcko", "shield": True},
            "zi": {"serialname": "sc11_zi", "shield": True},
            "qeso": {"serialname": "sc11_qeso", "shield": True},
            "to": {"serialname": "sc11_to", "shield": True},
            "xlamue": {"serialname": "sc11_xlamue", "shield": True},
            "xlamud": {"serialname": "sc11_xlamud", "shield": True},
            "eta": {"serialname": "sc11_eta", "shield": True},
            "c0t": {"serialname": "sc11_c0t", "shield": True},
            "dellal": {"serialname": "sc11_dellal", "shield": True},
            "buo": {"serialname": "sc11_buo", "shield": True},
            "drag": {"serialname": "sc11_drag", "shield": True},
            "zo": {"serialname": "sc11_zo", "shield": True},
            "pwo": {"serialname": "sc11_pwo", "shield": True},
            "cnvwt": {"serialname": "sc11_cnvwt", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sc11_clam",
            "sc11_c0s",
            "sc11_c1",
            "sc11_ncloud",
            "sc11_pgcon",
            "sc11_asolfac",
            "sc11_delt",
            "sc11_itc",
            "sc11_ntc",
            "sc11_ntk",
            "sc11_ntr",
        ]
        self.out_vars = {
            "flg": {"serialname": "sc11_flg", "shield": True},
            "cnvflg": {"serialname": "sc11_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "sc11_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kbm": {"serialname": "sc11_kbm", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "sc11_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "dbyo": {"serialname": "sc11_dbyo", "shield": True},
            "kbcon": {
                "serialname": "sc11_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "del0": {"serialname": "sc11_del0", "shield": True},
            "xmbmax": {"serialname": "sc11_xmbmax", "shield": True},
            "aa1": {"serialname": "sc11_aa1", "shield": True},
            "kb": {"serialname": "sc11_kb", "shield": True, "index_variable": True},
            "qcko": {"serialname": "sc11_qcko", "shield": True},
            "qo": {"serialname": "sc11_qo", "shield": True},
            "qrcko": {"serialname": "sc11_qrcko", "shield": True},
            "zi": {"serialname": "sc11_zi", "shield": True},
            "qeso": {"serialname": "sc11_qeso", "shield": True},
            "to": {"serialname": "sc11_to", "shield": True},
            "xlamue": {"serialname": "sc11_xlamue", "shield": True},
            "xlamud": {"serialname": "sc11_xlamud", "shield": True},
            "eta": {"serialname": "sc11_eta", "shield": True},
            "c0t": {"serialname": "sc11_c0t", "shield": True},
            "dellal": {"serialname": "sc11_dellal", "shield": True},
            "buo": {"serialname": "sc11_buo", "shield": True},
            "drag": {"serialname": "sc11_drag", "shield": True},
            "zo": {"serialname": "sc11_zo", "shield": True},
            "pwo": {"serialname": "sc11_pwo", "shield": True},
            "cnvwt": {"serialname": "sc11_cnvwt", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("sc11_delt"),
            ntke=int(inputs.pop("sc11_ntk") - 1),
            nsamftrac=int(inputs.pop("sc11_ntr")),
            ncld=int(inputs.pop("sc11_ncloud")),
            ntchm=int(inputs.pop("sc11_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("sc11_itc") - 1),
            clam_shal=inputs.pop("sc11_clam"),
            c0s_shal=inputs.pop("sc11_c0s"),
            c1_shal=inputs.pop("sc11_c1"),
            pgcon_shal=inputs.pop("sc11_pgcon"),
            asolfac_shal=inputs.pop("sc11_asolfac"),
        )
        self.compute_func = Static11(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateStatic12(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "s12_delp", "shield": True},
            "prslp": {"serialname": "s12_prslp", "shield": True},
            "psp": {"serialname": "s12_psp", "shield": True},
            "phil": {"serialname": "s12_phil", "shield": True},
            "qtr": {"serialname": "s12_qtr", "shield": True},
            "q1": {"serialname": "s12_q1", "shield": True},
            "t1": {"serialname": "s12_t1", "shield": True},
            "u1": {"serialname": "s12_u1", "shield": True},
            "v1": {"serialname": "s12_v1", "shield": True},
            "rn": {"serialname": "s12_rn", "shield": True},
            "kbot": {"serialname": "s12_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "s12_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "s12_kcnv", "shield": True},
            "islimsk": {"serialname": "s12_islimsk", "shield": True},
            "garea": {"serialname": "s12_garea", "shield": True},
            "dot": {"serialname": "s12_dot", "shield": True},
            "hpbl": {"serialname": "s12_hpbl", "shield": True},
            "ud_mf": {"serialname": "s12_ud_mf", "shield": True},
            "dt_mf": {"serialname": "s12_dt_mf", "shield": True},
            "cnvw": {"serialname": "s12_cnvw", "shield": True},
            "cnvc": {"serialname": "s12_cnvc", "shield": True},
            "flg": {"serialname": "s12_flg", "shield": True},
            "cnvflg": {"serialname": "s12_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "s12_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kbm": {"serialname": "s12_kbm", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "s12_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "dbyo": {"serialname": "s12_dbyo", "shield": True},
            "del0": {"serialname": "s12_del0", "shield": True},
            "aa1": {"serialname": "s12_aa1", "shield": True},
            "qcko": {"serialname": "s12_qcko", "shield": True},
            "qo": {"serialname": "s12_qo", "shield": True},
            "qrcko": {"serialname": "s12_qrcko", "shield": True},
            "zi": {"serialname": "s12_zi", "shield": True},
            "qeso": {"serialname": "s12_qeso", "shield": True},
            "to": {"serialname": "s12_to", "shield": True},
            "xlamue": {"serialname": "s12_xlamue", "shield": True},
            "xlamud": {"serialname": "s12_xlamud", "shield": True},
            "eta": {"serialname": "s12_eta", "shield": True},
            "c0t": {"serialname": "s12_c0t", "shield": True},
            "dellal": {"serialname": "s12_dellal", "shield": True},
            "buo": {"serialname": "s12_buo", "shield": True},
            "drag": {"serialname": "s12_drag", "shield": True},
            "zo": {"serialname": "s12_zo", "shield": True},
            "pwo": {"serialname": "s12_pwo", "shield": True},
            "cnvwt": {"serialname": "s12_cnvwt", "shield": True},
            "ktcon1": {
                "serialname": "sct_ktcon1",
                "shield": True,
                "index_variable": True,
            },
            "wu2": {"serialname": "s12_wu2", "shield": True},
            "wc": {"serialname": "s12_wc", "shield": True},
            "sumx": {"serialname": "s12_sumx", "shield": True},
        }
        self.in_vars["parameters"] = [
            "s12_clam",
            "s12_c0s",
            "s12_c1",
            "s12_ncloud",
            "s12_pgcon",
            "s12_asolfac",
            "s12_delt",
            "s12_itc",
            "s12_ntc",
            "s12_ntk",
            "s12_ntr",
        ]
        self.out_vars = {
            "flg": {"serialname": "s12_flg", "shield": True},
            "cnvflg": {"serialname": "s12_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "s12_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kbm": {"serialname": "s12_kbm", "shield": True, "index_variable": True},
            "kbcon1": {
                "serialname": "s12_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "dbyo": {"serialname": "s12_dbyo", "shield": True},
            "del0": {"serialname": "s12_del0", "shield": True},
            "aa1": {"serialname": "s12_aa1", "shield": True},
            "qcko": {"serialname": "s12_qcko", "shield": True},
            "qo": {"serialname": "s12_qo", "shield": True},
            "qrcko": {"serialname": "s12_qrcko", "shield": True},
            "zi": {"serialname": "s12_zi", "shield": True},
            "qeso": {"serialname": "s12_qeso", "shield": True},
            "to": {"serialname": "s12_to", "shield": True},
            "xlamue": {"serialname": "s12_xlamue", "shield": True},
            "xlamud": {"serialname": "s12_xlamud", "shield": True},
            "eta": {"serialname": "s12_eta", "shield": True},
            "c0t": {"serialname": "s12_c0t", "shield": True},
            "dellal": {"serialname": "s12_dellal", "shield": True},
            "buo": {"serialname": "s12_buo", "shield": True},
            "drag": {"serialname": "s12_drag", "shield": True},
            "zo": {"serialname": "s12_zo", "shield": True},
            "pwo": {"serialname": "s12_pwo", "shield": True},
            "cnvwt": {"serialname": "s12_cnvwt", "shield": True},
            "ktcon1": {
                "serialname": "sct_ktcon1",
                "shield": True,
                "index_variable": True,
            },
            "wu2": {"serialname": "s12_wu2", "shield": True},
            "wc": {"serialname": "s12_wc", "shield": True},
            "sumx": {"serialname": "s12_sumx", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("s12_delt"),
            ntke=int(inputs.pop("s12_ntk") - 1),
            nsamftrac=int(inputs.pop("s12_ntr")),
            ncld=int(inputs.pop("s12_ncloud")),
            ntchm=int(inputs.pop("s12_ntc")),
            ntiw=0,
            ntcw=1,
            itc=int(inputs.pop("s12_itc") - 1),
            clam_shal=inputs.pop("s12_clam"),
            c0s_shal=inputs.pop("s12_c0s"),
            c1_shal=inputs.pop("s12_c1"),
            pgcon_shal=inputs.pop("s12_pgcon"),
            asolfac_shal=inputs.pop("s12_asolfac"),
        )
        self.compute_func = Static12(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateFeedbackCtrl(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "cnvflg": {"serialname": "fc_cnvflg", "shield": True},
            "kmax": {"serialname": "fc_kmax", "shield": True, "index_variable": True},
            "kb": {"serialname": "fc_kb", "shield": True, "index_variable": True},
            "ktcon": {"serialname": "fc_ktcon", "shield": True, "index_variable": True},
            "flg": {"serialname": "fc_flg", "shield": True},
            "islimsk": {"serialname": "fc_islimsk", "shield": True},
            "ktop": {"serialname": "fc_ktop", "shield": True, "index_variable": True},
            "kbot": {"serialname": "fc_kbot", "shield": True, "index_variable": True},
            "kbcon": {"serialname": "fc_kbcon", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "fc_kcnv", "shield": True},
            "qeso": {"serialname": "fc_qeso", "shield": True},
            "pfld": {"serialname": "fc_pfld", "shield": True},
            "delhbar": {"serialname": "fc_delhbar", "shield": True},
            "delqbar": {"serialname": "fc_delqbar", "shield": True},
            "deltbar": {"serialname": "fc_deltbar", "shield": True},
            "delubar": {"serialname": "fc_delubar", "shield": True},
            "delvbar": {"serialname": "fc_delvbar", "shield": True},
            "qcond": {"serialname": "fc_qcond", "shield": True},
            "dellah": {"serialname": "fc_dellah", "shield": True},
            "dellaq": {"serialname": "fc_dellaq", "shield": True},
            "t1": {"serialname": "fc_t1", "shield": True},
            "xmb": {"serialname": "fc_xmb", "shield": True},
            "q1": {"serialname": "fc_q1", "shield": True},
            "u1": {"serialname": "fc_u1", "shield": True},
            "dellau": {"serialname": "fc_dellau", "shield": True},
            "v1": {"serialname": "fc_v1", "shield": True},
            "dellav": {"serialname": "fc_dellav", "shield": True},
            "del0": {"serialname": "fc_del", "shield": True},
            "rntot": {"serialname": "fc_rntot", "shield": True},
            "delqev": {"serialname": "fc_delqev", "shield": True},
            "delq2": {"serialname": "fc_delq2", "shield": True},
            "pwo": {"serialname": "fc_pwo", "shield": True},
            "deltv": {"serialname": "fc_deltv", "shield": True},
            "delq": {"serialname": "fc_delq", "shield": True},
            "qevap": {"serialname": "fc_qevap", "shield": True},
            "rn": {"serialname": "fc_rn", "shield": True},
            "edt": {"serialname": "fc_edt", "shield": True},
            "cnvw": {"serialname": "fc_cnvw", "shield": True},
            "cnvwt": {"serialname": "fc_cnvwt", "shield": True},
            "cnvc": {"serialname": "fc_cnvc", "shield": True},
            "eta": {"serialname": "fc_eta", "shield": True},
        }
        self.in_vars["parameters"] = [
            "fc_dt2",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "fc_cnvflg", "shield": True},
            "kmax": {"serialname": "fc_kmax", "shield": True, "index_variable": True},
            "kb": {"serialname": "fc_kb", "shield": True, "index_variable": True},
            "ktcon": {"serialname": "fc_ktcon", "shield": True, "index_variable": True},
            "flg": {"serialname": "fc_flg", "shield": True},
            "islimsk": {"serialname": "fc_islimsk", "shield": True},
            "ktop": {"serialname": "fc_ktop", "shield": True, "index_variable": True},
            "kbot": {"serialname": "fc_kbot", "shield": True, "index_variable": True},
            "kbcon": {"serialname": "fc_kbcon", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "fc_kcnv", "shield": True},
            "qeso": {"serialname": "fc_qeso", "shield": True},
            "pfld": {"serialname": "fc_pfld", "shield": True},
            "delhbar": {"serialname": "fc_delhbar", "shield": True},
            "delqbar": {"serialname": "fc_delqbar", "shield": True},
            "deltbar": {"serialname": "fc_deltbar", "shield": True},
            "delubar": {"serialname": "fc_delubar", "shield": True},
            "delvbar": {"serialname": "fc_delvbar", "shield": True},
            "qcond": {"serialname": "fc_qcond", "shield": True},
            "dellah": {"serialname": "fc_dellah", "shield": True},
            "dellaq": {"serialname": "fc_dellaq", "shield": True},
            "t1": {"serialname": "fc_t1", "shield": True},
            "xmb": {"serialname": "fc_xmb", "shield": True},
            "q1": {"serialname": "fc_q1", "shield": True},
            "u1": {"serialname": "fc_u1", "shield": True},
            "dellau": {"serialname": "fc_dellau", "shield": True},
            "v1": {"serialname": "fc_v1", "shield": True},
            "dellav": {"serialname": "fc_dellav", "shield": True},
            "rntot": {"serialname": "fc_rntot", "shield": True},
            "delqev": {"serialname": "fc_delqev", "shield": True},
            "delq2": {"serialname": "fc_delq2", "shield": True},
            "pwo": {"serialname": "fc_pwo", "shield": True},
            "deltv": {"serialname": "fc_deltv", "shield": True},
            "delq": {"serialname": "fc_delq", "shield": True},
            "qevap": {"serialname": "fc_qevap", "shield": True},
            "rn": {"serialname": "fc_rn", "shield": True},
            "edt": {"serialname": "fc_edt", "shield": True},
            "cnvw": {"serialname": "fc_cnvw", "shield": True},
            "cnvwt": {"serialname": "fc_cnvwt", "shield": True},
            "cnvc": {"serialname": "fc_cnvc", "shield": True},
            "eta": {"serialname": "fc_eta", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        self.compute_func = FeedbackCtrl(
            self.stencil_factory,
            self.quantity_factory,
            inputs.pop("fc_dt2"),
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateSC13(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "cnvflg": {"serialname": "s13_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "s13_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "qeso": {"serialname": "s13_qeso", "shield": True},
            "to": {"serialname": "s13_to", "shield": True},
            "dbyo": {"serialname": "s13_dbyo", "shield": True},
            "qcko": {"serialname": "s13_qcko", "shield": True},
            "qlko_ktcon": {"serialname": "s13_qlko_ktcon", "shield": True},
        }
        self.in_vars["parameters"] = [
            "s13_ncloud",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "s13_cnvflg", "shield": True},
            "ktcon": {
                "serialname": "s13_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "qeso": {"serialname": "s13_qeso", "shield": True},
            "to": {"serialname": "s13_to", "shield": True},
            "dbyo": {"serialname": "s13_dbyo", "shield": True},
            "qcko": {"serialname": "s13_qcko", "shield": True},
            "qlko_ktcon": {"serialname": "s13_qlko_ktcon", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        self.compute_func = SC13(
            self.stencil_factory,
            self.quantity_factory,
            int(inputs.pop("s13_ncloud")),
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)


class TranslateCompTendencies(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "cnvflg": {"serialname": "sct_cnvflg", "shield": True},
            "kbcon1": {
                "serialname": "sct_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "kbcon": {
                "serialname": "sct_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "ktcon1": {
                "serialname": "sct_ktcon1",
                "shield": True,
                "index_variable": True,
            },
            "ktcon": {
                "serialname": "sct_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "sct_kb", "shield": True, "index_variable": True},
            "kmax": {"serialname": "sct_kmax", "shield": True, "index_variable": True},
            "dellah": {"serialname": "sct_dellah", "shield": True},
            "dellaq": {"serialname": "sct_dellaq", "shield": True},
            "dellau": {"serialname": "sct_dellau", "shield": True},
            "dellav": {"serialname": "sct_dellav", "shield": True},
            "del0": {"serialname": "sct_del", "shield": True},
            "zi": {"serialname": "sct_zi", "shield": True},
            "heo": {"serialname": "sct_heo", "shield": True},
            "qo": {"serialname": "sct_qo", "shield": True},
            "xlamue": {"serialname": "sct_xlamue", "shield": True},
            "xlamud": {"serialname": "sct_xlamud", "shield": True},
            "eta": {"serialname": "sct_eta", "shield": True},
            "hcko": {"serialname": "sct_hcko", "shield": True},
            "qrcko": {"serialname": "sct_qrcko", "shield": True},
            "uo": {"serialname": "sct_uo", "shield": True},
            "ucko": {"serialname": "sct_ucko", "shield": True},
            "vo": {"serialname": "sct_vo", "shield": True},
            "vcko": {"serialname": "sct_vcko", "shield": True},
            "qcko": {"serialname": "sct_qcko", "shield": True},
            "dellal": {"serialname": "sct_dellal", "shield": True},
            "qlko_ktcon": {"serialname": "sct_qlko_ktcon", "shield": True},
            "wc": {"serialname": "sct_wc", "shield": True},
            "gdx": {"serialname": "sct_gdx", "shield": True},
            "dtconv": {"serialname": "sct_dtconv", "shield": True},
            "u1": {"serialname": "sct_u1", "shield": True},
            "v1": {"serialname": "sct_v1", "shield": True},
            "po": {"serialname": "sct_po", "shield": True},
            "to": {"serialname": "sct_to", "shield": True},
            "tauadv": {"serialname": "sct_tauadv", "shield": True},
            "xmb": {"serialname": "sct_xmb", "shield": True},
            "sigmagfm": {"serialname": "sct_sigmagfm", "shield": True},
            "garea": {"serialname": "sct_garea", "shield": True},
            "scaldfunc": {"serialname": "sct_scaldfunc", "shield": True},
            "xmbmax": {"serialname": "sct_xmbmax", "shield": True},
            "sumx": {"serialname": "sct_sumx", "shield": True},
            "umean": {"serialname": "sct_umean", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sct_dt2",
        ]
        self.out_vars = {
            "cnvflg": {"serialname": "sct_cnvflg", "shield": True},
            "kbcon1": {
                "serialname": "sct_kbcon1",
                "shield": True,
                "index_variable": True,
            },
            "kbcon": {
                "serialname": "sct_kbcon",
                "shield": True,
                "index_variable": True,
            },
            "ktcon1": {
                "serialname": "sct_ktcon1",
                "shield": True,
                "index_variable": True,
            },
            "ktcon": {
                "serialname": "sct_ktcon",
                "shield": True,
                "index_variable": True,
            },
            "kb": {"serialname": "sct_kb", "shield": True, "index_variable": True},
            "kmax": {"serialname": "sct_kmax", "shield": True, "index_variable": True},
            "dellah": {"serialname": "sct_dellah", "shield": True},
            "dellaq": {"serialname": "sct_dellaq", "shield": True},
            "dellau": {"serialname": "sct_dellau", "shield": True},
            "dellav": {"serialname": "sct_dellav", "shield": True},
            "del0": {"serialname": "sct_del", "shield": True},
            "zi": {"serialname": "sct_zi", "shield": True},
            "heo": {"serialname": "sct_heo", "shield": True},
            "qo": {"serialname": "sct_qo", "shield": True},
            "xlamue": {"serialname": "sct_xlamue", "shield": True},
            "xlamud": {"serialname": "sct_xlamud", "shield": True},
            "eta": {"serialname": "sct_eta", "shield": True},
            "hcko": {"serialname": "sct_hcko", "shield": True},
            "qrcko": {"serialname": "sct_qrcko", "shield": True},
            "uo": {"serialname": "sct_uo", "shield": True},
            "ucko": {"serialname": "sct_ucko", "shield": True},
            "vo": {"serialname": "sct_vo", "shield": True},
            "vcko": {"serialname": "sct_vcko", "shield": True},
            "qcko": {"serialname": "sct_qcko", "shield": True},
            "dellal": {"serialname": "sct_dellal", "shield": True},
            "qlko_ktcon": {"serialname": "sct_qlko_ktcon", "shield": True},
            "wc": {"serialname": "sct_wc", "shield": True},
            "gdx": {"serialname": "sct_gdx", "shield": True},
            "dtconv": {"serialname": "sct_dtconv", "shield": True},
            "u1": {"serialname": "sct_u1", "shield": True},
            "v1": {"serialname": "sct_v1", "shield": True},
            "po": {"serialname": "sct_po", "shield": True},
            "to": {"serialname": "sct_to", "shield": True},
            "tauadv": {"serialname": "sct_tauadv", "shield": True},
            "xmb": {"serialname": "sct_xmb", "shield": True},
            "sigmagfm": {"serialname": "sct_sigmagfm", "shield": True},
            "garea": {"serialname": "sct_garea", "shield": True},
            "scaldfunc": {"serialname": "sct_scaldfunc", "shield": True},
            "xmbmax": {"serialname": "sct_xmbmax", "shield": True},
            "sumx": {"serialname": "sct_sumx", "shield": True},
            "umean": {"serialname": "sct_umean", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing
        self.ignore_near_zero_errors = {"sct_tauadv": True}
        self.near_zero = 1e-30

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        self.compute_func = CompTendencies(
            self.stencil_factory,
            self.quantity_factory,
            inputs.pop("sct_dt2"),
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)
