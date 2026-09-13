import math

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.warm_rain import (
    accrete_rain,
    autoconvert_water_rain,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


def evaporate_rain(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    delp: FloatField,
    temperature: FloatField,
    density: FloatField,
    density_factor: FloatField,
    reevap: FloatFieldIJ,
    h_var: FloatFieldIJ,
    tin: FloatField,
    qsat: FloatField,
    dqdt: FloatField,
    dqh: FloatField,
    bool_check: FloatField,
    dq: FloatField,
    sink0: FloatField,
    sink: FloatField,
    vc: FloatField,
):
    """
    Rain evaporation to form water vapor, Lin et al. (1983)
    Fortran name is prevp
    """
    from __externals__ import (
        blinr,
        c1,
        c1_ice,
        c1_liq,
        c1_vap,
        c2,
        c3,
        c4,
        c5,
        fac_revap,
        lv00,
        mur,
        rhc_revap,
        t_wfr,
        timestep,
        use_rhc_revap,
    )

    with computation(FORWARD), interval(-1, None):
        reevap = 0.0

    with computation(FORWARD), interval(...):
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
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
        )

        tin = (temperature * cvm - lv00 * qliquid) / (
            1.0 + (qvapor + qliquid) * c1_vap + qrain * c1_liq + q_solid * c1_ice
        )

        bool_check = 0.0
        dq = -1.0
        sink0 = 0.0
        sink = 0.0
        vc = 0.0

        # calculate supersaturation and subgrid variability of water
        qpz = qvapor + qliquid
        qsat, dqdt = physfun.sat_spec_hum_water(tin, density)
        dqv = qsat - qvapor

        dqh = max(qliquid, h_var * max(qpz, mpcons.QCMIN))
        dqh = min(dqh, 0.2 * qpz)

        q_minus = qpz - dqh
        q_plus = qpz + dqh

        rh_tem = qpz / qsat

        if (
            (temperature > t_wfr)
            and (qrain > mpcons.QCMIN)
            and (dqv > 0.0)
            and (qsat > q_minus)
        ):
            bool_check = 1.0
            if qsat > q_plus:
                dq = qsat - qpz
            else:
                dq = 0.25 * (qsat - q_minus) ** 2 / dqh
            qden = qrain * density
            t2 = tin * tin
            sink0 = physfun.sublimation_function(
                t2,
                dq,
                qden,
                qsat,
                density,
                density_factor,
                lcpk,
                cvm,
                c1,
                c2,
                c3,
                c4,
                c5,
                blinr,
                mur,
            )
            vc = physfun.vent_coeff(qden, density_factor, c2, c3, blinr, mur)
            sink = min(
                qrain, min(timestep * fac_revap * sink0, dqv / (1.0 + lcpk * dqdt))
            )
            if (use_rhc_revap) and (rh_tem >= rhc_revap):
                sink = 0

            reevap += sink * delp

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cvm,
                temperature,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = physfun.update_hydrometeors_and_temperatures(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                sink,
                0.0,
                -sink,
                0.0,
                0.0,
                0.0,
                te,
            )


