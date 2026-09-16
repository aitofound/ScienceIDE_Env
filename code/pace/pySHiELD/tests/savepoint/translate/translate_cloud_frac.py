import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.cloud_fraction import (
    cloud_scheme_1,
    cloud_scheme_2,
    cloud_scheme_3,
    cloud_scheme_4,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


def cloud_fraction_test(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qa: FloatField,
    temperature: FloatField,
    density: FloatField,
    pz: FloatField,
    te: FloatField,
    h_var: FloatFieldIJ,
    gsize: FloatFieldIJ,
    qsi: FloatField,
    dqidt: FloatField,
    qsw: FloatField,
    dqwdt: FloatField,
):

    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        cfflag,
        li00,
        lv00,
        rad_graupel,
        rad_rain,
        rad_snow,
        t_wfr,
    )

    with computation(PARALLEL), interval(...):
        (
            q_liq,
            q_solid,
            cvm,
            te,
            lcpk,
            icpk,
            tcpk,
            tcp3,
        ) = physfun.calc_heat_cap_and_latent_heat_coeff(
            qvapor, qliquid, qrain, qice, qsnow, qgraupel, temperature
        )

        # Combine water species
        ice = q_solid
        q_solid = qice
        if rad_snow:
            q_solid += qsnow
            if rad_graupel:
                q_solid += qgraupel

        liq = q_liq
        q_liq = qliquid
        if rad_rain:
            q_liq += qrain

        q_cond = q_liq + q_solid
        qpz = qvapor + q_cond

        # use the "liquid - frozen water temperature" (tin)
        # to compute saturated specific humidity
        ice = ice - q_solid
        liq = liq - q_liq
        tin = (te - lv00 * qpz + li00 * ice) / (
            1.0 + qpz * c1_vap + liq * c1_liq + ice * c1_ice
        )

        if tin <= t_wfr:
            qstar = qsi
            dqdt = dqidt
        elif tin >= mpcons.TICE0:
            qstar = qsw
            dqdt = dqwdt
        else:
            if q_cond > mpcons.QCMIN:
                rqi = q_solid / q_cond
            else:
                rqi = (mpcons.TICE0 - tin) / (mpcons.TICE0 - t_wfr)
            qstar = rqi * qsi + (1.0 - rqi) * qsw
            dqdt = 0.5 * (dqidt + dqwdt)

        # Cloud schemes
        rh = qpz / qstar

        if cfflag == 1:
            qa = cloud_scheme_1(
                qpz,
                qstar,
                q_cond,
                qa,
                pz,
                rh,
                h_var,
            )
        elif cfflag == 2:
            qa = cloud_scheme_2(
                qstar,
                q_cond,
                qa,
                rh,
            )
        elif cfflag == 3:
            qa = cloud_scheme_3(
                q_cond,
                q_liq,
                q_solid,
                qa,
                gsize,
            )
        else:  # cfflag == 4:
            qa = cloud_scheme_4(
                q_cond,
                qa,
                gsize,
            )


