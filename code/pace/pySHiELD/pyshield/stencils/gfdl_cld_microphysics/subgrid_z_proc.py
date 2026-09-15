import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log
from ndsl.dsl.typing import Bool, FloatField, FloatFieldIJ
from ndsl.stencils.arithmetic_functions import dim
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig


@gtfunction
def perform_instant_processes(
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
):
    """
    Instant processes (include deposition, evaporation, and sublimation)
    Fortran name is pinst
    """
    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        do_mp_table_emulation,
        li00,
        lv00,
        t_min,
        t_sub,
    )

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
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(tin, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
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


@gtfunction
def cloud_condensation_evaporation(
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
    condensation,
    reevaporation,
):
    """
    cloud water condensation and evaporation, Hong and Lim (2006)
    pcond_pevap in Fortran
    """

    from __externals__ import (
        do_cond_timescale,
        do_evap_timescale,
        do_mp_table_emulation,
        rh_fac_cond,
        rh_fac_evap,
        rhc_cevap,
        tau_l2v,
        tau_v2l,
        timestep,
        use_rhc_cevap,
    )

    # TODO: set these at compiletime
    fac_l2v = 1.0 - exp(-timestep / tau_l2v)
    fac_v2l = 1.0 - exp(-timestep / tau_v2l)

    if do_mp_table_emulation:
        qsw, dqdt = physfun.wqs(temperature, density)
    else:
        qsw, dqdt = physfun.sat_spec_hum_water(temperature, density)
    qpz = qvapor + qliquid + qice
    rh_tem = qpz / qsw
    dq = qsw - qvapor

    if dq > 0.0:
        fac = min(1.0, fac_l2v * (rh_fac_evap * dq / qsw)) if do_evap_timescale else 1.0
        sink = min(qliquid, fac * dq / (1 + tcp3 * dqdt))
        if (use_rhc_cevap) and (rh_tem >= rhc_cevap):
            sink = 0.0
        reevaporation += sink * delp
    elif do_cond_timescale:
        fac = (
            min(1.0, fac_v2l * (rh_fac_cond * (-dq) / qsw))
            if do_evap_timescale
            else 1.0
        )
        sink = -min(qvapor, fac * (-dq) / (1.0 + tcp3 * dqdt))
        condensation -= sink * delp
    else:
        sink = -min(qvapor, -dq / (1.0 + tcp3 * dqdt))
        condensation -= sink * delp

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
        -sink,
        0.0,
        0.0,
        0.0,
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
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
        condensation,
        reevaporation,
    )