class RainFunction:
    def __init__(
        self,
        stencil_factory,
        config,
        timestep: float,
    ):

        self._idx = stencil_factory.grid_indexing
        self._fac_revap = 1.0
        if config.tau_revp > 1.0e-6:
            self._fac_revap = 1.0 - math.exp(-timestep / config.tau_revp)

        fac_rc = (4.0 / 3.0) * constants.PI * mpcons.RHO_W * config.rthresh**3
        aone = 2.0 / 9.0 * (3.0 / 4.0) ** (4.0 / 3.0) / constants.PI ** (1.0 / 3.0)
        cpaut = config.c_paut * aone * constants.GRAV / mpcons.VISD

        self._evaporate_rain = stencil_factory.from_origin_domain(
            func=evaporate_rain,
            externals={
                "timestep": timestep,
                "mur": config.mur,
                "blinr": config.blinr,
                "c1_vap": config.c1_vap,
                "c1_liq": config.c1_liq,
                "c1_ice": config.c1_ice,
                "t_wfr": config.t_wfr,
                "fac_revap": self._fac_revap,
                "use_rhc_revap": config.use_rhc_revap,
                "rhc_revap": config.rhc_revap,
                "d1_ice": config.d1_ice,
                "d1_vap": config.d1_vap,
                "li00": config.li00,
                "li20": config.li20,
                "lv00": config.lv00,
                "tice": mpcons.TICE0,
                "c1": config.crevp_1,
                "c2": config.crevp_2,
                "c3": config.crevp_3,
                "c4": config.crevp_4,
                "c5": config.crevp_5,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._accrete_rain = stencil_factory.from_origin_domain(
            func=accrete_rain,
            externals={
                "timestep": timestep,
                "t_wfr": config.t_wfr,
                "do_new_acc_water": config.do_new_acc_water,
                "cracw": config.cracw,
                "acco0_4": config.acco[0][4],
                "acco1_4": config.acco[1][4],
                "acco2_4": config.acco[2][4],
                "acc8": config.acc[8],
                "acc9": config.acc[9],
                "blinr": config.blinr,
                "mur": config.mur,
                "vdiffflag": config.vdiffflag,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._autoconvert_water_rain = stencil_factory.from_origin_domain(
            func=autoconvert_water_rain,
            externals={
                "timestep": timestep,
                "t_wfr": config.t_wfr,
                "irain_f": config.irain_f,
                "z_slope_liq": config.z_slope_liq,
                "do_psd_water_num": config.do_psd_water_num,
                "pcaw": config.pcaw,
                "pcbw": config.pcbw,
                "muw": config.muw,
                "fac_rc": fac_rc,
                "cpaut": cpaut,
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
        temperature: FloatField,
        delp: FloatField,
        density: FloatField,
        density_factor: FloatField,
        vterminal_water: FloatField,
        vterminal_rain: FloatField,
        cloud_condensation_nuclei: FloatField,
        reevap: FloatFieldIJ,
        h_var: FloatFieldIJ,
        tin: FloatField,
        qsat: FloatField,
        dqdt: FloatField,
        dqh: FloatField,
        bool_check: FloatField,
        dq: FloatField,
        sink0: FloatField,
        sink: FloatField,
        vc: FloatField,
    ):
        """
        Warm rain cloud microphysics
        Args:
            qvapor (inout):
            qliquid (inout):
            qrain (inout):
            qice (inout):
            qsnow (inout):
            qgraupel (inout):
            temperature (inout):
            delp (in):
            density (in):
            density_factor (in):
            vterminal_water (in):
            vterminal_rain (in):
            cloud_condensation_nuclei (inout):
            reevap (inout):
            h_var (in):
        """
        self._evaporate_rain(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            delp,
            temperature,
            density,
            density_factor,
            reevap,
            h_var,
            tin,
            qsat,
            dqdt,
            dqh,
            bool_check,
            dq,
            sink0,
            sink,
            vc,
        )

        # self._accrete_rain(
        #     qliquid,
        #     qrain,
        #     temperature,
        #     density,
        #     density_factor,
        #     vterminal_water,
        #     vterminal_rain,
        # )

        # self._autoconvert_water_rain(
        #     qliquid,
        #     qrain,
        #     cloud_condensation_nuclei,
        #     temperature,
        #     density,
        #     h_var,
        # )


class TranslateWRainSubFunc(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "ws_qv", "shield": True},
            "qliquid": {"serialname": "ws_ql", "shield": True},
            "qrain": {"serialname": "ws_qr", "shield": True},
            "qice": {"serialname": "ws_qi", "shield": True},
            "qsnow": {"serialname": "ws_qs", "shield": True},
            "qgraupel": {"serialname": "ws_qg", "shield": True},
            "temperature": {"serialname": "ws_pt", "shield": True},
            "density": {"serialname": "ws_den", "shield": True},
            "density_factor": {"serialname": "ws_denfac", "shield": True},
            "vterminal_water": {"serialname": "ws_vtw", "shield": True},
            "vterminal_rain": {"serialname": "ws_vtr", "shield": True},
            "delp": {"serialname": "ws_delp", "shield": True},
            "h_var": {"serialname": "ws_h_var", "shield": True},
            "cloud_condensation_nuclei": {"serialname": "ws_ccn", "shield": True},
            "reevap": {"serialname": "ws_reevap", "shield": True},
            "tin": {"serialname": "ws_tin", "shield": True},
            "qsat": {"serialname": "ws_qsat", "shield": True},
            "dqdt": {"serialname": "ws_dqdt", "shield": True},
            "dqh": {"serialname": "ws_dqh", "shield": True},
            "bool_check": {"serialname": "ws_bool_check", "shield": True},
            "dq": {"serialname": "ws_dq", "shield": True},
            "sink0": {"serialname": "ws_sink0", "shield": True},
            "sink": {"serialname": "ws_sink", "shield": True},
            "vc": {"serialname": "ws_vc", "shield": True},
        }

        self.in_vars["parameters"] = [
            "dt",
        ]

        self.out_vars = {
            "qvapor": {"serialname": "ws_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "ws_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "ws_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "ws_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "ws_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "ws_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "ws_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_condensation_nuclei": {
                "serialname": "ws_ccn",
                "kend": self.config.npz,
                "shield": True,
            },
            "reevap": {"serialname": "ws_reevap", "shield": True},
            "tin": {"serialname": "ws_tin", "kend": self.config.npz, "shield": True},
            "qsat": {"serialname": "ws_qsat", "kend": self.config.npz, "shield": True},
            "dqdt": {"serialname": "ws_dqdt", "kend": self.config.npz, "shield": True},
            "dqh": {"serialname": "ws_dqh", "kend": self.config.npz, "shield": True},
            "bool_check": {
                "serialname": "ws_bool_check",
                "kend": self.config.npz,
                "shield": True,
            },
            "dq": {"serialname": "ws_dq", "kend": self.config.npz, "shield": True},
            "sink0": {
                "serialname": "ws_sink0",
                "kend": self.config.npz,
                "shield": True,
            },
            "sink": {"serialname": "ws_sink", "kend": self.config.npz, "shield": True},
            "vc": {"serialname": "ws_vc", "kend": self.config.npz, "shield": True},
        }

        self.max_error = 5.0e-13  # only qrain, everything else is good

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

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = RainFunction(
            self.stencil_factory,
            self.mpconfig,
            timestep=inputs.pop("dt"),
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
