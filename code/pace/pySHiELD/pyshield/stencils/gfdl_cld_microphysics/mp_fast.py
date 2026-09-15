import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval
from ndsl.dsl.typing import Bool, FloatField, FloatFieldIJ
from ndsl.stencils.arithmetic_functions import dim
from pyshield.stencils.gfdl_cld_microphysics._config import FastMPConfig
from pyshield.stencils.gfdl_cld_microphysics.ice_cloud import (
    freeze_cloud_water,
    melt_cloud_ice,
)
from pyshield.stencils.gfdl_cld_microphysics.subgrid_z_proc import (
    cloud_condensation_evaporation,
    complete_freeze,
    deposit_and_sublimate_graupel,
    deposit_and_sublimate_ice,
    deposit_and_sublimate_snow,
    freeze_bigg,
    wegener_bergeron_findeisen,
)


@gtfunction
def freeze_rain_to_graupel_simple(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temp,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Fortran name is pgfr_simp
    """

    from __externals__ import fac_r2g, tice

    tc = temp - tice
    if (tc < 0.0) and (qrain > mpcons.QCMIN):
        sink = (-tc * 0.025) ** 2 * qrain
        sink = min(qrain, sink, -fac_r2g * tc / icpk)

        (
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            cvm,
            temp,
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
            0.0,
            0.0,
            -sink,
            0.0,
            0.0,
            sink,
            te,
        )

    return (
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        cvm,
        temp,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


@gtfunction
def melt_snow_simple(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temp,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Fortran name is psmlt_simp
    """

    from __externals__ import fac_smlt, qs_mlt, tice

    tc = temp - tice
    if (tc > 0.0) and (qsnow > mpcons.QCMIN):
        sink = (tc * 0.1) ** 2 * qsnow
        sink = min(qsnow, sink, fac_smlt * tc / icpk)
        tmp = min(sink, dim(qs_mlt, qliquid))

        (
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            cvm,
            temp,
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
            0.0,
            tmp,
            sink - tmp,
            0.0,
            sink,
            0.0,
            te,
        )

    return (
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        cvm,
        temp,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


@gtfunction
def autoconvert_water_to_rain_simple(
    qliquid,
    qrain,
    temp,
):
    """
    Cloud water to rain autoconversion, simple version
    Fortran name is praut_simp
    """

    from __externals__ import fac_l2r, ql0_max, t_wfr

    tc = temp - t_wfr
    if (tc > 0) and (qliquid > ql0_max):
        sink = fac_l2r * (qliquid - ql0_max)
        qliquid -= sink
        qrain += sink

    return qliquid, qrain


@gtfunction
def autoconvert_ice_to_snow_simple(qice, qsnow, temp, density):
    """
    Cloud ice to snow autoconversion, simple version
    Fortran name is psaut_simp
    """
    from __externals__ import fac_i2s, qi0_max, tice

    tc = temp - tice
    qim = qi0_max / density

    if (tc < 0) and (qice > qim):
        sink = fac_i2s * (qice - qim)
        qice -= sink
        qsnow += sink
    return qice, qsnow


def fast_microphysics(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temp: FloatField,
    delp: FloatField,
    density: FloatField,
    denfac: FloatField,
    cloud_condensation_nuclei: FloatField,
    cloud_ice_nuclei: FloatField,
    condensation: FloatFieldIJ,
    deposition: FloatFieldIJ,
    evaporation: FloatFieldIJ,
    sublimation: FloatFieldIJ,
    last_step: Bool,
):
    from __externals__ import (
        convt,
        delay_cond_evap,
        do_warm_rain_mp,
        do_wbf,
        fast_dep_sub,
        fast_fr_mlt,
        nconds,
    )

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
            qvapor, qliquid, qrain, qice, qsnow, qgraupel, temp
        )

        if (not do_warm_rain_mp) and fast_fr_mlt:
            cond = 0.0
            dep = 0.0
            reevap = 0.0
            sub = 0.0

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = melt_cloud_ice(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = complete_freeze(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            cond_evap = last_step if delay_cond_evap else True

            if cond_evap:
                n = 1
                while n <= nconds:
                    (
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        temp,
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
                        temp,
                        delp,
                        density,
                        te,
                        tcp3,
                    )
                    n += 1

        condensation += cond * convt
        evaporation += reevap * convt

        if (not do_warm_rain_mp) and fast_fr_mlt:
            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = freeze_cloud_water(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                density,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )
            if do_wbf:
                (
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temp,
                    cvm,
                    lcpk,
                    icpk,
                    tcpk,
                    tcp3,
                ) = wegener_bergeron_findeisen(
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temp,
                    density,
                    cvm,
                    te,
                    lcpk,
                    icpk,
                    tcpk,
                    tcp3,
                )
            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cloud_condensation_nuclei,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = freeze_bigg(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cloud_condensation_nuclei,
                temp,
                density,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cvm,
                temp,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = freeze_rain_to_graupel_simple(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cvm,
                temp,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            ) = melt_snow_simple(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                te,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

        qliquid, qrain = autoconvert_water_to_rain_simple(qliquid, qrain, temp)

        if (not do_warm_rain_mp) and fast_dep_sub:
            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cloud_ice_nuclei,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
                dep,
                sub,
            ) = deposit_and_sublimate_ice(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                cloud_ice_nuclei,
                temp,
                delp,
                density,
                cvm,
                te,
                dep,
                sub,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            deposition += dep * convt
            sublimation += sub * convt

            qice, qsnow = autoconvert_ice_to_snow_simple(qice, qsnow, temp, density)

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
                dep,
                sub,
            ) = deposit_and_sublimate_snow(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                delp,
                density,
                denfac,
                cvm,
                te,
                dep,
                sub,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )

            (
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                cvm,
                lcpk,
                icpk,
                tcpk,
                tcp3,
                dep,
                sub,
            ) = deposit_and_sublimate_graupel(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temp,
                delp,
                density,
                denfac,
                cvm,
                te,
                dep,
                sub,
                lcpk,
                icpk,
                tcpk,
                tcp3,
            )


class FastMicrophysics:
    """
    Fast microphysics loop, mp_fast in Fortran
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: FastMPConfig,
        timestep: float,
        convert_mm_day: float,
    ):

        self._idx: GridIndexing = stencil_factory.grid_indexing
        fac_r2g = 1.0 - exp(-timestep / config.tau_r2g)
        fac_smlt = 1.0 - exp(-timestep / config.tau_smlt)
        fac_l2r = 1.0 - exp(-timestep / config.tau_l2r)
        fac_i2s = 1.0 - exp(-timestep / config.tau_i2s)

        self._fast_microphysics = stencil_factory.from_origin_domain(
            func=fast_microphysics,
            externals={
                "do_warm_rain_mp": config.do_warm_rain_mp,
                "do_wbf": config.do_wbf,
                "c1_vap": config.c1_vap,
                "c1_liq": config.c1_liq,
                "c1_ice": config.c1_ice,
                "lv00": config.lv00,
                "li00": config.li00,
                "li20": config.li20,
                "d1_vap": config.d1_vap,
                "d1_ice": config.d1_ice,
                "tice": mpcons.TICE0,
                "t_wfr": config.t_wfr,
                "convt": convert_mm_day,
                "ql_mlt": config.ql_mlt,
                "qs_mlt": config.qs_mlt,
                "tau_imlt": config.tau_imlt,
                "tice_mlt": config.tice_mlt,
                "timestep": timestep,
                "do_cond_timescale": config.do_cond_timescale,
                "rh_fac_evap": config.rh_fac_evap,
                "rh_fac_cond": config.rh_fac_cond,
                "rhc_cevap": config.rhc_cevap,
                "tau_l2v": config.tau_l2v,
                "tau_v2l": config.tau_v2l,
                "fac_r2g": fac_r2g,
                "fac_smlt": fac_smlt,
                "fac_l2r": fac_l2r,
                "use_rhc_cevap": config.use_rhc_cevap,
                "qi0_crt": config.qi0_crt,
                "qi0_max": config.qi0_max,
                "ql0_max": config.ql0_max,
                "tau_wbf": config.tau_wbf,
                "do_psd_water_num": config.do_psd_water_num,
                "do_psd_ice_num": config.do_psd_ice_num,
                "muw": config.muw,
                "mui": config.mui,
                "pcaw": config.pcaw,
                "pcbw": config.pcbw,
                "pcai": config.pcai,
                "pcbi": config.pcbi,
                "prog_ccn": config.prog_ccn,
                "inflag": config.inflag,
                "igflag": config.igflag,
                "qi_lim": config.qi_lim,
                "t_sub": config.t_sub,
                "is_fac": config.is_fac,
                "fac_i2s": fac_i2s,
                "fast_fr_mlt": config.fast_fr_mlt,
                "fast_dep_sub": config.fast_dep_sub,
                "delay_cond_evap": config.delay_cond_evap,
                "nconds": config.nconds,
                "do_evap_timescale": config.do_evap_timescale,
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
        temp: FloatField,
        delp: FloatField,
        density: FloatField,
        denfac: FloatField,
        cloud_condensation_nuclei: FloatField,
        cloud_ice_nuclei: FloatField,
        condensation: FloatFieldIJ,
        deposition: FloatFieldIJ,
        evaporation: FloatFieldIJ,
        sublimation: FloatFieldIJ,
        last_step: Bool,
    ):
        """
        Fast microphysics loop
        Args:
            qvapor (inout):
            qliquid (inout):
            qrain (inout):
            qice (inout):
            qsnow (inout):
            qgraupel (inout):
            temp (inout):
            delp (in):
            density (in):
            cloud_condensation_nuclei (inout):
            cloud_ice_nuclei (inout):
            condensation (inout):
            deposition (inout):
            evaporation (inout):
            sublimation (inout):
        """

        self._fast_microphysics(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temp,
            delp,
            density,
            denfac,
            cloud_condensation_nuclei,
            cloud_ice_nuclei,
            condensation,
            deposition,
            evaporation,
            sublimation,
            last_step,
        )
