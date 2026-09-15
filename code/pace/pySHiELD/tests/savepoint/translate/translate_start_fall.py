import ndsl.constants as constants
from ndsl import StencilFactory
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.terminal_fall import (
    prep_terminal_fall,
    update_energy_wind_heat_post_fall,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class StartFall:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config,
    ):
        self._idx = stencil_factory.grid_indexing
        self.config = config
        self._prep_terminal_fall = stencil_factory.from_origin_domain(
            func=prep_terminal_fall,
            externals={
                "do_sedi_w": config.do_sedi_w,
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "c_air": config.c_air,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

    def __call__(
        self,
        q_fall: FloatField,
        delp: FloatField,
        qvapor: FloatField,
        qliquid: FloatField,
        qrain: FloatField,
        qice: FloatField,
        qsnow: FloatField,
        qgraupel: FloatField,
        temperature: FloatField,
        dm: FloatField,
        tot_e_initial: FloatFieldIJ,
        no_fall: FloatFieldIJ,
    ):
        self._prep_terminal_fall(
            q_fall,
            delp,
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            dm,
            tot_e_initial,
            no_fall,
        )


class EndFall:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config,
    ):
        self._idx = stencil_factory.grid_indexing
        self.config = config
        self._update_energy_wind_heat_post_fall = stencil_factory.from_origin_domain(
            func=update_energy_wind_heat_post_fall,
            externals={
                "do_sedi_uv": config.do_sedi_uv,
                "do_sedi_w": config.do_sedi_w,
                "do_sedi_heat": config.do_sedi_heat,
                "cw": constants.C_ICE,
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "c_air": config.c_air,
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
        ua: FloatField,
        va: FloatField,
        wa: FloatField,
        temperature: FloatField,
        delp: FloatField,
        delz: FloatField,
        flux: FloatField,
        dm: FloatField,
        v_terminal: FloatField,
        tmp_energy1: FloatFieldIJ,
        tmp_energy2: FloatFieldIJ,
        column_energy_change: FloatFieldIJ,
        no_fall: FloatFieldIJ,
    ):
        self._update_energy_wind_heat_post_fall(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            ua,
            va,
            wa,
            temperature,
            delp,
            delz,
            flux,
            dm,
            v_terminal,
            tmp_energy1,
            tmp_energy2,
            column_energy_change,
            no_fall,
        )


class TranslateStartFall(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "q_fall": {"serialname": "sf_qf", "shield": True},
            "qvapor": {"serialname": "sf_qv", "shield": True},
            "qliquid": {"serialname": "sf_ql", "shield": True},
            "qrain": {"serialname": "sf_qr", "shield": True},
            "qice": {"serialname": "sf_qi", "shield": True},
            "qsnow": {"serialname": "sf_qs", "shield": True},
            "qgraupel": {"serialname": "sf_qg", "shield": True},
            "temperature": {"serialname": "sf_pt", "shield": True},
            "delp": {"serialname": "sf_delp", "shield": True},
            "dm": {"serialname": "sf_dm", "shield": True},
            "tot_e_initial": {"serialname": "sf_e1", "shield": True},
            "no_fall": {"serialname": "sf_nf", "shield": True},
        }

        self.out_vars = {
            "dm": {"serialname": "sf_dm", "kend": self.config.npz, "shield": True},
            "tot_e_initial": {"serialname": "sf_e1", "shield": True},
            "no_fall": {"serialname": "sf_nf", "shield": True},
        }

        self.stencil_factory = stencil_factory
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

        compute_func = StartFall(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateEndFall(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "ef_qv", "shield": True},
            "qliquid": {"serialname": "ef_ql", "shield": True},
            "qrain": {"serialname": "ef_qr", "shield": True},
            "qice": {"serialname": "ef_qi", "shield": True},
            "qsnow": {"serialname": "ef_qs", "shield": True},
            "qgraupel": {"serialname": "ef_qg", "shield": True},
            "ua": {"serialname": "ef_ua", "shield": True},
            "va": {"serialname": "ef_va", "shield": True},
            "wa": {"serialname": "ef_wa", "shield": True},
            "temperature": {"serialname": "ef_pt", "shield": True},
            "delp": {"serialname": "ef_dp", "shield": True},
            "delz": {"serialname": "ef_dz", "shield": True},
            "flux": {"serialname": "ef_pfi", "shield": True},
            "dm": {"serialname": "ef_dm", "shield": True},
            "v_terminal": {"serialname": "ef_vt", "shield": True},
            "column_energy_change": {"serialname": "ef_dte", "shield": True},
            "tmp_energy1": {"serialname": "ef_ie", "shield": True},
            "tmp_energy2": {"serialname": "ef_fe", "shield": True},
            "no_fall": {"serialname": "ef_nf", "shield": True},
        }

        self.out_vars = {
            "ua": {"serialname": "ef_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "ef_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "ef_wa", "kend": self.config.npz, "shield": True},
            "temperature": {
                "serialname": "ef_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "tmp_energy1": {
                "serialname": "ef_ie",
                "shield": True,
            },
            "column_energy_change": {"serialname": "ef_dte", "shield": True},
        }

        self.ignore_near_zero_errors = {"ef_ie": True}
        self.stencil_factory = stencil_factory
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

        compute_func = EndFall(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
