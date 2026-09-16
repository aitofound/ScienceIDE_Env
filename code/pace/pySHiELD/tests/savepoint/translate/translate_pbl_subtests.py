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
    IntFieldIJ,
)
from ndsl.stencils import copy
from pyshield._config import TRACER_DIM, FloatFieldTracer
from pyshield.stencils.pbl import PBLConfig
from pyshield.stencils.pbl import constants as pbl_constants
from pyshield.stencils.pbl.mfpblt import PBLMassFlux
from pyshield.stencils.pbl.mfscu import StratocumulusMassFlux
from pyshield.stencils.pbl.satmedmfvdiff import (
    compute_asymptotic_mixing_length,
    compute_eddy_diffusivity_buoy_shear,
    compute_prandtl_num_exchange_coeff,
    enhance_pbl_height_thermal,
    heat_moist_tridiag_mat_ele_comp,
    init_turbulence,
    moment_tridiag_mat_ele_comp,
    mrf_pbl_2_thermal_excess,
    mrf_pbl_scheme_part1,
    predict_tke,
    recover_heat_tendency_add_diss_heat,
    recover_moisture_tendency,
    recover_momentum_tendency_and_finish,
    recover_tke_tendency,
    setup_multi_tracer_tridiag,
    stratocumulus,
    thermal_pbl_calc,
    tke_tridiag_matrix_ele_comp,
    tke_up_down_prop,
    tridi2,
    tridin,
    tridit,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


def set_thlvx_0(
    thlvx: FloatField,
    thlvx_0: FloatFieldIJ,
):

    with computation(FORWARD):
        with interval(0, 1):
            thlvx_0 = thlvx[0, 0, 0]


def set_pbot_ptop(
    phii: FloatField,
    pbot: FloatFieldIJ,
    ptop: FloatFieldIJ,
):
    with computation(FORWARD):
        with interval(0, 1):
            pbot = phii
        with interval(-1, None):
            ptop = phii


class InitTurb:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._dt_atmos = config.dt_atmos
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntke = config.ntke

        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._ptop = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="Pa",
            dtype=Float,
        )
        self._pbot = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="Pa",
            dtype=Float,
        )
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        self._tem1 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._init_turbulence = stencil_factory.from_origin_domain(
            func=init_turbulence,
            externals={
                "cap_k0_land": config.cap_k0_land,
                "do_dk_hb19": config.do_dk_hb19,
                "dt2": self._dt_atmos,
                "km1": km1,
                "ntcw": self._ntcw,
                "ntiw": self._ntiw,
                "ntke": self._ntke,
                "xkzm_hi": config.xkzm_hi,
                "xkzm_hl": config.xkzm_hl,
                "xkzm_ho": config.xkzm_ho,
                "xkzm_lim": config.xkzm_lim,
                "xkzm_mi": config.xkzm_mi,
                "xkzm_ml": config.xkzm_ml,
                "xkzm_mo": config.xkzm_mo,
                "xkzm_s": config.xkzm_s,
                "xkzminv": config.xkzminv,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, 1)),
        )

    def __call__(
        self,
        zi: FloatField,
        zl: FloatField,
        zm: FloatField,
        phii: FloatField,
        phil: FloatField,
        chz: FloatField,
        ckz: FloatField,
        area: FloatFieldIJ,
        gdx: FloatFieldIJ,
        tke: FloatField,
        q1: FloatFieldTracer,
        rdzt: FloatField,
        prn: FloatField,
        kx1: IntFieldIJ,
        prsi: FloatField,
        kinver: IntFieldIJ,
        tx1: FloatFieldIJ,
        tx2: FloatFieldIJ,
        xkzo: FloatField,
        xkzmo: FloatField,
        kpblx: IntFieldIJ,
        hpblx: FloatFieldIJ,
        pblflg: BoolFieldIJ,
        sfcflg: BoolFieldIJ,
        pcnvflg: BoolFieldIJ,
        scuflg: BoolFieldIJ,
        zorl: FloatFieldIJ,
        dusfc: FloatFieldIJ,
        dvsfc: FloatFieldIJ,
        dtsfc: FloatFieldIJ,
        dqsfc: FloatFieldIJ,
        kpbl: IntFieldIJ,
        hpbl: FloatFieldIJ,
        rbsoil: FloatFieldIJ,
        radmin: FloatFieldIJ,
        mrad: IntFieldIJ,
        krad: IntFieldIJ,
        lcld: IntFieldIJ,
        kcld: IntFieldIJ,
        theta: FloatField,
        prslk: FloatField,
        psk: FloatFieldIJ,
        t1: FloatField,
        pix: FloatField,
        qlx: FloatField,
        slx: FloatField,
        thvx: FloatField,
        qtx: FloatField,
        thlx: FloatField,
        thlvx: FloatField,
        svx: FloatField,
        thetae: FloatField,
        gotvx: FloatField,
        prsl: FloatField,
        plyr: FloatField,
        rhly: FloatField,
        qstl: FloatField,
        bf: FloatField,
        cfly: FloatField,
        crb: FloatFieldIJ,
        dtdz1: FloatFieldIJ,
        evap: FloatFieldIJ,
        heat: FloatFieldIJ,
        hlw: FloatField,
        radx: FloatField,
        sflux: FloatFieldIJ,
        shr2: FloatField,
        stress: FloatFieldIJ,
        hsw: FloatField,
        thermal: FloatFieldIJ,
        tsea: FloatFieldIJ,
        ustar: FloatFieldIJ,
        u1: FloatField,
        v1: FloatField,
        u10m: FloatFieldIJ,
        v10m: FloatFieldIJ,
        xmu: FloatFieldIJ,
        islimsk: FloatFieldIJ,
        xkzm_hx: FloatFieldIJ,
        xkzm_mx: FloatFieldIJ,
        tvx: FloatField,
        tem1: FloatField,
    ):
        self._init_turbulence(
            zi,
            zl,
            zm,
            phii,
            phil,
            chz,
            ckz,
            area,
            gdx,
            tke,
            q1,
            rdzt,
            prn,
            kx1,
            prsi,
            self._k_mask,
            kinver,
            tx1,
            tx2,
            xkzo,
            xkzmo,
            kpblx,
            hpblx,
            pblflg,
            sfcflg,
            pcnvflg,
            scuflg,
            zorl,
            dusfc,
            dvsfc,
            dtsfc,
            dqsfc,
            kpbl,
            hpbl,
            rbsoil,
            radmin,
            mrad,
            krad,
            lcld,
            kcld,
            theta,
            prslk,
            psk,
            t1,
            pix,
            qlx,
            slx,
            thvx,
            qtx,
            thlx,
            thlvx,
            svx,
            thetae,
            gotvx,
            prsl,
            plyr,
            rhly,
            qstl,
            bf,
            cfly,
            crb,
            dtdz1,
            evap,
            heat,
            hlw,
            radx,
            sflux,
            shr2,
            stress,
            hsw,
            thermal,
            tsea,
            ustar,
            u1,
            v1,
            u10m,
            v10m,
            xmu,
            islimsk,
            self._ptop,
            self._pbot,
            xkzm_hx,
            xkzm_mx,
            tvx,
            tem1,
        )


