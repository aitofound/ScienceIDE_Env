import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, interval, max, min
from ndsl.dsl.typing import FloatField, IntField
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.humidity_tables import (
    HumiditySaturationTables,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


def init_tables(
    temp: FloatField,
    table0: FloatField,
    table2: FloatField,
):
    with computation(FORWARD), interval(...):
        it = physfun.temperature_index(temp)
        table0 = physfun.table0(it)
        table2 = physfun.table2(it)


def calc_table_values(
    temp: FloatField,
    den: FloatField,
    iqs: FloatField,
    wqs: FloatField,
    didt: FloatField,
    dwdt: FloatField,
    ap1: FloatField,
    it1: IntField,
    it2: IntField,
):
    from __externals__ import do_mp_table_emulation

    with computation(FORWARD), interval(...):
        if do_mp_table_emulation:
            wqs, dwdt = physfun.wqs(temp, den)
            iqs, didt = physfun.iqs(temp, den)
        else:
            wqs, dwdt = physfun.sat_spec_hum_water(temp, den)
            iqs, didt = physfun.sat_spec_hum_water_ice(temp, den)

        ap1 = 10.0 * max(temp - (mpcons.TICE0 - 160.0), 0.0) + 1
        ap1 = min(ap1, 2621.0)
        it1 = ap1
        it2 = ap1 - 0.5


class CalcTables:
    def __init__(self, stencil_factory: StencilFactory, config):
        self._idx = stencil_factory.grid_indexing
        self.config = config
        self._init_tables = stencil_factory.from_origin_domain(
            func=init_tables,
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )
        self._calc_table_values = stencil_factory.from_origin_domain(
            func=calc_table_values,
            externals={
                "do_mp_table_emulation": config.do_mp_table_emulation,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

    def __call__(
        self,
        temp: FloatField,
        table0: FloatField,
        table2: FloatField,
        iqs: FloatField,
        wqs: FloatField,
        didt: FloatField,
        dwdt: FloatField,
        temp2: FloatField,
        den: FloatField,
        ap1: FloatField,
        it1: IntField,
        it2: IntField,
    ):
        self._init_tables(
            temp,
            table0,
            table2,
        )
        self._calc_table_values(temp2, den, iqs, wqs, didt, dwdt, ap1, it1, it2)


class LookupPython:
    def __init__(self, length):
        self.sat_tables = HumiditySaturationTables(length)

    def __call__(
        self,
        index: FloatField,
        table0: FloatField,
        table2: FloatField,
        iqs: FloatField,
        wqs: FloatField,
        didt: FloatField,
        dwdt: FloatField,
        temp: FloatField,
        den: FloatField,
    ):
        table0 = self.sat_tables.table0[list(index.astype(int) - 1)]
        table2 = self.sat_tables.table2[list(index.astype(int) - 1)]
        wqs, dwdt = self.sat_tables.sat_water(temp, den)
        iqs, didt = self.sat_tables.sat_ice_water(temp, den)
        return table0, table2, wqs, dwdt, iqs, didt


class TranslatePythonTables(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "index": {"serialname": "tc_index", "shield": True},
            "table0": {"serialname": "tc_t0", "shield": True},
            "table2": {"serialname": "tc_t2", "shield": True},
            "wqs": {"serialname": "tab_wq", "shield": True},
            "dwdt": {"serialname": "tab_dwq", "shield": True},
            "iqs": {"serialname": "tab_iq", "shield": True},
            "didt": {"serialname": "tab_diq", "shield": True},
            "temp": {"serialname": "tab_pt", "shield": True},
            "den": {"serialname": "tab_den", "shield": True},
        }

        self.out_vars = {
            "table0": {"serialname": "tc_t0", "kend": self.config.npz, "shield": True},
            "table2": {"serialname": "tc_t2", "kend": self.config.npz, "shield": True},
            "wqs": {"serialname": "tab_wq", "kend": self.config.npz, "shield": True},
            "dwdt": {"serialname": "tab_dwq", "kend": self.config.npz, "shield": True},
            "iqs": {"serialname": "tab_iq", "kend": self.config.npz, "shield": True},
            "didt": {"serialname": "tab_diq", "kend": self.config.npz, "shield": True},
        }
        self.max_error = 2.0e-14

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

        compute_func = LookupPython(2621)

        table0, table2, wqs, dwdt, iqs, didt = compute_func(**inputs)
        inputs["table0"] = table0
        inputs["table2"] = table2
        inputs["wqs"] = wqs
        inputs["iqs"] = iqs
        inputs["dwdt"] = dwdt
        inputs["didt"] = didt

        return self.slice_output(inputs)


class TranslateTableComputation(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "temp": {"serialname": "tc_temp", "shield": True},
            "table0": {"serialname": "tc_t0", "shield": True},
            "table2": {"serialname": "tc_t2", "shield": True},
            "wqs": {"serialname": "tab_wq", "shield": True},
            "dwdt": {"serialname": "tab_dwq", "shield": True},
            "iqs": {"serialname": "tab_iq", "shield": True},
            "didt": {"serialname": "tab_diq", "shield": True},
            "temp2": {"serialname": "tab_pt", "shield": True},
            "den": {"serialname": "tab_den", "shield": True},
            "ap1": {"serialname": "tc_ap1", "shield": True},
            "it1": {"serialname": "tc_it1", "shield": True},
            "it2": {"serialname": "tc_it2", "shield": True},
        }

        self.out_vars = {
            "table0": {"serialname": "tc_t0", "kend": self.config.npz, "shield": True},
            "table2": {"serialname": "tc_t2", "kend": self.config.npz, "shield": True},
            "wqs": {"serialname": "tab_wq", "kend": self.config.npz, "shield": True},
            "dwdt": {"serialname": "tab_dwq", "kend": self.config.npz, "shield": True},
            "iqs": {"serialname": "tab_iq", "kend": self.config.npz, "shield": True},
            "didt": {"serialname": "tab_diq", "kend": self.config.npz, "shield": True},
            "ap1": {"serialname": "tc_ap1", "shield": True},
            "it1": {"serialname": "tc_it1", "shield": True},
            "it2": {"serialname": "tc_it2", "shield": True},
        }

        self.max_error = 1e-13  # 10^-25 absolute errors at the top of the tables

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
        self.mpconfig.do_mp_table_emulation = True

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = CalcTables(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