@gtfunction
def complete_freeze(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Enforces complete freezing below t_wfr
    Fortran name is pcomp
    """
    from __externals__ import t_wfr

    tc = t_wfr - temperature
    if (tc > 0.0) and (qliquid > mpcons.QCMIN):
        sink = qliquid * tc / mpcons.DT_FR
        sink = min(qliquid, min(sink, tc / icpk))
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
            0.0,
            -sink,
            0.0,
            sink,
            0.0,
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
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


@gtfunction
def wegener_bergeron_findeisen(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Wegener Bergeron Findeisen process, Storelvmo and Tan (2015)
    Fortran name is pwbf
    """

    from __externals__ import do_mp_table_emulation, qi0_crt, tau_wbf, timestep

    tc = mpcons.TICE0 - temperature
    if do_mp_table_emulation:
        qsw, dqdt = physfun.wqs(temperature, density)
        qsi, dqdt = physfun.iqs(temperature, density)
    else:
        qsw, dqdt = physfun.sat_spec_hum_water(temperature, density)
        qsi, dqdt = physfun.sat_spec_hum_water_ice(temperature, density)

    if (
        (tc > 0.0)
        and (qliquid > mpcons.QCMIN)
        and (qice > mpcons.QCMIN)
        and (qvapor > qsi)
        and (qvapor < qsw)
    ):
        # TODO: This can be moved to compile-time
        fac_wbf = 1.0 - exp(-timestep / tau_wbf)

        sink = min(fac_wbf * qliquid, tc / icpk)
        qim = qi0_crt / density
        tmp = min(sink, dim(qim, qice))

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
            0.0,
            -sink,
            0.0,
            tmp,
            sink - tmp,
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
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


@gtfunction
def freeze_bigg(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    cloud_condensation_nuclei,
    temperature,
    density,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Bigg freezing mechanism, Bigg (1953)
    Fortran name is pbigg
    """
    from __externals__ import do_psd_water_num, muw, pcaw, pcbw, timestep

    tc = mpcons.TICE0 - temperature
    if (tc > 0.0) and (qliquid > mpcons.QCMIN):
        if do_psd_water_num:
            cloud_condensation_nuclei = physfun.calc_particle_concentration(
                qliquid, density, pcaw, pcbw, muw
            )
            cloud_condensation_nuclei = cloud_condensation_nuclei / density

        sink = (
            100.0
            / (mpcons.RHO_W * cloud_condensation_nuclei)
            * timestep
            * (exp(0.66 * tc) - 1.0)
            * qliquid**2.0
        )
        sink = min(qliquid, min(sink, tc / icpk))

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
            0.0,
            -sink,
            0.0,
            sink,
            0.0,
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
        cloud_condensation_nuclei,
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


@gtfunction
def deposit_and_sublimate_ice(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    cloud_ice_nuclei,
    temperature,
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
):
    """
    Cloud ice deposition and sublimation, Hong et al. (2004)
    Fortran name is pidep_pisub
    """

    from __externals__ import (
        do_mp_table_emulation,
        do_psd_ice_num,
        igflag,
        inflag,
        is_fac,
        mui,
        pcai,
        pcbi,
        prog_ccn,
        qi_lim,
        t_sub,
        timestep,
    )

    if temperature < mpcons.TICE0:
        pidep = 0.0
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(temperature, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(temperature, density)
        dq = qvapor - qsi
        tmp = dq / (1.0 + tcpk * dqdt)

        if qice > mpcons.QCMIN:
            if not prog_ccn:
                if inflag == 1:
                    cloud_ice_nuclei = 5.38e7 * exp(0.75 * log(qice * density))
                elif inflag == 2:
                    cloud_ice_nuclei = (
                        exp(-2.80 + 0.262 * (mpcons.TICE0 - temperature)) * 1000.0
                    )
                elif inflag == 3:
                    cloud_ice_nuclei = (
                        exp(-0.639 + 12.96 * (qvapor / qsi - 1.0)) * 1000.0
                    )
                elif inflag == 4:
                    cloud_ice_nuclei = (
                        5.0e-3 * exp(0.304 * (mpcons.TICE0 - temperature)) * 1000.0
                    )
                else:  # inflag == 5:
                    cloud_ice_nuclei = (
                        1.0e-5 * exp(0.5 * (mpcons.TICE0 - temperature)) * 1000.0
                    )
            if do_psd_ice_num:
                cloud_ice_nuclei = physfun.calc_particle_concentration(
                    qice, density, pcai, pcbi, mui
                )
                cloud_ice_nuclei = cloud_ice_nuclei / density

            pidep = (
                timestep
                * dq
                * 4.0
                * 11.9
                * exp(0.5 * log(qice * density * cloud_ice_nuclei))
                / (
                    qsi
                    * density
                    * (tcpk * cvm) ** 2
                    / (mpcons.TCOND * constants.RVGAS * temperature**2)
                    + 1.0 / mpcons.VDIFU
                )
            )
        if dq > 0:
            tc = mpcons.TICE0 - temperature
            qi_gen = 4.92e-11 * exp(1.33 * log(1.0e3 * exp(0.1 * tc)))
            if igflag == 1:
                qi_crt = qi_gen / density
            elif igflag == 2:
                qi_crt = qi_gen * min(qi_lim, 0.1 * tc) / density
            elif igflag == 3:
                qi_crt = 1.82e-6 * min(qi_lim, 0.1 * tc) / density
            else:  # igflag == 4:
                qi_crt = max(qi_gen, 1.82e-6) * min(qi_lim, 0.1 * tc) / density
            sink = min(tmp, min(max(qi_crt - qice, pidep), tc / tcpk))
            dep += sink * delp
        else:
            pidep = pidep * min(1, dim(temperature, t_sub) * is_fac)
            sink = max(pidep, max(tmp, -qice))
            sub -= sink * delp

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
    return (
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        cloud_ice_nuclei,
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
        dep,
        sub,
    )


@gtfunction
def deposit_and_sublimate_snow(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    delp,
    density,
    density_factor,
    cvm,
    te,
    dep,
    sub,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Snow deposition and sublimation, Lin et al. (1983)
    Fortran name is psdep_pssub
    """
    from __externals__ import (
        blins,
        cssub_1,
        cssub_2,
        cssub_3,
        cssub_4,
        cssub_5,
        do_mp_table_emulation,
        mus,
        ss_fac,
        t_sub,
        timestep,
    )

    if qsnow > mpcons.QCMIN:
        tin = temperature
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(tin, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
        qden = qsnow * density
        t2 = temperature * temperature
        dq = qsi - qvapor
        pssub = physfun.sublimation_function(
            t2,
            dq,
            qden,
            qsi,
            density,
            density_factor,
            tcpk,
            cvm,
            cssub_1,
            cssub_2,
            cssub_3,
            cssub_4,
            cssub_5,
            blins,
            mus,
        )
        pssub *= timestep
        dq = dq / (1 + tcpk * dqdt)

        if pssub > 0:
            sink = min(pssub * min(1.0, dim(temperature, t_sub) * ss_fac), qsnow)
            sub += sink * delp
        else:
            sink = 0.0
            if temperature <= mpcons.TICE0:
                sink = max(dq, (temperature - mpcons.TICE0) / tcpk)
                sink = max(pssub, sink)
            dep -= sink * delp

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
            0.0,
            0.0,
            -sink,
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
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
        dep,
        sub,
    )


@gtfunction
def deposit_and_sublimate_graupel(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    delp,
    density,
    density_factor,
    cvm,
    te,
    dep,
    sub,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Graupel deposition and sublimation, Lin et al. (1983)
    Fortran name is pgdep_pgsub
    """
    from __externals__ import (
        bling,
        cgsub_1,
        cgsub_2,
        cgsub_3,
        cgsub_4,
        cgsub_5,
        do_mp_table_emulation,
        gs_fac,
        mug,
        t_sub,
        timestep,
    )

    if qgraupel > mpcons.QCMIN:
        tin = temperature
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(tin, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
        qden = qgraupel * density
        t2 = temperature * temperature
        dq = qsi - qvapor
        pgsub = physfun.sublimation_function(
            t2,
            dq,
            qden,
            qsi,
            density,
            density_factor,
            tcpk,
            cvm,
            cgsub_1,
            cgsub_2,
            cgsub_3,
            cgsub_4,
            cgsub_5,
            bling,
            mug,
        )
        pgsub *= timestep
        dq = dq / (1 + tcpk * dqdt)

        if pgsub > 0:
            sink = min(pgsub * min(1.0, dim(temperature, t_sub) * gs_fac), qgraupel)
            sub += sink * delp
        else:
            sink = 0.0
            if temperature <= mpcons.TICE0:
                sink = max(dq, (temperature - mpcons.TICE0) / tcpk)
                sink = max(pgsub, sink)
            dep -= sink * delp

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
            0.0,
            0.0,
            0.0,
            -sink,
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
    cond: FloatFieldIJ,
    dep: FloatFieldIJ,
    reevap: FloatFieldIJ,
    sub: FloatFieldIJ,
    rh_adj: FloatFieldIJ,
    last_step: Bool,
):
    """"""
    from __externals__ import delay_cond_evap, do_warm_rain_mp, do_wbf, nconds

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
                    n += 1

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
                ) = complete_freeze(
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temperature,
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
                        temperature,
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
                        temperature,
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
                    temperature,
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
                    temperature,
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
                    cloud_ice_nuclei,
                    temperature,
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
                    temperature,
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
                    sub,
                ) = deposit_and_sublimate_snow(
                    qvapor,
                    qliquid,
                    qrain,
                    qice,
                    qsnow,
                    qgraupel,
                    temperature,
                    delp,
                    density,
                    density_factor,
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
                    temperature,
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
                    temperature,
                    delp,
                    density,
                    density_factor,
                    cvm,
                    te,
                    dep,
                    sub,
                    lcpk,
                    icpk,
                    tcpk,
                    tcp3,
                )


class VerticalSubgridProcesses:
    """
    subgrid_z_proc in Fortran
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: GFDLCloudMPConfig,
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
        temperature: FloatField,
        density: FloatField,
        density_factor: FloatField,
        delp: FloatField,
        cloud_condensation_nuclei: FloatField,
        cloud_ice_nuclei: FloatField,
        cond: FloatFieldIJ,
        dep: FloatFieldIJ,
        reevap: FloatFieldIJ,
        sub: FloatFieldIJ,
        rh_adj: FloatFieldIJ,
        last_step: Bool,
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
            cond,
            dep,
            reevap,
            sub,
            rh_adj,
            last_step,
        )