class MRFScheme:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._mrf_pbl_scheme_part1 = stencil_factory.from_origin_domain(
            func=mrf_pbl_scheme_part1,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._mrf_pbl_2_thermal_excess = stencil_factory.from_origin_domain(
            func=mrf_pbl_2_thermal_excess,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._thlvx_0 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )

    def __call__(
        self,
        crb,
        flg,
        kpblx,
        rbdn,
        rbup,
        rbsoil,
        thermal,
        thlvx,
        u1,
        v1,
        zl,
        evap,
        fh,
        fm,
        gotvx,
        zol,
        heat,
        hpbl,
        hpblx,
        kpbl,
        pblflg,
        pcnvflg,
        phih,
        phim,
        sfcflg,
        sflux,
        theta,
        ustar,
        vpert,
        zi,
    ):
        self._mrf_pbl_scheme_part1(
            crb,
            flg,
            kpblx,
            self._k_mask,
            rbdn,
            rbup,
            rbsoil,
            thermal,
            thlvx,
            self._thlvx_0,
            u1,
            v1,
            zl,
        )
        self._mrf_pbl_2_thermal_excess(
            crb,
            evap,
            fh,
            flg,
            fm,
            gotvx,
            heat,
            hpbl,
            hpblx,
            kpbl,
            kpblx,
            self._k_mask,
            pblflg,
            pcnvflg,
            phih,
            phim,
            rbdn,
            rbup,
            rbsoil,
            sfcflg,
            sflux,
            thermal,
            theta,
            ustar,
            vpert,
            zi,
            zl,
            zol,
        )


class ThermalPBL:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._thlvx_0 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )

        self._thermal_pbl_calc = stencil_factory.from_origin_domain(
            func=thermal_pbl_calc,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._set_thlvx_0 = stencil_factory.from_origin_domain(
            func=set_thlvx_0,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._enhance_pbl_height_thermal = stencil_factory.from_origin_domain(
            func=enhance_pbl_height_thermal,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        crb,
        flg,
        kpbl,
        rbdn,
        rbup,
        thermal,
        thlvx,
        u1,
        v1,
        zl,
        hpbl,
        pblflg,
        pcnvflg,
        zi,
    ):
        self._set_thlvx_0(
            thlvx,
            self._thlvx_0,
        )

        self._thermal_pbl_calc(
            crb,
            flg,
            kpbl,
            self._k_mask,
            rbdn,
            rbup,
            thermal,
            thlvx,
            self._thlvx_0,
            u1,
            v1,
            zl,
        )

        self._enhance_pbl_height_thermal(
            crb,
            hpbl,
            kpbl,
            self._k_mask,
            pblflg,
            pcnvflg,
            rbdn,
            rbup,
            zi,
            zl,
        )


class Stratocumulus:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._stratocumulus = stencil_factory.from_origin_domain(
            func=stratocumulus,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmscu),
        )

    def __call__(
        self,
        flg,
        kcld,
        krad,
        lcld,
        radmin,
        radx,
        qlx,
        scuflg,
        zl,
    ):
        self._stratocumulus(
            flg,
            kcld,
            krad,
            lcld,
            self._k_mask,
            radmin,
            radx,
            qlx,
            scuflg,
            zl,
        )


class PBLAML:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1

        self._lev = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Int,
        )

        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        self._ptem = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._mlenflg = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Bool,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._compute_asymptotic_mixing_length = stencil_factory.from_origin_domain(
            func=compute_asymptotic_mixing_length,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        zldn,
        zlup,
        thvx,
        tke,
        gotvx,
        zl,
        tsea,
        q1,
        zi,
        rlam,
        ele,
        elm,
        zol,
        gdx,
        phii,
    ):
        self._compute_asymptotic_mixing_length(
            zldn,
            zlup,
            thvx,
            tke,
            gotvx,
            zl,
            tsea,
            q1,
            zi,
            rlam,
            ele,
            elm,
            self._ptem,
            zol,
            gdx,
            self._lev,
            self._k_mask,
            self._mlenflg,
        )


class TKETridiag:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._dt_atmos = config.dt_atmos
        self._ntke = config.ntracers - 1
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._cu = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._rt = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._f1_p1 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )
        self._ad_p1 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )

        self._tke_tridiag_matrix_ele_comp = stencil_factory.from_origin_domain(
            func=tke_tridiag_matrix_ele_comp,
            externals={
                "dt2": self._dt_atmos,
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ad,
        al,
        au,
        delta,
        dkq,
        f1,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        qcdo,
        qcko,
        rdzt,
        scuflg,
        tke,
        xmf,
        xmfd,
    ):
        self._tke_tridiag_matrix_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkq,
            f1,
            self._f1_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tke,
            xmf,
            xmfd,
            self._cu,
            self._rt,
        )


class Prandtl:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        self._kmpbl = idx.domain[2] // 2 + 1
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._compute_prandtl_num_exchange_coeff = stencil_factory.from_origin_domain(
            func=compute_prandtl_num_exchange_coeff,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

    def __call__(
        self,
        chz,
        ckz,
        hpbl,
        kpbl,
        pcnvflg,
        phih,
        phim,
        prn,
        zi,
    ):
        self._compute_prandtl_num_exchange_coeff(
            chz,
            ckz,
            hpbl,
            kpbl,
            self._k_mask,
            pcnvflg,
            phih,
            phim,
            prn,
            zi,
        )


class TKEPredict:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: PBLConfig,
    ):
        idx = stencil_factory.grid_indexing
        self._dt_atmos = config.dt_atmos
        self._kk = max(round(self._dt_atmos / pbl_constants.CDTN), 1)
        self._dtn = self._dt_atmos / float(self._kk)

        self._predict_tke = stencil_factory.from_origin_domain(
            func=predict_tke,
            externals={
                "dtn": self._dtn,
                "kk": self._kk,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, -1)),
        )

    def __call__(
        self,
        diss,
        prod,
        rle,
        tke,
        ele,
    ):
        self._predict_tke(
            diss,
            prod,
            rle,
            tke,
            ele,
        )


class EdDiffShear:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ):
        idx = stencil_factory.grid_indexing
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        self._dkt_out = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._compute_eddy_diffusivity_buoy_shear = stencil_factory.from_origin_domain(
            func=compute_eddy_diffusivity_buoy_shear,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        bf,
        buod,
        buou,
        chz,
        ckz,
        dku,
        dkt,
        dkq,
        elm,
        gotvx,
        kpbl,
        mrad,
        krad,
        pblflg,
        pcnvflg,
        phim,
        prn,
        prod,
        radj,
        rdzt,
        scuflg,
        sflux,
        shr2,
        stress,
        tke,
        u1,
        ucdo,
        ucko,
        ustar,
        v1,
        vcdo,
        vcko,
        xkzo,
        xkzmo,
        xmf,
        xmfd,
        zl,
    ):
        self._compute_eddy_diffusivity_buoy_shear(
            bf,
            buod,
            buou,
            chz,
            ckz,
            dku,
            dkt,
            dkq,
            elm,
            gotvx,
            kpbl,
            self._k_mask,
            mrad,
            krad,
            pblflg,
            pcnvflg,
            phim,
            prn,
            prod,
            radj,
            rdzt,
            scuflg,
            sflux,
            shr2,
            stress,
            tke,
            u1,
            ucdo,
            ucko,
            ustar,
            v1,
            vcdo,
            vcko,
            xkzo,
            xkzmo,
            xmf,
            xmfd,
            zl,
            self._dkt_out,
        )


class UpDownTKE:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
    ):
        idx = stencil_factory.grid_indexing
        self._ntke = config.ntracers - 1

        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._tke_up_down_prop = stencil_factory.from_origin_domain(
            func=tke_up_down_prop,
            externals={
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        pcnvflg,
        qcdo,
        qcko,
        scuflg,
        tke,
        kpbl,
        xlamue,
        zl,
        krad,
        mrad,
        xlamde,
    ):
        self._tke_up_down_prop(
            pcnvflg,
            qcdo,
            qcko,
            scuflg,
            tke,
            kpbl,
            self._k_mask,
            xlamue,
            zl,
            krad,
            mrad,
            xlamde,
        )


class MomentTridiagComp:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
    ):
        idx = stencil_factory.grid_indexing
        self._dt_atmos = config.dt_atmos
        self._dspheat = config.dspheat
        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: config.ntracers,
            }
        )

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._k_mask = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._moment_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=moment_tridiag_mat_ele_comp,
            externals={
                "dspheat": self._dspheat,
                "dt2": self._dt_atmos,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ad,
        al,
        au,
        delta,
        diss,
        dku,
        dtdz1,
        f1,
        f2,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        rdzt,
        scuflg,
        spd1,
        stress,
        tdt,
        u1,
        ucdo,
        ucko,
        v1,
        vcdo,
        vcko,
        xmf,
        xmfd,
    ):
        self._moment_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            diss,
            dku,
            dtdz1,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            rdzt,
            scuflg,
            spd1,
            stress,
            tdt,
            u1,
            ucdo,
            ucko,
            v1,
            vcdo,
            vcko,
            xmf,
            xmfd,
            self._cu,
            self._rt,
            self._a2,
        )