class CloudFractionTest:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: GFDLCloudMPConfig,
    ):
        self._idx: GridIndexing = stencil_factory.grid_indexing

        self._te = quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")

        self._cloud_fraction = stencil_factory.from_origin_domain(
            func=cloud_fraction_test,
            externals={
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "cfflag": config.cfflag,
                "li00": config.li00,
                "lv00": config.lv00,
                "li20": config.li20,
                "d1_vap": config.d1_vap,
                "d1_ice": config.d1_ice,
                "rad_graupel": config.rad_graupel,
                "rad_rain": config.rad_rain,
                "rad_snow": config.rad_snow,
                "t_wfr": config.t_wfr,
                "tice": mpcons.TICE0,
                "cld_min": config.cld_min,
                "do_cld_adj": config.do_cld_adj,
                "f_dq_m": config.f_dq_m,
                "f_dq_p": config.f_dq_p,
                "icloud_f": config.icloud_f,
                "rh_thres": config.rh_thres,
                "xr_a": config.xr_a,
                "xr_b": config.xr_b,
                "xr_c": config.xr_c,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

    def __call__(
        self,
        qvapor: FloatField,
        qliquid: FloatField,
        qrain: FloatField,
        qice: FloatField,
        qsnow: FloatField,
        qgraupel: FloatField,
        qa: FloatField,
        temperature: FloatField,
        density: FloatField,
        pz: FloatField,
        h_var: FloatFieldIJ,
        gsize: FloatFieldIJ,
        qsi: FloatField,
        dqidt: FloatField,
        qsw: FloatField,
        dqwdt: FloatField,
    ):
        """
        Calculates cloud fraction diagnostic.
        Args:
            qvapor (in):
            qliquid (in):
            qrain (in):
            qice (in):
            qsnow (in):
            qgraupel (in):
            qa (out):
            temperature (in):
            density (in):
            pz (in):
            h_var (in):
            gsize (in):
        """
        self._cloud_fraction(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            qa,
            temperature,
            density,
            pz,
            self._te,
            h_var,
            gsize,
            qsi,
            dqidt,
            qsw,
            dqwdt,
        )


class TranslateCloudFrac(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "cf_qv", "shield": True},
            "qliquid": {"serialname": "cf_ql", "shield": True},
            "qrain": {"serialname": "cf_qr", "shield": True},
            "qice": {"serialname": "cf_qi", "shield": True},
            "qsnow": {"serialname": "cf_qs", "shield": True},
            "qgraupel": {"serialname": "cf_qg", "shield": True},
            "qa": {"serialname": "cf_qa", "shield": True},
            "temperature": {"serialname": "cf_pt", "shield": True},
            "density": {"serialname": "cf_den", "shield": True},
            "pz": {"serialname": "cf_pz", "shield": True},
            "h_var": {"serialname": "cf_h_var", "shield": True},
            "gsize": {"serialname": "cf_gsize", "shield": True},
            "qsi": {"serialname": "cf_qsi", "shield": True},
            "dqidt": {"serialname": "cf_dqidt", "shield": True},
            "qsw": {"serialname": "cf_qsw", "shield": True},
            "dqwdt": {"serialname": "cf_dqwdt", "shield": True},
        }
        # self.max_error = 1.0e-12

        self.out_vars = {
            "qa": {"serialname": "cf_qa", "kend": self.config.npz, "shield": True},
        }

        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing
        self.mpconfig = GFDLCloudMPConfig(
            dt_full=self.config.dt_atmos,
            hydrostatic=self.config.hydrostatic,
            npx=self.config.npx,
            npy=self.config.npy,
            npz=self.config.npz,
            nwat=self.config.nwat,
            do_qa=self.config.do_qa,
            do_inline_mp=self.config.do_inline_mp,
            c_cracw=self.config.c_cracw,
            c_paut=self.config.c_paut,
            c_pgacs=self.config.c_pgacs,
            c_psaci=self.config.c_psaci,
            ccn_l=self.config.ccn_l,
            ccn_o=self.config.ccn_o,
            const_vg=self.config.const_vg,
            const_vi=self.config.const_vi,
            const_vr=self.config.const_vr,
            const_vs=self.config.const_vs,
            vs_fac=self.config.vs_fac,
            vg_fac=self.config.vg_fac,
            vi_fac=self.config.vi_fac,
            vr_fac=self.config.vr_fac,
            de_ice=self.config.de_ice,
            layout=self.config.layout,
            tau_imlt=self.config.tau_imlt,
            tau_i2s=self.config.tau_i2s,
            tau_g2v=self.config.tau_g2v,
            tau_v2g=self.config.tau_v2g,
            ql_mlt=self.config.ql_mlt,
            qs_mlt=self.config.qs_mlt,
            t_sub=self.config.t_sub,
            qi_gen=self.config.qi_gen,
            qi_lim=self.config.qi_lim,
            qi0_max=self.config.qi0_max,
            rad_snow=self.config.rad_snow,
            rad_rain=self.config.rad_rain,
            dw_ocean=self.config.dw_ocean,
            dw_land=self.config.dw_land,
            tau_l2v=self.config.tau_l2v,
            c2l_ord=self.config.c2l_ord,
            do_sedi_heat=self.config.do_sedi_heat,
            do_sedi_w=self.config.do_sedi_w,
            fast_sat_adj=self.config.fast_sat_adj,
            qc_crt=self.config.qc_crt,
            fix_negative=self.config.fix_negative,
            irain_f=self.config.irain_f,
            mp_time=self.config.mp_time,
            prog_ccn=self.config.prog_ccn,
            qi0_crt=self.config.qi0_crt,
            qs0_crt=self.config.qs0_crt,
            rh_inc=self.config.rh_inc,
            rh_inr=self.config.rh_inr,
            rthresh=self.config.rthresh,
            sedi_transport=self.config.sedi_transport,
            use_ppm=self.config.use_ppm,
            vg_max=self.config.vg_max,
            vi_max=self.config.vi_max,
            vr_max=self.config.vr_max,
            vs_max=self.config.vs_max,
            z_slope_ice=self.config.z_slope_ice,
            z_slope_liq=self.config.z_slope_liq,
            tice=self.config.tice,
            alin=self.config.alin,
            clin=self.config.clin,
        )
        self.mpconfig.do_mp_table_emulation = True
        print(f"cfflag is {self.mpconfig.cfflag}")

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npy - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = CloudFractionTest(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
