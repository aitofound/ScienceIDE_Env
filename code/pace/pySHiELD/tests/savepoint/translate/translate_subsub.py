import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from ndsl.stencils.arithmetic_functions import dim
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.subgrid_z_proc import (
    cloud_condensation_evaporation,
    perform_instant_processes,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


@gtfunction
def perform_instant_processes_test(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    delp,
    te,
    rh_adj,
    dep,
    reevap,
    sub,
    qsi,
    dqidt,
    cvm,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Instant processes (include deposition, evaporation, and sublimation)
    Fortran name is pinst
    """
    from __externals__ import c1_ice, c1_liq, c1_vap, li00, lv00, t_min, t_sub

    # Instant deposit all water vapor to cloud ice when temperature is super low
    if temperature < t_min:
        sink = dim(qvapor, mpcons.QCMIN)
        dep += sink * delp

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
            -sink,
            0.0,
            0.0,
            sink,
            0.0,
            0.0,
            te,
        )

    # Instant evaporation / sublimation of all clouds when rh < rh_adj
    qpz = qvapor + qliquid + qice
    tin = (te - lv00 * qpz + li00 * (qsnow + qgraupel)) / (
        1.0 + qpz * c1_vap + qrain * c1_liq + (qsnow + qgraupel) * c1_ice
    )

    if tin > (t_sub + 6.0):
        # qsi, dqidt = physfun.sat_spec_hum_water_ice(tin, density)
        rh = qpz / qsi

        if rh < rh_adj:
            sink = qliquid
            tmp = qice

            reevap += sink * delp
            sub += tmp * delp

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
                sink + tmp,
                -sink,
                0.0,
                -tmp,
                0,
                0,
                te,
            )
    return (
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
        dep,
        reevap,
        sub,
    )


def vertical_subgrid_processes(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temperature: FloatField,
    density: FloatField,
    density_factor: FloatField,
    delp: FloatField,
    cloud_condensation_nuclei: FloatField,
    cloud_ice_nuclei: FloatField,
    te: FloatField,
    cvm: FloatField,
    lcpk: FloatField,
    icpk: FloatField,
    tcpk: FloatField,
    tcp3: FloatField,
    cond: FloatFieldIJ,
    dep: FloatFieldIJ,
    reevap: FloatFieldIJ,
    sub: FloatFieldIJ,
    rh_adj: FloatFieldIJ,
    qsi: FloatField,
    dqidt: FloatField,
    qsw: FloatField,
    dqwdt: FloatField,
):
    """"""
    from __externals__ import do_warm_rain_mp, do_wbf  # noqa

    with computation(FORWARD):
        with interval(-1, None):
            cond = 0
            dep = 0
            reevap = 0
            sub = 0

    with computation(FORWARD):
        with interval(...):
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

            if not do_warm_rain_mp:
                (
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temperature,
                    cvm,
                    lcpk,
                    icpk,
                    tcpk,
                    tcp3,
                    dep,
                    reevap,
                    sub,
                ) = perform_instant_processes(
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temperature,
                    density,
                    delp,
                    te,
                    rh_adj,
                    dep,
                    reevap,
                    sub,
                )

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
                cond,
                reevap,
            ) = cloud_condensation_evaporation(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                delp,
                density,
                te,
                tcp3,
                cond,
                reevap,
            )

            # if (not do_warm_rain_mp):
            #     (
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         cvm,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     ) = complete_freeze(
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         cvm,
            #         te,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     )

            #     if (do_wbf):
            #         (
            #             qvapor,
            #             qliquid,
            #             qrain,
            #             qice,
            #             qsnow,
            #             qgraupel,
            #             temperature,
            #             cvm,
            #             lcpk,
            #             icpk,
            #             tcpk,
            #             tcp3,
            #         ) = wegener_bergeron_findeisen(
            #             qvapor,
            #             qliquid,
            #             qrain,
            #             qice,
            #             qsnow,
            #             qgraupel,
            #             temperature,
            #             density,
            #             cvm,
            #             te,
            #             lcpk,
            #             icpk,
            #             tcpk,
            #             tcp3,
            #         )

            #     (
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         cloud_condensation_nuclei,
            #         temperature,
            #         cvm,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     ) = freeze_bigg(
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         cloud_condensation_nuclei,
            #         temperature,
            #         density,
            #         cvm,
            #         te,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     )

            #     (
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         cloud_ice_nuclei,
            #         temperature,
            #         cvm,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #         dep,
            #         sub,
            #     ) = deposit_and_sublimate_ice(
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         cloud_ice_nuclei,
            #         temperature,
            #         delp,
            #         density,
            #         cvm,
            #         te,
            #         dep,
            #         sub,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     )

            #     (
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         cvm,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #         dep,
            #         sub,
            #     ) = deposit_and_sublimate_snow(
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         delp,
            #         density,
            #         density_factor,
            #         cvm,
            #         te,
            #         dep,
            #         sub,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     )

            #     (
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         cvm,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #         dep,
            #         sub,
            #     ) = deposit_and_sublimate_graupel(
            #         qvapor,
            #         qliquid,
            #         qrain,
            #         qice,
            #         qsnow,
            #         qgraupel,
            #         temperature,
            #         delp,
            #         density,
            #         density_factor,
            #         cvm,
            #         te,
            #         dep,
            #         sub,
            #         lcpk,
            #         icpk,
            #         tcpk,
            #         tcp3,
            #     )


class SubSubgridProcesses:
    """
    subgrid_z_proc in Fortran
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        config,
        timestep: float,
    ):
        self._idx: GridIndexing = stencil_factory.grid_indexing

        if config.do_hail:
            mu_g = config.muh
            blin_g = config.blinh
        else:
            mu_g = config.mug
            blin_g = config.bling

        if config.inflag not in [1, 2, 3, 4, 5]:
            raise ValueError(
                f"Ice Nucleation Flag must be an integer from 1 to 5"
                f"not {config.inflag}"
            )

        if config.igflag not in [1, 2, 3, 4]:
            raise ValueError(
                f"Ice Generation Flag must be an int from 1 to 4, got {config.igflag}"
            )

        self._vertical_subgrid_processes = stencil_factory.from_origin_domain(
            func=vertical_subgrid_processes,
            externals={
                "timestep": timestep,
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "d1_ice": config.d1_ice,
                "d1_vap": config.d1_vap,
                "li00": config.li00,
                "li20": config.li20,
                "lv00": config.lv00,
                "t_wfr": config.t_wfr,
                "t_min": config.t_min,
                "t_sub": config.t_sub,
                "do_cond_timescale": config.do_cond_timescale,
                "do_evap_timescale": config.do_evap_timescale,
                "rh_fac_evap": config.rh_fac_evap,
                "rh_fac_cond": config.rh_fac_cond,
                "rhc_cevap": config.rhc_cevap,
                "tau_l2v": config.tau_l2v,
                "tau_v2l": config.tau_v2l,
                "use_rhc_cevap": config.use_rhc_cevap,
                "qi0_crt": config.qi0_crt,
                "tau_wbf": config.tau_wbf,
                "do_psd_water_num": config.do_psd_water_num,
                "muw": config.muw,
                "pcaw": config.pcaw,
                "pcbw": config.pcbw,
                "do_psd_ice_num": config.do_psd_ice_num,
                "igflag": config.igflag,
                "inflag": config.inflag,
                "is_fac": config.is_fac,
                "mui": config.mui,
                "pcai": config.pcai,
                "pcbi": config.pcbi,
                "prog_ccn": config.prog_ccn,
                "qi_lim": config.qi_lim,
                "blins": config.blins,
                "mus": config.mus,
                "cssub_1": config.cssub_1,
                "cssub_2": config.cssub_2,
                "cssub_3": config.cssub_3,
                "cssub_4": config.cssub_4,
                "cssub_5": config.cssub_5,
                "ss_fac": config.ss_fac,
                "cgsub_1": config.cgsub_1,
                "cgsub_2": config.cgsub_2,
                "cgsub_3": config.cgsub_3,
                "cgsub_4": config.cgsub_4,
                "cgsub_5": config.cgsub_5,
                "bling": config.bling,
                "mug": config.mug,
                "gs_fac": config.gs_fac,
                "do_warm_rain_mp": config.do_warm_rain_mp,
                "do_wbf": config.do_wbf,
                "do_mp_table_emulation": config.do_mp_table_emulation,
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
        density: FloatField,
        density_factor: FloatField,
        delp: FloatField,
        cloud_condensation_nuclei: FloatField,
        cloud_ice_nuclei: FloatField,
        te: FloatField,
        cond: FloatFieldIJ,
        dep: FloatFieldIJ,
        reevap: FloatFieldIJ,
        sub: FloatFieldIJ,
        rh_adj: FloatFieldIJ,
        cvm: FloatField,
        lcpk: FloatField,
        icpk: FloatField,
        tcpk: FloatField,
        tcp3: FloatField,
        qsi: FloatField,
        dqidt: FloatField,
        qsw: FloatField,
        dqwdt: FloatField,
    ):
        """
        Temperature sentive high vertical resolution processes
        Args:
            qvapor (inout):
            qliquid (inout):
            qrain (inout):
            qice (inout):
            qsnow (inout):
            qgraupel (inout):
            temperature (inout):
            density (in):
            density_factor (in):
            delp (in):
            cloud_condensation_nuclei (inout):
            cloud_ice_nuclei (inout):
            te (in):
            cond (out):
            dep (out):
            reevap (out):
            sub (out):
            rh_adj (in):
        """

        self._vertical_subgrid_processes(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            density_factor,
            delp,
            cloud_condensation_nuclei,
            cloud_ice_nuclei,
            te,
            cvm,
            lcpk,
            icpk,
            tcpk,
            tcp3,
            cond,
            dep,
            reevap,
            sub,
            rh_adj,
            qsi,
            dqidt,
            qsw,
            dqwdt,
        )


class TranslateSubgridZSubs(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "szs_qv", "shield": True},
            "qliquid": {"serialname": "szs_ql", "shield": True},
            "qrain": {"serialname": "szs_qr", "shield": True},
            "qice": {"serialname": "szs_qi", "shield": True},
            "qsnow": {"serialname": "szs_qs", "shield": True},
            "qgraupel": {"serialname": "szs_qg", "shield": True},
            "temperature": {"serialname": "szs_pt", "shield": True},
            "density": {"serialname": "szs_den", "shield": True},
            "density_factor": {"serialname": "szs_denfac", "shield": True},
            "delp": {"serialname": "szs_delp", "shield": True},
            "rh_adj": {"serialname": "szs_rh_adj", "shield": True},
            "cloud_condensation_nuclei": {"serialname": "szs_ccn", "shield": True},
            "cloud_ice_nuclei": {"serialname": "szs_cin", "shield": True},
            "te": {"serialname": "szs_te", "shield": True},
            "cond": {"serialname": "szs_cond", "shield": True},
            "dep": {"serialname": "szs_dep", "shield": True},
            "reevap": {"serialname": "szs_reevap", "shield": True},
            "sub": {"serialname": "szs_sub", "shield": True},
            "cvm": {"serialname": "szs_cvm", "shield": True},
            "lcpk": {"serialname": "szs_lcpk", "shield": True},
            "icpk": {"serialname": "szs_icpk", "shield": True},
            "tcpk": {"serialname": "szs_tcpk", "shield": True},
            "tcp3": {"serialname": "szs_tcp3", "shield": True},
            "qsi": {"serialname": "szs_qsi", "shield": True},
            "dqidt": {"serialname": "szs_dqidt", "shield": True},
            "qsw": {"serialname": "szs_qsw", "shield": True},
            "dqwdt": {"serialname": "szs_dqwdt", "shield": True},
        }

        self.in_vars["parameters"] = [
            "dt",
        ]

        self.out_vars = {
            "qvapor": {"serialname": "szs_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {
                "serialname": "szs_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {"serialname": "szs_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "szs_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "szs_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "szs_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "szs_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_condensation_nuclei": {
                "serialname": "szs_ccn",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_ice_nuclei": {
                "serialname": "szs_cin",
                "kend": self.config.npz,
                "shield": True,
            },
            "cond": {"serialname": "szs_cond", "kend": self.config.npz, "shield": True},
            "dep": {"serialname": "szs_dep", "kend": self.config.npz, "shield": True},
            "reevap": {
                "serialname": "szs_reevap",
                "kend": self.config.npz,
                "shield": True,
            },
            "sub": {"serialname": "szs_sub", "kend": self.config.npz, "shield": True},
            "cvm": {"serialname": "szs_cvm", "kend": self.config.npz, "shield": True},
            "lcpk": {"serialname": "szs_lcpk", "kend": self.config.npz, "shield": True},
            "icpk": {"serialname": "szs_icpk", "kend": self.config.npz, "shield": True},
            "tcpk": {"serialname": "szs_tcpk", "kend": self.config.npz, "shield": True},
            "tcp3": {"serialname": "szs_tcp3", "kend": self.config.npz, "shield": True},
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

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = SubSubgridProcesses(
            self.stencil_factory,
            self.mpconfig,
            timestep=inputs.pop("dt"),
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