class HeatTracerTridiag:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
    ):
        idx = stencil_factory.grid_indexing
        self._dt_atmos = config.dt_atmos
        self._dspheat = config.dspheat
        self._ntrac1 = config.ntracers - 1
        self._ntracers = config.ntracers
        self._ntke = config.ntracers - 1
        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: config.ntracers,
            }
        )

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._k_mask = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._heat_moist_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=heat_moist_tridiag_mat_ele_comp,
            externals={"dt2": self._dt_atmos},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        if self._ntrac1 >= 2:
            self._setup_multi_tracer_tridiag = stencil_factory.from_origin_domain(
                func=setup_multi_tracer_tridiag,
                externals={"dt2": self._dt_atmos},
                origin=idx.origin_compute(),
                domain=idx.domain_compute(),
            )

    def __call__(
        self,
        ad,
        al,
        au,
        delta,
        dkt,
        f1,
        f2,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        q1,
        qcdo,
        qcko,
        rdzt,
        scuflg,
        tcdo,
        tcko,
        t1,
        xmf,
        xmfd,
        dtdz1,
        evap,
        heat,
    ):

        self._heat_moist_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkt,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            q1,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tcdo,
            tcko,
            t1,
            xmf,
            xmfd,
            dtdz1,
            evap,
            heat,
            self._cu,
            self._rt,
            self._a2,
        )

        for n in range(self._ntracers):
            dim_n = n  # if n < self._ntke else n + 1
            if dim_n != self._ntke:
                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._setup_multi_tracer_tridiag(
                            pcnvflg,
                            self._k_mask,
                            kpbl,
                            delta,
                            prsl,
                            rdzt,
                            xmf,
                            qcko,
                            q1,
                            f2,
                            self._f2_p1,
                            scuflg,
                            mrad,
                            krad,
                            xmfd,
                            qcdo,
                            self._a2,
                            dim_n,
                        )


class TKETendencyCalc:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config,
    ):
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1
        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        self._ntke = config.ntracers - 1
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._cu = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._rt = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._f1_p1 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )
        self._ad_p1 = quantity_factory.zeros(
            [I_DIM, J_DIM],
            units="unknown",
            dtype=Float,
        )

        self._tridit = stencil_factory.from_origin_domain(
            func=tridit,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._copy_stencil = stencil_factory.from_origin_domain(
            func=copy,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tke_tridiag_matrix_ele_comp = stencil_factory.from_origin_domain(
            func=tke_tridiag_matrix_ele_comp,
            externals={
                "dt2": self._dt_atmos,
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_tke_tendency = stencil_factory.from_origin_domain(
            func=recover_tke_tendency,
            externals={"rdt": self._rdt, "ntke": self._ntke},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ad,
        au,
        al,
        delta,
        dkq,
        f1,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        qcdo,
        qcko,
        rdzt,
        scuflg,
        rtg,
        q1,
        tke,
        xmf,
        xmfd,
    ):
        self._tke_tridiag_matrix_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkq,
            f1,
            self._f1_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tke,
            xmf,
            xmfd,
            self._cu,
            self._rt,
        )

        self._tridit(
            self._cu,
            ad,
            al,
            self._rt,
            au,
            f1,
        )

        self._recover_tke_tendency(
            rtg,
            f1,
            q1,
        )


class HeatTracerTendencyCalc:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
    ):
        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        idx = stencil_factory.grid_indexing
        self._dt_atmos = config.dt_atmos
        self._dspheat = config.dspheat
        self._ntrac1 = config.ntracers - 1
        self._ntracers = config.ntracers
        self._ntke = config.ntracers - 1
        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: config.ntracers,
            }
        )

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._k_mask = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._heat_moist_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=heat_moist_tridiag_mat_ele_comp,
            externals={"dt2": self._dt_atmos},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        if self._ntrac1 >= 2:
            self._setup_multi_tracer_tridiag = stencil_factory.from_origin_domain(
                func=setup_multi_tracer_tridiag,
                externals={"dt2": self._dt_atmos},
                origin=idx.origin_compute(),
                domain=idx.domain_compute(),
            )

        self._tridin = stencil_factory.from_origin_domain(
            func=tridin,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_moisture_tendency = stencil_factory.from_origin_domain(
            func=recover_moisture_tendency,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_heat_tendency_add_diss_heat = stencil_factory.from_origin_domain(
            func=recover_heat_tendency_add_diss_heat,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ad,
        al,
        au,
        delta,
        dkt,
        f1,
        f2,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        qcdo,
        qcko,
        rdzt,
        scuflg,
        evap,
        tcdo,
        tcko,
        xmf,
        xmfd,
        t1,
        q1,
        dtdz1,
        heat,
        rtg,
        tdt,
        dtsfc,
        dqsfc,
    ):
        self._heat_moist_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkt,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            q1,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tcdo,
            tcko,
            t1,
            xmf,
            xmfd,
            dtdz1,
            evap,
            heat,
            self._cu,
            self._rt,
            self._a2,
        )

        for n in range(self._ntracers):
            dim_n = n  # if n < self._ntke else n + 1
            if dim_n != self._ntke:
                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._setup_multi_tracer_tridiag(
                            pcnvflg,
                            self._k_mask,
                            kpbl,
                            delta,
                            prsl,
                            rdzt,
                            xmf,
                            qcko,
                            q1,
                            f2,
                            self._f2_p1,
                            scuflg,
                            mrad,
                            krad,
                            xmfd,
                            qcdo,
                            self._a2,
                            dim_n,
                        )

                self._tridin(
                    al,
                    ad,
                    self._cu,
                    self._rt,
                    self._a2,
                    au,
                    f1,
                    f2,
                    dim_n,
                )

                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._recover_moisture_tendency(
                            f2,
                            q1,
                            rtg,
                            dim_n,
                        )

        self._recover_heat_tendency_add_diss_heat(
            tdt,
            f1,
            t1,
            f2,
            q1,
            rtg,
            dtsfc,
            delta,
            dqsfc,
        )


class MomentTendencyCalc:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
    ):
        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        idx = stencil_factory.grid_indexing
        self._dspheat = config.dspheat
        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: config.ntracers,
            }
        )

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._k_mask = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._moment_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=moment_tridiag_mat_ele_comp,
            externals={
                "dspheat": self._dspheat,
                "dt2": self._dt_atmos,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )
        self._tridi2 = stencil_factory.from_origin_domain(
            func=tridi2,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._cu = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._r1 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Float,
        )
        self._r2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._recover_momentum_tendency_and_finish = stencil_factory.from_origin_domain(
            func=recover_momentum_tendency_and_finish,
            externals={"rdt": self._rdt},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        delta,
        diss,
        dku,
        dtdz1,
        vcko,
        xmf,
        xmfd,
        du,
        dv,
        dusfc,
        dvsfc,
        f1,
        f2,
        al,
        ad,
        au,
        kpbl,
        krad,
        mrad,
        pcnvflg,
        prsl,
        rdzt,
        scuflg,
        spd1,
        stress,
        tdt,
        u1,
        ucdo,
        ucko,
        v1,
        vcdo,
        hpbl,
        hpblx,
        kpblx,
    ):
        self._moment_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            diss,
            dku,
            dtdz1,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            rdzt,
            scuflg,
            spd1,
            stress,
            tdt,
            u1,
            ucdo,
            ucko,
            v1,
            vcdo,
            vcko,
            xmf,
            xmfd,
            self._cu,
            self._rt,
            self._a2,
        )

        self._tridi2(
            f1,
            f2,
            au,
            al,
            ad,
            self._cu,
            self._rt,
            self._a2,
        )

        self._recover_momentum_tendency_and_finish(
            delta,
            du,
            dusfc,
            dv,
            dvsfc,
            f1,
            f2,
            hpbl,
            hpblx,
            kpbl,
            kpblx,
            self._k_mask,
            u1,
            v1,
        )


class Half2:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
        kmscu: int,
        kmpbl: int,
    ):
        self._ntracers = config.ntracers
        assert (
            self._ntracers == 9
        ), f"PBL scheme satmedmfvdif requires ntracer ({config.ntracers}) == 9"

        self._ntrac1 = self._ntracers - 1

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                TRACER_DIM: self._ntracers,
            }
        )
        idx = stencil_factory.grid_indexing

        km1 = idx.domain[2] - 1
        self._kmpbl = kmpbl
        self._kmscu = kmscu

        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        self._kk = max(round(self._dt_atmos / pbl_constants.CDTN), 1)
        self._dtn = self._dt_atmos / float(self._kk)

        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntke = config.ntke

        self._dspheat = config.dspheat
        self.TRACER_DIM = TRACER_DIM

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._tem1 = make_quantity()
        self._lev = make_quantity_2D(Int)
        self._mlenflg = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Bool,
        )

        self._mfpblt = PBLMassFlux(
            stencil_factory,
            quantity_factory,
            dt2=self._dt_atmos,
            ntcw=self._ntcw,
            ntrac1=self._ntrac1,
            kmpbl=self._kmpbl,
        )

        self._mfscu = StratocumulusMassFlux(
            stencil_factory,
            quantity_factory,
            self._dt_atmos,
            self._ntracers,
            self._ntcw,
            self._ntrac1,
            self._kmscu,
            self._ntke,
        )

        self._compute_prandtl_num_exchange_coeff = stencil_factory.from_origin_domain(
            func=compute_prandtl_num_exchange_coeff,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._compute_asymptotic_mixing_length = stencil_factory.from_origin_domain(
            func=compute_asymptotic_mixing_length,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._compute_eddy_diffusivity_buoy_shear = stencil_factory.from_origin_domain(
            func=compute_eddy_diffusivity_buoy_shear,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._predict_tke = stencil_factory.from_origin_domain(
            func=predict_tke,
            externals={
                "dtn": self._dtn,
                "kk": self._kk,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, -1)),
        )

        self._tke_up_down_prop = stencil_factory.from_origin_domain(
            func=tke_up_down_prop,
            externals={
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tke_tridiag_matrix_ele_comp = stencil_factory.from_origin_domain(
            func=tke_tridiag_matrix_ele_comp,
            externals={
                "dt2": self._dt_atmos,
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridit = stencil_factory.from_origin_domain(
            func=tridit,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_tke_tendency = stencil_factory.from_origin_domain(
            func=recover_tke_tendency,
            externals={"rdt": self._rdt, "ntke": self._ntke},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._heat_moist_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=heat_moist_tridiag_mat_ele_comp,
            externals={"dt2": self._dt_atmos},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        if self._ntrac1 >= 2:
            self._setup_multi_tracer_tridiag = stencil_factory.from_origin_domain(
                func=setup_multi_tracer_tridiag,
                externals={"dt2": self._dt_atmos},
                origin=idx.origin_compute(),
                domain=idx.domain_compute(),
            )

        self._tridin = stencil_factory.from_origin_domain(
            func=tridin,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_moisture_tendency = stencil_factory.from_origin_domain(
            func=recover_moisture_tendency,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_heat_tendency_add_diss_heat = stencil_factory.from_origin_domain(
            func=recover_heat_tendency_add_diss_heat,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._moment_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=moment_tridiag_mat_ele_comp,
            externals={
                "dspheat": self._dspheat,
                "dt2": self._dt_atmos,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridi2 = stencil_factory.from_origin_domain(
            func=tridi2,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_momentum_tendency_and_finish = stencil_factory.from_origin_domain(
            func=recover_momentum_tendency_and_finish,
            externals={"rdt": self._rdt},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ckz,
        chz,
        hpbl,
        kpbl,
        zi,
        phih,
        zldn,
        zlup,
        thvx,
        tke,
        gotvx,
        zl,
        tsea,
        q1,
        rlam,
        ele,
        elm,
        zol,
        gdx,
        phii,
        phim,
        prn,
        bf,
        buod,
        buou,
        dku,
        dkt,
        dkq,
        mrad,
        krad,
        pblflg,
        prod,
        radj,
        rdzt,
        sflux,
        shr2,
        stress,
        u1,
        ucdo,
        ucko,
        ustar,
        v1,
        vcdo,
        vcko,
        xkzo,
        xkzmo,
        xmf,
        xmfd,
        rle,
        diss,
        prsl,
        rtg,
        qcdo,
        qcko,
        f2,
        spd1,
        xlamue,
        xlamde,
        evap,
        ad,
        al,
        au,
        delta,
        f1,
        hpblx,
        kpblx,
        tcdo,
        tcko,
        t1,
        dtdz1,
        heat,
        dtsfc,
        dqsfc,
        tdt,
        du,
        dv,
        dusfc,
        dvsfc,
        zm,
        plyr,
        pix,
        thlx,
        vpert,
        thlvx,
        thetae,
        radmin,
        pcnvflg,
        scuflg,
    ):
        self._mfpblt(
            pcnvflg,
            zl,
            zm,
            q1,  # I, J, K, ntracer field
            u1,
            v1,
            plyr,
            pix,
            thlx,
            thvx,
            gdx,
            hpbl,
            kpbl,
            vpert,
            buou,
            xmf,
            tcko,
            qcko,  # I, J, K, ntracer field
            ucko,
            vcko,
            xlamue,
            self._k_mask,
        )

        self._mfscu(
            scuflg,
            zl,
            zm,
            q1,  # I, J, K, ntracer field
            u1,
            v1,
            plyr,
            pix,
            thlx,
            thvx,
            thlvx,
            gdx,
            thetae,
            radj,
            krad,
            mrad,
            radmin,
            buod,
            xmfd,
            tcdo,
            qcdo,  # I, J, K, ntracer field
            ucdo,
            vcdo,
            xlamde,
            self._k_mask,
        )

        self._compute_prandtl_num_exchange_coeff(
            chz,
            ckz,
            hpbl,
            kpbl,
            self._k_mask,
            pcnvflg,
            phih,
            phim,
            prn,
            zi,
        )

        self._compute_asymptotic_mixing_length(
            zldn,
            zlup,
            thvx,
            tke,
            gotvx,
            zl,
            tsea,
            q1,
            zi,
            rlam,
            ele,
            elm,
            self._tem1,
            zol,
            gdx,
            self._lev,
            self._k_mask,
            self._mlenflg,
        )

        self._compute_eddy_diffusivity_buoy_shear(
            bf,
            buod,
            buou,
            chz,
            ckz,
            dku,
            dkt,
            dkq,
            elm,
            gotvx,
            kpbl,
            self._k_mask,
            mrad,
            krad,
            pblflg,
            pcnvflg,
            phim,
            prn,
            prod,
            radj,
            rdzt,
            scuflg,
            sflux,
            shr2,
            stress,
            tke,
            u1,
            ucdo,
            ucko,
            ustar,
            v1,
            vcdo,
            vcko,
            xkzo,
            xkzmo,
            xmf,
            xmfd,
            zl,
            dkt,
        )

        self._predict_tke(
            diss,
            prod,
            rle,
            tke,
            ele,
        )

        self._tke_up_down_prop(
            pcnvflg,
            qcdo,
            qcko,
            scuflg,
            tke,
            kpbl,
            self._k_mask,
            xlamue,
            zl,
            krad,
            mrad,
            xlamde,
        )

        self._tke_tridiag_matrix_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkq,
            f1,
            self._f1_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tke,
            xmf,
            xmfd,
            self._cu,
            self._rt,
        )

        self._tridit(
            self._cu,
            ad,
            al,
            self._rt,
            au,
            f1,
        )

        self._recover_tke_tendency(
            rtg,
            f1,
            q1,
        )

        self._heat_moist_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkt,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            q1,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tcdo,
            tcko,
            t1,
            xmf,
            xmfd,
            dtdz1,
            evap,
            heat,
            self._cu,
            self._rt,
            self._a2,
        )

        for n in range(self._ntracers):
            dim_n = n  # if n < self._ntke else n + 1
            if dim_n != self._ntke:
                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._setup_multi_tracer_tridiag(
                            pcnvflg,
                            self._k_mask,
                            kpbl,
                            delta,
                            prsl,
                            rdzt,
                            xmf,
                            qcko,
                            q1,
                            f2,
                            self._f2_p1,
                            scuflg,
                            mrad,
                            krad,
                            xmfd,
                            qcdo,
                            self._a2,
                            dim_n,
                        )

                self._tridin(
                    al,
                    ad,
                    self._cu,
                    self._rt,
                    self._a2,
                    au,
                    f1,
                    f2,
                    dim_n,
                )

                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._recover_moisture_tendency(
                            f2,
                            q1,
                            rtg,
                            dim_n,
                        )

        self._recover_heat_tendency_add_diss_heat(
            tdt,
            f1,
            t1,
            f2,
            q1,
            rtg,
            dtsfc,
            delta,
            dqsfc,
        )

        self._moment_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            diss,
            dku,
            dtdz1,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            rdzt,
            scuflg,
            spd1,
            stress,
            tdt,
            u1,
            ucdo,
            ucko,
            v1,
            vcdo,
            vcko,
            xmf,
            xmfd,
            self._cu,
            self._rt,
            self._a2,
        )

        self._tridi2(
            f1,
            f2,
            au,
            al,
            ad,
            self._cu,
            self._rt,
            self._a2,
        )

        self._recover_momentum_tendency_and_finish(
            delta,
            du,
            dusfc,
            dv,
            dvsfc,
            f1,
            f2,
            hpbl,
            hpblx,
            kpbl,
            kpblx,
            self._k_mask,
            u1,
            v1,
        )


class SCUEnd:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: PBLConfig,
        kmscu: int,
        kmpbl: int,
    ):
        self._ntracers = config.ntracers
        self._ntrac1 = self._ntracers - 1

        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                TRACER_DIM: self._ntracers,
            }
        )
        idx = stencil_factory.grid_indexing
        km1 = idx.domain[2] - 1
        self._kmpbl = kmpbl
        self._kmscu = kmscu

        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        self._kk = max(round(self._dt_atmos / pbl_constants.CDTN), 1)
        self._dtn = self._dt_atmos / float(self._kk)

        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntke = config.ntke

        self._dspheat = config.dspheat
        self.TRACER_DIM = TRACER_DIM

        def make_quantity():
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return self.quantity_factory.zeros(
                [I_DIM, J_DIM],
                units="unknown",
                dtype=type,
            )

        self._cu = make_quantity()
        self._rt = make_quantity()
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._a2 = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._tem1 = make_quantity()
        self._lev = make_quantity_2D(Int)
        self._mlenflg = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Bool,
        )

        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        self._mfscu = StratocumulusMassFlux(
            stencil_factory,
            quantity_factory,
            self._dt_atmos,
            self._ntracers,
            self._ntcw,
            self._ntrac1,
            self._kmscu,
            self._ntke,
        )

        self._compute_prandtl_num_exchange_coeff = stencil_factory.from_origin_domain(
            func=compute_prandtl_num_exchange_coeff,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._compute_asymptotic_mixing_length = stencil_factory.from_origin_domain(
            func=compute_asymptotic_mixing_length,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._compute_eddy_diffusivity_buoy_shear = stencil_factory.from_origin_domain(
            func=compute_eddy_diffusivity_buoy_shear,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._predict_tke = stencil_factory.from_origin_domain(
            func=predict_tke,
            externals={
                "dtn": self._dtn,
                "kk": self._kk,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, -1)),
        )

        self._tke_up_down_prop = stencil_factory.from_origin_domain(
            func=tke_up_down_prop,
            externals={
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tke_tridiag_matrix_ele_comp = stencil_factory.from_origin_domain(
            func=tke_tridiag_matrix_ele_comp,
            externals={
                "dt2": self._dt_atmos,
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridit = stencil_factory.from_origin_domain(
            func=tridit,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_tke_tendency = stencil_factory.from_origin_domain(
            func=recover_tke_tendency,
            externals={"rdt": self._rdt, "ntke": self._ntke},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._heat_moist_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=heat_moist_tridiag_mat_ele_comp,
            externals={"dt2": self._dt_atmos},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        if self._ntrac1 >= 2:
            self._setup_multi_tracer_tridiag = stencil_factory.from_origin_domain(
                func=setup_multi_tracer_tridiag,
                externals={"dt2": self._dt_atmos},
                origin=idx.origin_compute(),
                domain=idx.domain_compute(),
            )

        self._tridin = stencil_factory.from_origin_domain(
            func=tridin,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_moisture_tendency = stencil_factory.from_origin_domain(
            func=recover_moisture_tendency,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_heat_tendency_add_diss_heat = stencil_factory.from_origin_domain(
            func=recover_heat_tendency_add_diss_heat,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._moment_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=moment_tridiag_mat_ele_comp,
            externals={
                "dspheat": self._dspheat,
                "dt2": self._dt_atmos,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridi2 = stencil_factory.from_origin_domain(
            func=tridi2,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_momentum_tendency_and_finish = stencil_factory.from_origin_domain(
            func=recover_momentum_tendency_and_finish,
            externals={"rdt": self._rdt},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        ckz,
        chz,
        hpbl,
        kpbl,
        pcnvflg,
        zi,
        phih,
        zldn,
        zlup,
        thvx,
        tke,
        gotvx,
        zl,
        tsea,
        q1,
        rlam,
        ele,
        elm,
        zol,
        gdx,
        phii,
        phim,
        prn,
        bf,
        buod,
        buou,
        dku,
        dkt,
        dkq,
        mrad,
        krad,
        pblflg,
        prod,
        radj,
        rdzt,
        scuflg,
        sflux,
        shr2,
        stress,
        u1,
        ucdo,
        ucko,
        ustar,
        v1,
        vcdo,
        vcko,
        xkzo,
        xkzmo,
        xmf,
        xmfd,
        rle,
        diss,
        prsl,
        rtg,
        qcdo,
        qcko,
        f2,
        spd1,
        xlamue,
        xlamde,
        evap,
        ad,
        al,
        au,
        delta,
        f1,
        hpblx,
        kpblx,
        tcdo,
        tcko,
        t1,
        dtdz1,
        heat,
        dtsfc,
        dqsfc,
        tdt,
        du,
        dv,
        dusfc,
        dvsfc,
        zm,
        plyr,
        pix,
        thlx,
        vpert,
        thlvx,
        thetae,
        radmin,
    ):
        self._mfscu(
            scuflg,
            zl,
            zm,
            q1,  # I, J, K, ntracer field
            u1,
            v1,
            plyr,
            pix,
            thlx,
            thvx,
            thlvx,
            gdx,
            thetae,
            radj,
            krad,
            mrad,
            radmin,
            buod,
            xmfd,
            tcdo,
            qcdo,  # I, J, K, ntracer field
            ucdo,
            vcdo,
            xlamde,
            self._k_mask,
        )

        self._compute_prandtl_num_exchange_coeff(
            chz,
            ckz,
            hpbl,
            kpbl,
            self._k_mask,
            pcnvflg,
            phih,
            phim,
            prn,
            zi,
        )

        self._compute_asymptotic_mixing_length(
            zldn,
            zlup,
            thvx,
            tke,
            gotvx,
            zl,
            tsea,
            q1,
            zi,
            rlam,
            ele,
            elm,
            self._tem1,
            zol,
            gdx,
            self._lev,
            self._k_mask,
            self._mlenflg,
        )

        self._compute_eddy_diffusivity_buoy_shear(
            bf,
            buod,
            buou,
            chz,
            ckz,
            dku,
            dkt,
            dkq,
            elm,
            gotvx,
            kpbl,
            self._k_mask,
            mrad,
            krad,
            pblflg,
            pcnvflg,
            phim,
            prn,
            prod,
            radj,
            rdzt,
            scuflg,
            sflux,
            shr2,
            stress,
            tke,
            u1,
            ucdo,
            ucko,
            ustar,
            v1,
            vcdo,
            vcko,
            xkzo,
            xkzmo,
            xmf,
            xmfd,
            zl,
            dkt,
        )

        self._predict_tke(
            diss,
            prod,
            rle,
            tke,
            ele,
        )

        self._tke_up_down_prop(
            pcnvflg,
            qcdo,
            qcko,
            scuflg,
            tke,
            kpbl,
            self._k_mask,
            xlamue,
            zl,
            krad,
            mrad,
            xlamde,
        )

        self._tke_tridiag_matrix_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkq,
            f1,
            self._f1_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tke,
            xmf,
            xmfd,
            self._cu,
            self._rt,
        )

        self._tridit(
            self._cu,
            ad,
            al,
            self._rt,
            au,
            f1,
        )

        self._recover_tke_tendency(
            rtg,
            f1,
            q1,
        )

        self._heat_moist_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            dkt,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            q1,
            qcdo,
            qcko,
            rdzt,
            scuflg,
            tcdo,
            tcko,
            t1,
            xmf,
            xmfd,
            dtdz1,
            evap,
            heat,
            self._cu,
            self._rt,
            self._a2,
        )

        for n in range(self._ntracers):
            dim_n = n  # if n < self._ntke else n + 1
            if dim_n != self._ntke:
                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._setup_multi_tracer_tridiag(
                            pcnvflg,
                            self._k_mask,
                            kpbl,
                            delta,
                            prsl,
                            rdzt,
                            xmf,
                            qcko,
                            q1,
                            f2,
                            self._f2_p1,
                            scuflg,
                            mrad,
                            krad,
                            xmfd,
                            qcdo,
                            self._a2,
                            dim_n,
                        )

                self._tridin(
                    al,
                    ad,
                    self._cu,
                    self._rt,
                    self._a2,
                    au,
                    f1,
                    f2,
                    dim_n,
                )

                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._recover_moisture_tendency(
                            f2,
                            q1,
                            rtg,
                            dim_n,
                        )

        self._recover_heat_tendency_add_diss_heat(
            tdt,
            f1,
            t1,
            f2,
            q1,
            rtg,
            dtsfc,
            delta,
            dqsfc,
        )

        self._moment_tridiag_mat_ele_comp(
            ad,
            self._ad_p1,
            al,
            au,
            delta,
            diss,
            dku,
            dtdz1,
            f1,
            self._f1_p1,
            f2,
            self._f2_p1,
            kpbl,
            krad,
            self._k_mask,
            mrad,
            pcnvflg,
            prsl,
            rdzt,
            scuflg,
            spd1,
            stress,
            tdt,
            u1,
            ucdo,
            ucko,
            v1,
            vcdo,
            vcko,
            xmf,
            xmfd,
            self._cu,
            self._rt,
            self._a2,
        )

        self._tridi2(
            f1,
            f2,
            au,
            al,
            ad,
            self._cu,
            self._rt,
            self._a2,
        )

        self._recover_momentum_tendency_and_finish(
            delta,
            du,
            dusfc,
            dv,
            dvsfc,
            f1,
            f2,
            hpbl,
            hpblx,
            kpbl,
            kpblx,
            self._k_mask,
            u1,
            v1,
        )


class TranslatePBLInit(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "zi": {"shield": True, "kend": config.npz + 1},
            "zl": {"shield": True},
            "zm": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phil": {"shield": True},
            "chz": {"shield": True},
            "ckz": {"shield": True},
            "area": {"shield": True},
            "gdx": {"shield": True},
            "tke": {"shield": True},
            "q1": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "prn": {"shield": True, "kend": config.npz - 1},
            "kx1": {"shield": True, "index_variable": True},
            "prsi": {"shield": True, "kend": config.npz + 1},
            "kinver": {"shield": True},
            "tx1": {"shield": True},
            "tx2": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "kpblx": {"shield": True, "index_variable": True},
            "hpblx": {"shield": True},
            "pblflg": {"shield": True},
            "sfcflg": {"shield": True},
            "pcnvflg": {"shield": True},
            "scuflg": {"shield": True},
            "zorl": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "hpbl": {"shield": True},
            "rbsoil": {"shield": True},
            "radmin": {"shield": True},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "lcld": {"shield": True, "index_variable": True},
            "kcld": {"shield": True, "index_variable": True},
            "theta": {"shield": True},
            "prslk": {"shield": True},
            "psk": {"shield": True},
            "t1": {"shield": True},
            "pix": {"shield": True},
            "qlx": {"shield": True},
            "slx": {"shield": True},
            "thvx": {"shield": True},
            "qtx": {"shield": True},
            "thlx": {"shield": True},
            "thlvx": {"shield": True},
            "svx": {"shield": True},
            "thetae": {"shield": True},
            "gotvx": {"shield": True},
            "prsl": {"shield": True},
            "plyr": {"shield": True},
            "rhly": {"shield": True},
            "qstl": {"shield": True},
            "bf": {"shield": True, "kend": config.npz - 1},
            "cfly": {"shield": True},
            "crb": {"shield": True},
            "dtdz1": {"shield": True},
            "evap": {"shield": True},
            "heat": {"shield": True},
            "hlw": {"shield": True},
            "radx": {"shield": True, "kend": config.npz - 1},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "hsw": {"shield": True},
            "thermal": {"shield": True},
            "tsea": {"shield": True},
            "u10m": {"shield": True},
            "ustar": {"shield": True},
            "u1": {"shield": True},
            "v1": {"shield": True},
            "v10m": {"shield": True},
            "xmu": {"shield": True},
            "islimsk": {"shield": True},
            "xkzm_hx": {"shield": True},
            "xkzm_mx": {"shield": True},
            "tvx": {"shield": True},
            "tem1": {"shield": True, "serialname": "ser_tem"},
        }
        self.in_vars["parameters"] = [
            "ntcw",
            "ntiw",
            "ntke",
        ]
        self.out_vars = {
            "zi": {"shield": True, "kend": config.npz + 1},
            "zl": {"shield": True},
            "zm": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phil": {"shield": True},
            "chz": {"shield": True},
            "ckz": {"shield": True},
            "area": {"shield": True},
            "gdx": {"shield": True},
            "tke": {"shield": True},
            "q1": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "prn": {"shield": True, "kend": config.npz - 1},
            "kx1": {"shield": True, "index_variable": True},
            "prsi": {"shield": True, "kend": config.npz + 1},
            "kinver": {"shield": True},
            "tx1": {"shield": True},
            "tx2": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "kpblx": {"shield": True, "index_variable": True},
            "hpblx": {"shield": True},
            "pblflg": {"shield": True},
            "sfcflg": {"shield": True},
            "pcnvflg": {"shield": True},
            "scuflg": {"shield": True},
            "zorl": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "hpbl": {"shield": True},
            "rbsoil": {"shield": True},
            "radmin": {"shield": True},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "lcld": {"shield": True, "index_variable": True},
            "kcld": {"shield": True, "index_variable": True},
            "theta": {"shield": True},
            "prslk": {"shield": True},
            "psk": {"shield": True},
            "t1": {"shield": True},
            "pix": {"shield": True},
            "qlx": {"shield": True},
            "slx": {"shield": True},
            "thvx": {"shield": True},
            "qtx": {"shield": True},
            "thlx": {"shield": True},
            "thlvx": {"shield": True},
            "svx": {"shield": True},
            "thetae": {"shield": True},
            "gotvx": {"shield": True},
            "prsl": {"shield": True},
            "plyr": {"shield": True},
            "rhly": {"shield": True},
            "qstl": {"shield": True},
            "bf": {"shield": True, "kend": config.npz - 1},
            "cfly": {"shield": True},
            "crb": {"shield": True},
            "dtdz1": {"shield": True},
            "evap": {"shield": True},
            "heat": {"shield": True},
            "hlw": {"shield": True},
            "radx": {"shield": True, "kend": config.npz - 1},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "hsw": {"shield": True},
            "thermal": {"shield": True},
            "tsea": {"shield": True},
            "u10m": {"shield": True},
            "ustar": {"shield": True},
            "u1": {"shield": True},
            "v1": {"shield": True},
            "v10m": {"shield": True},
            "xmu": {"shield": True},
            "xkzm_hx": {"shield": True},
            "xkzm_mx": {"shield": True},
            "tvx": {"shield": True},
            "tem1": {"shield": True, "serialname": "ser_tem"},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs.pop("ntke")
        config.ntcw = int(inputs.pop("ntcw") - 1)
        config.ntiw = int(inputs.pop("ntiw") - 1)
        config.ntke = config.ntracers - 1
        config.xkzminv = 1.0
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["lcld"] = inputs["lcld"].astype(int)
        inputs["kcld"] = inputs["kcld"].astype(int)
        inputs["kpblx"] = inputs["kpblx"].astype(int)

        compute_func = InitTurb(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateMRF(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "crb": {"serialname": "crb", "shield": True},
            "flg": {"serialname": "flg", "shield": True},
            "kpblx": {"serialname": "kpblx", "shield": True, "index_variable": True},
            "rbdn": {"serialname": "rbdn", "shield": True},
            "rbup": {"serialname": "rbup", "shield": True},
            "rbsoil": {"serialname": "rbsoil", "shield": True},
            "thermal": {"serialname": "thermal", "shield": True},
            "thlvx": {"serialname": "thlvx", "shield": True},
            "u1": {"serialname": "u1", "shield": True},
            "v1": {"serialname": "v1", "shield": True},
            "zl": {"serialname": "zl", "shield": True},
            "evap": {"serialname": "evap", "shield": True},
            "fh": {"serialname": "fh", "shield": True},
            "fm": {"serialname": "fm", "shield": True},
            "gotvx": {"serialname": "gotvx", "shield": True},
            "zol": {"serialname": "zol", "shield": True},
            "heat": {"serialname": "heat", "shield": True},
            "hpbl": {"serialname": "hpbl", "shield": True},
            "hpblx": {"serialname": "hpblx", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "pblflg": {"serialname": "pblflg", "shield": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "phih": {"serialname": "phih", "shield": True},
            "phim": {"serialname": "phim", "shield": True},
            "sfcflg": {"serialname": "sfcflg", "shield": True},
            "sflux": {"serialname": "sflux", "shield": True},
            "theta": {"serialname": "theta", "shield": True},
            "ustar": {"serialname": "ustar", "shield": True},
            "vpert": {"serialname": "vpert", "shield": True},
            "zi": {"serialname": "zi", "shield": True, "kend": config.npz + 1},
        }

        self.out_vars = {
            "crb": {"serialname": "crb", "shield": True},
            "flg": {"serialname": "flg", "shield": True},
            "kpblx": {"serialname": "kpblx", "shield": True, "index_variable": True},
            "rbdn": {"serialname": "rbdn", "shield": True},
            "rbup": {"serialname": "rbup", "shield": True},
            "rbsoil": {"serialname": "rbsoil", "shield": True},
            "thermal": {"serialname": "thermal", "shield": True},
            "thlvx": {"serialname": "thlvx", "shield": True},
            "u1": {"serialname": "u1", "shield": True},
            "v1": {"serialname": "v1", "shield": True},
            "zl": {"serialname": "zl", "shield": True},
            "evap": {"serialname": "evap", "shield": True},
            "fh": {"serialname": "fh", "shield": True},
            "fm": {"serialname": "fm", "shield": True},
            "gotvx": {"serialname": "gotvx", "shield": True},
            "zol": {"serialname": "zol", "shield": True},
            "heat": {"serialname": "heat", "shield": True},
            "hpbl": {"serialname": "hpbl", "shield": True},
            "hpblx": {"serialname": "hpblx", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "pblflg": {"serialname": "pblflg", "shield": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "phih": {"serialname": "phih", "shield": True},
            "phim": {"serialname": "phim", "shield": True},
            "sfcflg": {"serialname": "sfcflg", "shield": True},
            "sflux": {"serialname": "sflux", "shield": True},
            "theta": {"serialname": "theta", "shield": True},
            "ustar": {"serialname": "ustar", "shield": True},
            "vpert": {"serialname": "vpert", "shield": True},
            "zi": {"serialname": "zi", "shield": True, "kend": config.npz + 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["kpblx"] = inputs["kpblx"].astype(int)

        compute_func = MRFScheme(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateThermalPBL(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "crb": {"serialname": "crb", "shield": True},
            "flg": {"serialname": "flg", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "rbdn": {"serialname": "rbdn", "shield": True},
            "rbup": {"serialname": "rbup", "shield": True},
            "thermal": {"serialname": "thermal", "shield": True},
            "thlvx": {"serialname": "thlvx", "shield": True},
            "u1": {"serialname": "u1", "shield": True},
            "v1": {"serialname": "v1", "shield": True},
            "zl": {"serialname": "zl", "shield": True},
            "hpbl": {"serialname": "hpbl", "shield": True},
            "pblflg": {"serialname": "pblflg", "shield": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "zi": {"serialname": "zi", "shield": True, "kend": config.npz + 1},
        }

        self.out_vars = {
            "crb": {"serialname": "crb", "shield": True},
            "flg": {"serialname": "flg", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "rbdn": {"serialname": "rbdn", "shield": True},
            "rbup": {"serialname": "rbup", "shield": True},
            "thermal": {"serialname": "thermal", "shield": True},
            "thlvx": {"serialname": "thlvx", "shield": True},
            "u1": {"serialname": "u1", "shield": True},
            "v1": {"serialname": "v1", "shield": True},
            "zl": {"serialname": "zl", "shield": True},
            "hpbl": {"serialname": "hpbl", "shield": True},
            "pblflg": {"serialname": "pblflg", "shield": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "zi": {"serialname": "zi", "shield": True, "kend": config.npz + 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs["kpbl"] = inputs["kpbl"].astype(int)

        compute_func = ThermalPBL(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateStratocumulus(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "flg": {"shield": True},
            "kcld": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "lcld": {"shield": True, "index_variable": True},
            "radmin": {"shield": True},
            "radx": {"shield": True, "kend": config.npz - 1},
            "qlx": {"shield": True},
            "scuflg": {"shield": True},
            "zl": {"shield": True},
        }

        self.out_vars = {
            "flg": {"shield": True},
            "kcld": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "lcld": {"shield": True, "index_variable": True},
            "radmin": {"shield": True},
            "radx": {"shield": True, "kend": config.npz - 1},
            "qlx": {"shield": True},
            "scuflg": {"shield": True},
            "zl": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs["kcld"] = inputs["kcld"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["lcld"] = inputs["lcld"].astype(int)

        compute_func = Stratocumulus(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslatePBLAML(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
        }

        self.out_vars = {
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl

        compute_func = PBLAML(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateTKETridiagEle(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"serialname": "ad", "shield": True},
            "al": {"serialname": "al", "shield": True, "kend": config.npz - 1},
            "au": {"serialname": "au", "shield": True, "kend": config.npz - 1},
            "delta": {"serialname": "delta", "shield": True},
            "dkq": {"serialname": "dkq", "shield": True, "kend": config.npz - 1},
            "f1": {"serialname": "f1", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "krad": {"serialname": "krad", "shield": True, "index_variable": True},
            "mrad": {"serialname": "mrad", "shield": True, "index_variable": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "prsl": {"serialname": "prsl", "shield": True},
            "qcdo": {"serialname": "qcdo", "shield": True},
            "qcko": {"serialname": "qcko", "shield": True},
            "rdzt": {"serialname": "rdzt", "shield": True, "kend": config.npz - 1},
            "scuflg": {"serialname": "scuflg", "shield": True},
            "tke": {"serialname": "tke", "shield": True},
            "xmf": {"serialname": "xmf", "shield": True},
            "xmfd": {"serialname": "xmfd", "shield": True},
        }

        self.out_vars = {
            "ad": {"serialname": "ad", "shield": True},
            "al": {"serialname": "al", "shield": True, "kend": config.npz - 1},
            "au": {"serialname": "au", "shield": True, "kend": config.npz - 1},
            "delta": {"serialname": "delta", "shield": True},
            "dkq": {"serialname": "dkq", "shield": True, "kend": config.npz - 1},
            "f1": {"serialname": "f1", "shield": True},
            "kpbl": {"serialname": "kpbl", "shield": True, "index_variable": True},
            "krad": {"serialname": "krad", "shield": True, "index_variable": True},
            "mrad": {"serialname": "mrad", "shield": True, "index_variable": True},
            "pcnvflg": {"serialname": "pcnvflg", "shield": True},
            "prsl": {"serialname": "prsl", "shield": True},
            "qcdo": {"serialname": "qcdo", "shield": True},
            "qcko": {"serialname": "qcko", "shield": True},
            "rdzt": {"serialname": "rdzt", "shield": True, "kend": config.npz - 1},
            "scuflg": {"serialname": "scuflg", "shield": True},
            "tke": {"serialname": "tke", "shield": True},
            "xmf": {"serialname": "xmf", "shield": True},
            "xmfd": {"serialname": "xmfd", "shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)

        compute_func = TKETridiag(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslatePrandtl(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
        }

        self.out_vars = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)

        compute_func = Prandtl(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateTKEPredict(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ele": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "tke": {"shield": True},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prod": {"shield": True, "kend": config.npz - 1},
        }

        self.out_vars = {
            "ele": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "tke": {"shield": True},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prod": {"shield": True, "kend": config.npz - 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl

        compute_func = TKEPredict(
            self.stencil_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateEdDiffShear(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "elm": {"shield": True},
            "gotvx": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "pcnvflg": {"shield": True},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "tke": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "zl": {"shield": True},
        }

        self.out_vars = {
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "elm": {"shield": True},
            "gotvx": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "pcnvflg": {"shield": True},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "tke": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "zl": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = EdDiffShear(
            self.stencil_factory,
            quantity_factory,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateUpDownTKE(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "pcnvflg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "scuflg": {"shield": True},
            "tke": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "zl": {"shield": True},
            "mrad": {"shield": True, "index_variable": True},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
        }

        self.out_vars = {
            "pcnvflg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "scuflg": {"shield": True},
            "tke": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "zl": {"shield": True},
            "mrad": {"shield": True, "index_variable": True},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)
        config = self.config.pbl

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = UpDownTKE(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateMomentTridiagComp(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "diss": {"shield": True, "kend": config.npz - 1},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dtdz1": {"shield": True},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "spd1": {"shield": True},
            "stress": {"shield": True},
            "tdt": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
        }
        self.out_vars = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "diss": {"shield": True, "kend": config.npz - 1},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dtdz1": {"shield": True},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "spd1": {"shield": True},
            "stress": {"shield": True},
            "tdt": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)
        config = self.config.pbl

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = MomentTridiagComp(self.stencil_factory, quantity_factory, config)

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateHeatTracerTridiagEle(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "t1": {"shield": True},
            "q1": {"shield": True},
            "dtdz1": {"shield": True},
            "evap": {"shield": True},
            "heat": {"shield": True},
        }
        self.out_vars = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "t1": {"shield": True},
            "q1": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)
        config = self.config.pbl

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = HeatTracerTridiag(self.stencil_factory, quantity_factory, config)

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateTKETendencyCalc(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "f1": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "tke": {"shield": True},
            "q1": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "rtg": {"shield": True},
        }
        self.in_vars["parameters"] = [
            "ntcw",
            "ntiw",
            "ntke",
        ]
        self.out_vars = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "f1": {"shield": True},
            "rtg": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        inputs.pop("ntke")
        config.ntcw = int(inputs.pop("ntcw") - 1)
        config.ntiw = int(inputs.pop("ntiw") - 1)
        config.ntke = config.ntracers - 1

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = TKETendencyCalc(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateHeatTracerTendencyCalc(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "evap": {"shield": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "t1": {"shield": True},
            "q1": {"shield": True},
            "dtdz1": {"shield": True},
            "heat": {"shield": True},
            "rtg": {"shield": True},
            "tdt": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
        }
        self.out_vars = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "rtg": {"shield": True},
            "tdt": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        config = self.config.pbl
        config.ntke = config.ntracers - 1

        self.make_storage_data_input_vars(inputs)

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = HeatTracerTendencyCalc(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateMomentTendencyCalc(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "diss": {"shield": True, "kend": config.npz - 1},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dtdz1": {"shield": True},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "kpbl": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "prsl": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "spd1": {"shield": True},
            "stress": {"shield": True},
            "tdt": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "hpbl": {"shield": True},
            "hpblx": {"shield": True},
            "kpblx": {"shield": True, "index_variable": True},
        }
        self.in_vars["parameters"] = ["delt"]
        self.out_vars = {
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "f1": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        config.ntke = config.ntracers - 1
        config.dt_atmos = inputs.pop("delt")

        inputs["kpblx"] = inputs["kpblx"].astype(int)
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        compute_func = MomentTendencyCalc(
            self.stencil_factory,
            quantity_factory,
            config,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateHalf2(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prsl": {"shield": True},
            "rtg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "spd1": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "evap": {"shield": True},
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "f1": {"shield": True},
            "hpblx": {"shield": True},
            "kpblx": {"shield": True, "index_variable": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "t1": {"shield": True},
            "dtdz1": {"shield": True},
            "heat": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "tdt": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "zm": {"shield": True},
            "plyr": {"shield": True},
            "pix": {"shield": True},
            "thlx": {"shield": True},
            "vpert": {"shield": True},
            "thlvx": {"shield": True},
            "thetae": {"shield": True},
            "radmin": {"shield": True},
        }
        self.in_vars["parameters"] = [
            "delt",
            "ntcw",
            "ntiw",
            "ntke",
            "ntrac1",
            "kmpbl",
            "kmscu",
        ]
        self.out_vars = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prsl": {"shield": True},
            "rtg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "spd1": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "evap": {"shield": True},
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "f1": {"shield": True},
            "hpblx": {"shield": True},
            "kpblx": {"shield": True, "index_variable": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "t1": {"shield": True},
            "dtdz1": {"shield": True},
            "heat": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "tdt": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        config.ntke = config.ntracers - 1
        config.dt_atmos = inputs.pop("delt")
        config.ntiw = inputs.pop("ntiw") - 1
        config.ntcw = inputs.pop("ntcw") - 1
        inputs.pop("ntke")
        inputs.pop("ntrac1")

        inputs["kpblx"] = inputs["kpblx"].astype(int)
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        pcnvflg = quantity_factory.from_array(
            data=inputs.pop("pcnvflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )

        scuflg = quantity_factory.from_array(
            data=inputs.pop("scuflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )

        compute_func = Half2(
            self.stencil_factory,
            quantity_factory,
            config,
            int(inputs.pop("kmpbl")),
            int(inputs.pop("kmscu")),
        )

        compute_func(**inputs, pcnvflg=pcnvflg, scuflg=scuflg)
        inputs["pcnvflg"] = pcnvflg
        inputs["scuflg"] = scuflg

        return self.slice_output(inputs)


class TranslateSCUEnd(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prsl": {"shield": True},
            "rtg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "spd1": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "evap": {"shield": True},
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "f1": {"shield": True},
            "hpblx": {"shield": True},
            "kpblx": {"shield": True, "index_variable": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "t1": {"shield": True},
            "dtdz1": {"shield": True},
            "heat": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "tdt": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
            "zm": {"shield": True},
            "plyr": {"shield": True},
            "pix": {"shield": True},
            "thlx": {"shield": True},
            "vpert": {"shield": True},
            "thlvx": {"shield": True},
            "thetae": {"shield": True},
            "radmin": {"shield": True},
        }
        self.in_vars["parameters"] = [
            "delt",
            "ntcw",
            "ntiw",
            "ntke",
            "ntrac1",
            "kmpbl",
            "kmscu",
        ]
        self.out_vars = {
            "ckz": {"shield": True},
            "chz": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "pcnvflg": {"shield": True},
            "zi": {"shield": True, "kend": config.npz + 1},
            "phih": {"shield": True},
            "zldn": {"shield": True},
            "zlup": {"shield": True},
            "thvx": {"shield": True},
            "tke": {"shield": True},
            "gotvx": {"shield": True},
            "zl": {"shield": True},
            "tsea": {"shield": True},
            "q1": {"shield": True},
            "rlam": {"shield": True, "kend": config.npz - 1},
            "ele": {"shield": True},
            "elm": {"shield": True},
            "zol": {"shield": True},
            "gdx": {"shield": True},
            "phii": {"shield": True, "kend": config.npz + 1},
            "phim": {"shield": True},
            "prn": {"shield": True, "kend": config.npz - 1},
            "bf": {"shield": True, "kend": config.npz - 1},
            "buod": {"shield": True},
            "buou": {"shield": True},
            "dku": {"shield": True, "kend": config.npz - 1},
            "dkt": {"shield": True, "kend": config.npz - 1},
            "dkq": {"shield": True, "kend": config.npz - 1},
            "mrad": {"shield": True, "index_variable": True},
            "krad": {"shield": True, "index_variable": True},
            "pblflg": {"shield": True},
            "prod": {"shield": True, "kend": config.npz - 1},
            "radj": {"shield": True},
            "rdzt": {"shield": True, "kend": config.npz - 1},
            "scuflg": {"shield": True},
            "sflux": {"shield": True},
            "shr2": {"shield": True, "kend": config.npz - 1},
            "stress": {"shield": True},
            "u1": {"shield": True},
            "ucdo": {"shield": True},
            "ucko": {"shield": True},
            "ustar": {"shield": True},
            "v1": {"shield": True},
            "vcdo": {"shield": True},
            "vcko": {"shield": True},
            "xkzo": {"shield": True, "kend": config.npz - 1},
            "xkzmo": {"shield": True, "kend": config.npz - 1},
            "xmf": {"shield": True},
            "xmfd": {"shield": True},
            "rle": {"shield": True, "kend": config.npz - 1},
            "diss": {"shield": True, "kend": config.npz - 1},
            "prsl": {"shield": True},
            "rtg": {"shield": True},
            "qcdo": {"shield": True},
            "qcko": {"shield": True},
            "f2": {"shield": True, "serialname": "f2_ser"},
            "spd1": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
            "xlamde": {"shield": True, "kend": config.npz - 1},
            "evap": {"shield": True},
            "ad": {"shield": True},
            "al": {"shield": True, "kend": config.npz - 1},
            "au": {"shield": True, "kend": config.npz - 1},
            "delta": {"shield": True},
            "f1": {"shield": True},
            "hpblx": {"shield": True},
            "kpblx": {"shield": True, "index_variable": True},
            "tcdo": {"shield": True},
            "tcko": {"shield": True},
            "t1": {"shield": True},
            "dtdz1": {"shield": True},
            "heat": {"shield": True},
            "dtsfc": {"shield": True},
            "dqsfc": {"shield": True},
            "tdt": {"shield": True},
            "du": {"shield": True},
            "dv": {"shield": True},
            "dusfc": {"shield": True},
            "dvsfc": {"shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = self.config.pbl
        config.ntke = config.ntracers - 1
        config.dt_atmos = inputs.pop("delt")
        config.ntiw = inputs.pop("ntiw") - 1
        config.ntcw = inputs.pop("ntcw") - 1
        inputs.pop("ntke")
        inputs.pop("ntrac1")

        inputs["kpblx"] = inputs["kpblx"].astype(int)
        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["krad"] = inputs["krad"].astype(int)
        inputs["mrad"] = inputs["mrad"].astype(int)

        pcnvflg = quantity_factory.from_array(
            data=inputs.pop("pcnvflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )

        scuflg = quantity_factory.from_array(
            data=inputs.pop("scuflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )

        compute_func = SCUEnd(
            self.stencil_factory,
            quantity_factory,
            config,
            int(inputs.pop("kmpbl")),
            int(inputs.pop("kmscu")),
        )

        compute_func(**inputs, pcnvflg=pcnvflg, scuflg=scuflg)
        inputs["pcnvflg"] = pcnvflg
        inputs["scuflg"] = scuflg

        return self.slice_output(inputs)
