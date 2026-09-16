import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import FORWARD, PARALLEL, computation, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from ndsl.stencils.arithmetic_functions import dim
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig


@gtfunction
def melt_cloud_ice(
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
    Cloud ice melting to form cloud water and rain
    Fortran name is pimlt
    """

    from __externals__ import ql_mlt, tau_imlt, tice_mlt, timestep

    # Todo: this can be done at compile time
    fac_imlt = 1.0 - exp(-timestep / tau_imlt)

    tc = temperature - tice_mlt
    if (tc > 0.0) and (qice > mpcons.QCMIN):
        sink = fac_imlt * tc / icpk
        sink = min(qice, sink)
        tmp = min(sink, dim(ql_mlt, qliquid))

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
            tmp,
            sink - tmp,
            -sink,
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
    )


@gtfunction
def freeze_cloud_water(
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
    Cloud water freezing to form cloud ice and snow, Lin et al. (1983)
    Fortran name is pifr
    """

    from __externals__ import qi0_crt, t_wfr

    tc = t_wfr - temperature
    if (tc > 0.0) and (qliquid > mpcons.QCMIN):
        sink = qliquid * tc / mpcons.DT_FR
        sink = min(qliquid, min(sink, tc / icpk))
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
def melt_snow(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    density_factor,
    vterminal_w,
    vterminal_r,
    vterminal_s,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Snow melting (includes snow accretion with cloud water and rain)
    to form cloud water and rain, Lin et al. (1983)
    Fortran name is psmlt
    """

    from __externals__ import (
        acc0,
        acc1,
        acc2,
        acc3,
        acc12,
        acc13,
        acco_0_0,
        acco_0_1,
        acco_0_6,
        acco_1_0,
        acco_1_1,
        acco_1_6,
        acco_2_0,
        acco_2_1,
        acco_2_6,
        blins,
        cracs,
        csacr,
        csacw,
        csmlt_1,
        csmlt_2,
        csmlt_3,
        csmlt_4,
        do_mp_table_emulation,
        do_new_acc_water,
        mus,
        qs_mlt,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc >= 0) and (qsnow > mpcons.QCMIN):
        psacw = 0.0
        qden = qsnow * density
        if qliquid > mpcons.QCMIN:
            if do_new_acc_water:
                psacw = physfun.accretion_3d(
                    vterminal_s,
                    vterminal_w,
                    qliquid,
                    qsnow,
                    density,
                    csacw,
                    acco_0_6,
                    acco_1_6,
                    acco_2_6,
                    acc12,
                    acc13,
                )
            else:
                factor = physfun.accretion_2d(qden, density_factor, csacw, blins, mus)
                psacw = factor / (1.0 + timestep * factor) * qliquid

        psacr = 0.0
        pracs = 0.0
        if qrain > mpcons.QCMIN:
            psacr = physfun.accretion_3d(
                vterminal_s,
                vterminal_r,
                qrain,
                qsnow,
                density,
                csacr,
                acco_0_1,
                acco_1_1,
                acco_2_1,
                acc2,
                acc3,
            )
            psacr = min(qrain / timestep, psacr)
            pracs = physfun.accretion_3d(
                vterminal_r,
                vterminal_s,
                qsnow,
                qrain,
                density,
                cracs,
                acco_0_0,
                acco_1_0,
                acco_2_0,
                acc0,
                acc1,
            )

        tin = temperature
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(tin, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
        dq = qsi - qvapor

        sink = max(
            0.0,
            physfun.melting_function(
                tc,
                dq,
                qden,
                psacw,
                psacr,
                density,
                density_factor,
                lcpk,
                icpk,
                cvm,
                blins,
                mus,
                csmlt_1,
                csmlt_2,
                csmlt_3,
                csmlt_4,
            ),
        )
        sink = min(qsnow, min((sink + pracs) * timestep, tc / icpk))
        tmp = min(sink, dim(qs_mlt, qliquid))

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
            tmp,
            sink - tmp,
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
    )


@gtfunction
def melt_graupel(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    density_factor,
    vterminal_w,
    vterminal_r,
    vterminal_g,
    cvm,
    te,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    graupel melting (includes graupel accretion with cloud water and rain)
    to form rain, Lin et al. (1983)
    Fortran name is pgmlt
    """

    from __externals__ import (
        acc4,
        acc5,
        acc16,
        acc17,
        acco_0_2,
        acco_0_8,
        acco_1_2,
        acco_1_8,
        acco_2_2,
        acco_2_8,
        bling,
        cgacr,
        cgacw,
        cgmlt_1,
        cgmlt_2,
        cgmlt_3,
        cgmlt_4,
        do_mp_table_emulation,
        do_new_acc_water,
        mug,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc >= 0) and (qgraupel > mpcons.QCMIN):
        pgacw = 0.0
        qden = qgraupel * density
        if qliquid > mpcons.QCMIN:
            if do_new_acc_water:
                pgacw = physfun.accretion_3d(
                    vterminal_g,
                    vterminal_w,
                    qliquid,
                    qgraupel,
                    density,
                    cgacw,
                    acco_0_8,
                    acco_1_8,
                    acco_2_8,
                    acc16,
                    acc17,
                )
            else:
                factor = physfun.accretion_2d(qden, density_factor, cgacw, bling, mug)
                pgacw = factor / (1.0 + timestep * factor) * qliquid

        pgacr = 0.0
        if qrain > mpcons.QCMIN:
            pgacr = min(
                qrain / timestep,
                physfun.accretion_3d(
                    vterminal_g,
                    vterminal_r,
                    qrain,
                    qgraupel,
                    density,
                    cgacr,
                    acco_0_2,
                    acco_1_2,
                    acco_2_2,
                    acc4,
                    acc5,
                ),
            )

        tin = temperature
        if do_mp_table_emulation:
            qsi, dqdt = physfun.iqs(tin, density)
        else:
            qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
        dq = qsi - qvapor

        sink = max(
            0.0,
            physfun.melting_function(
                tc,
                dq,
                qden,
                pgacw,
                pgacr,
                density,
                density_factor,
                lcpk,
                icpk,
                cvm,
                bling,
                mug,
                cgmlt_1,
                cgmlt_2,
                cgmlt_3,
                cgmlt_4,
            ),
        )
        sink = min(qgraupel, min(sink * timestep, tc / icpk))

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
            0.0,
            sink,
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
    )


@gtfunction
def accrete_snow_with_ice(
    qice,
    qsnow,
    temperature,
    density,
    density_factor,
    vterminal_i,
    vterminal_s,
):
    """
    Snow accretion with cloud ice, Lin et al. (1983)
    Fortran name is psaci
    """

    from __externals__ import (
        acc14,
        acc15,
        acco_0_7,
        acco_1_7,
        acco_2_7,
        blins,
        csaci,
        do_new_acc_ice,
        fi2s_fac,
        mus,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qice > mpcons.QCMIN):
        sink = 0.0
        qden = qsnow * density
        if qsnow > mpcons.QCMIN:
            if do_new_acc_ice:
                sink = timestep * physfun.accretion_3d(
                    vterminal_s,
                    vterminal_i,
                    qice,
                    qsnow,
                    density,
                    csaci,
                    acco_0_7,
                    acco_1_7,
                    acco_2_7,
                    acc14,
                    acc15,
                )
            else:
                factor = timestep * physfun.accretion_2d(
                    qden, density_factor, csaci, blins, mus
                )
                sink = factor / (1.0 + factor) * qice

        sink = min(fi2s_fac * qice, sink)

        qice -= sink
        qsnow += sink

    return qice, qsnow


@gtfunction
def autoconvert_ice_to_snow(
    qice,
    qsnow,
    temperature,
    density,
    di,
):
    """
    Cloud ice to snow autoconversion, Lin et al. (1983)
    Fortran name is psaut
    """
    from __externals__ import fi2s_fac, qi0_crt, tau_i2s, timestep

    fac_i2s = 1.0 - exp(-timestep / tau_i2s)
    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qice > mpcons.QCMIN):
        sink = 0.0
        tmp = fac_i2s * exp(0.025 * tc)
        di = max(di, mpcons.QCMIN)
        q_plus = qice + di
        qim = qi0_crt / density

        if q_plus > (qim + mpcons.QCMIN):
            if qim > (qice - di):
                dq = (0.25 * (q_plus - qim) ** 2) / di
            else:
                dq = qice - qim
            sink = tmp * dq

        sink = min(fi2s_fac * qice, sink)

        qice -= sink
        qsnow += sink

    return qice, qsnow, di, temperature


@gtfunction
def accrete_graupel_with_ice(
    qice, qgraupel, temperature, density, density_factor, vtermainal_i, vterminal_g
):
    """
    Graupel accretion with cloud ice, Lin et al. (1983)
    Fortran name is pgaci
    """
    from __externals__ import (
        acc18,
        acc19,
        acco_0_9,
        acco_1_9,
        acco_2_9,
        bling,
        cgaci,
        do_new_acc_ice,
        fi2g_fac,
        mug,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qice > mpcons.QCMIN):
        sink = 0.0
        qden = qgraupel * density

        if qgraupel > mpcons.QCMIN:
            if do_new_acc_ice:
                sink = timestep * physfun.accretion_3d(
                    vterminal_g,
                    vtermainal_i,
                    qice,
                    qgraupel,
                    density,
                    cgaci,
                    acco_0_9,
                    acco_1_9,
                    acco_2_9,
                    acc18,
                    acc19,
                )
            else:
                factor = timestep * physfun.accretion_2d(
                    qden, density_factor, cgaci, bling, mug
                )
                sink = factor / (1.0 + factor) * qice

        sink = min(fi2g_fac * qice, sink)

        qice -= sink
        qgraupel += sink

    return qice, qgraupel


@gtfunction
def accrete_snow_with_rain_and_freeze_to_graupel(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    vterminal_r,
    vterminal_s,
    te,
    cvm,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Snow accretion with rain and rain freezing to form graupel, Lin et al. (1983)
    Fortran name is psacr_pgfr
    """
    from __externals__ import (
        acc2,
        acc3,
        acco_0_1,
        acco_1_1,
        acco_2_1,
        cgfr_1,
        cgfr_2,
        csacr,
        mur,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qrain > mpcons.QCMIN):
        psacr = 0.0
        if qsnow > mpcons.QCMIN:
            psacr = timestep * physfun.accretion_3d(
                vterminal_s,
                vterminal_r,
                qrain,
                qsnow,
                density,
                csacr,
                acco_0_1,
                acco_1_1,
                acco_2_1,
                acc2,
                acc3,
            )

        pgfr = (
            timestep
            * cgfr_1
            / density
            * (exp(-cgfr_2 * tc) - 1)
            * exp((6 + mur) / (mur + 3) * log(6 * qrain * density))
        )
        sink = psacr + pgfr
        factor = min(sink, min(qrain, -tc / icpk)) / max(sink, mpcons.QCMIN)
        psacr = factor * psacr
        pgfr = factor * pgfr

        sink = min(qrain, psacr + pgfr)

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
            0.0,
            -sink,
            0.0,
            psacr,
            pgfr,
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
def accrete_graupel_with_snow(
    qsnow,
    qgraupel,
    temperature,
    density,
    vterminal_s,
    vterminal_g,
):
    """
    Graupel accretion with snow, Lin et al. (1983)
    Fortran name is pgacs
    """
    from __externals__ import (
        acc6,
        acc7,
        acco_0_3,
        acco_1_3,
        acco_2_3,
        cgacs,
        fs2g_fac,
        timestep,
    )

    if (
        (temperature < mpcons.TICE0)
        and (qsnow > mpcons.QCMIN)
        and (qgraupel > mpcons.QCMIN)
    ):
        sink = timestep * physfun.accretion_3d(
            vterminal_g,
            vterminal_s,
            qsnow,
            qgraupel,
            density,
            cgacs,
            acco_0_3,
            acco_1_3,
            acco_2_3,
            acc6,
            acc7,
        )
        sink = min(fs2g_fac * qsnow, sink)

        qsnow -= sink
        qgraupel += sink

    return qsnow, qgraupel


@gtfunction
def autoconvert_snow_to_graupel(
    qsnow,
    qgraupel,
    temperature,
    density,
):
    """
    Snow to graupel autoconversion, Lin et al. (1983)
    Fortran name is pgaut
    """
    from __externals__ import fs2g_fac, qs0_crt, timestep

    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qsnow > mpcons.QCMIN):
        sink = 0.0
        qsm = qs0_crt / density

        if qsnow > qsm:
            factor = timestep * 1.0e-3 * exp(0.09 * tc)
            sink = factor / (1.0 + factor) * (qsnow - qsm)

        sink = min(fs2g_fac * qsnow, sink)

        qsnow -= sink
        qgraupel += sink

    return qsnow, qgraupel


@gtfunction
def accrete_graupel_with_cloud_water_and_rain(
    qvapor,
    qliquid,
    qrain,
    qice,
    qsnow,
    qgraupel,
    temperature,
    density,
    density_factor,
    vterminal_r,
    vterminal_g,
    te,
    cvm,
    lcpk,
    icpk,
    tcpk,
    tcp3,
):
    """
    Graupel accretion with cloud water and rain, Lin et al. (1983)
    Fortran name is pgacw_pgacr
    """
    from __externals__ import (
        acc4,
        acc5,
        acco_0_2,
        acco_1_2,
        acco_2_2,
        bling,
        cgacr,
        cgacw,
        mug,
        timestep,
    )

    tc = temperature - mpcons.TICE0

    if (tc < 0.0) and (qgraupel > mpcons.QCMIN):
        pgacw = 0.0

        if qliquid > mpcons.QCMIN:
            qden = qgraupel * density
            factor = timestep * physfun.accretion_2d(
                qden, cgacw, density_factor, bling, mug
            )
            pgacw = factor / (1.0 + factor) * qliquid

        pgacr = 0.0
        if qrain > mpcons.QCMIN:
            pgacr = min(
                timestep
                * physfun.accretion_3d(
                    vterminal_g,
                    vterminal_r,
                    qrain,
                    qgraupel,
                    density,
                    cgacr,
                    acco_0_2,
                    acco_1_2,
                    acco_2_2,
                    acc4,
                    acc5,
                ),
                qrain,
            )

        sink = pgacr + pgacw
        factor = min(sink, dim(mpcons.TICE0, temperature) / icpk) / max(
            sink, mpcons.QCMIN
        )
        pgacr = factor * pgacr
        pgacw = factor * pgacw

        sink = pgacr + pgacw

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
            -pgacw,
            -pgacr,
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
        temperature,
        cvm,
        lcpk,
        icpk,
        tcpk,
        tcp3,
    )


def ice_cloud(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temperature: FloatField,
    density: FloatField,
    density_factor: FloatField,
    vterminal_w: FloatField,
    vterminal_r: FloatField,
    vterminal_i: FloatField,
    vterminal_s: FloatField,
    vterminal_g: FloatField,
    h_var: FloatFieldIJ,
):
    from __externals__ import z_slope_ice

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
        ) = melt_cloud_ice(
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
        ) = freeze_cloud_water(
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

    with computation(FORWARD):
        with interval(0, 1):
            if z_slope_ice:
                # linear_prof
                di = 0.0
        with interval(1, None):
            if z_slope_ice:
                dq = 0.5 * (qice - qice[0, 0, -1])
    with computation(FORWARD):
        with interval(1, -1):
            if z_slope_ice:
                # Use twice the strength of the
                # positive definiteness limiter (lin et al 1994)
                di = 0.5 * min(abs(dq + dq[0, 0, +1]), 0.5 * qice[0, 0, 0])
                if dq * dq[0, 0, +1] <= 0.0:
                    if dq > 0.0:  # Local maximum
                        di = min(di, min(dq, -dq[0, 0, +1]))
                    else:  # Local minimum
                        di = 0.0
        with interval(-1, None):
            if z_slope_ice:
                di = 0.0
    with computation(PARALLEL), interval(...):
        if z_slope_ice:
            # Impose a presumed background horizontal variability that is
            # proportional to the value itself
            di = max(di, max(0.0, h_var * qice))
        else:
            di = max(0.0, h_var * qice)

    with computation(PARALLEL), interval(...):
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
        ) = melt_snow(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            density_factor,
            vterminal_w,
            vterminal_r,
            vterminal_s,
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
            temperature,
            cvm,
            lcpk,
            icpk,
            tcpk,
            tcp3,
        ) = melt_graupel(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            density_factor,
            vterminal_w,
            vterminal_r,
            vterminal_g,
            cvm,
            te,
            lcpk,
            icpk,
            tcpk,
            tcp3,
        )

        qice, qsnow = accrete_snow_with_ice(
            qice,
            qsnow,
            temperature,
            density,
            density_factor,
            vterminal_i,
            vterminal_s,
        )

        qice, qsnow, di, temperature = autoconvert_ice_to_snow(
            qice, qsnow, temperature, density, di
        )

        qice, qgraupel = accrete_graupel_with_ice(
            qice,
            qgraupel,
            temperature,
            density,
            density_factor,
            vterminal_i,
            vterminal_g,
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
        ) = accrete_snow_with_rain_and_freeze_to_graupel(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            vterminal_r,
            vterminal_s,
            te,
            cvm,
            lcpk,
            icpk,
            tcpk,
            tcp3,
        )

        qsnow, qgraupel = accrete_graupel_with_snow(
            qsnow,
            qgraupel,
            temperature,
            density,
            vterminal_s,
            vterminal_g,
        )

        qsnow, qgraupel = autoconvert_snow_to_graupel(
            qsnow, qgraupel, temperature, density
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
        ) = accrete_graupel_with_cloud_water_and_rain(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            density_factor,
            vterminal_r,
            vterminal_g,
            te,
            cvm,
            lcpk,
            icpk,
            tcpk,
            tcp3,
        )


class IceCloud:
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

        self._ice_cloud = stencil_factory.from_origin_domain(
            func=ice_cloud,
            externals={
                "timestep": timestep,
                "z_slope_ice": config.z_slope_ice,
                "vdiffflag": config.vdiffflag,
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "d1_ice": config.d1_ice,
                "d1_vap": config.d1_vap,
                "li00": config.li00,
                "li20": config.li20,
                "lv00": config.lv00,
                "t_wfr": config.t_wfr,
                "tice": mpcons.TICE0,
                "ql_mlt": config.ql_mlt,
                "tau_imlt": config.tau_imlt,
                "tice_mlt": config.tice_mlt,
                "tau_i2s": config.tau_i2s,
                "qi0_crt": config.qi0_crt,
                "qs0_crt": config.qs0_crt,
                "acc0": config.acc[0],
                "acc1": config.acc[1],
                "acc2": config.acc[2],
                "acc3": config.acc[3],
                "acc4": config.acc[4],
                "acc5": config.acc[5],
                "acc6": config.acc[6],
                "acc7": config.acc[7],
                "acc12": config.acc[12],
                "acc13": config.acc[13],
                "acc14": config.acc[14],
                "acc15": config.acc[15],
                "acc16": config.acc[16],
                "acc17": config.acc[17],
                "acc18": config.acc[18],
                "acc19": config.acc[19],
                "acco_0_0": config.acco[0][0],
                "acco_1_0": config.acco[1][0],
                "acco_2_0": config.acco[2][0],
                "acco_0_1": config.acco[0][1],
                "acco_1_1": config.acco[1][1],
                "acco_2_1": config.acco[2][1],
                "acco_0_2": config.acco[0][2],
                "acco_1_2": config.acco[1][2],
                "acco_2_2": config.acco[2][2],
                "acco_0_3": config.acco[0][3],
                "acco_1_3": config.acco[1][3],
                "acco_2_3": config.acco[2][3],
                "acco_0_6": config.acco[0][6],
                "acco_1_6": config.acco[1][6],
                "acco_2_6": config.acco[2][6],
                "acco_0_7": config.acco[0][7],
                "acco_1_7": config.acco[1][7],
                "acco_2_7": config.acco[2][7],
                "acco_0_8": config.acco[0][8],
                "acco_1_8": config.acco[1][8],
                "acco_2_8": config.acco[2][8],
                "acco_0_9": config.acco[0][9],
                "acco_1_9": config.acco[1][9],
                "acco_2_9": config.acco[2][9],
                "cracs": config.cracs,
                "csacr": config.csacr,
                "csacw": config.csacw,
                "cgacw": config.cgacw,
                "cgacr": config.cgacr,
                "cgacs": config.cgacs,
                "csaci": config.csaci,
                "cgaci": config.cgaci,
                "csmlt_1": config.csmlt_1,
                "csmlt_2": config.csmlt_2,
                "csmlt_3": config.csmlt_3,
                "csmlt_4": config.csmlt_4,
                "cgmlt_1": config.cgmlt_1,
                "cgmlt_2": config.cgmlt_2,
                "cgmlt_3": config.cgmlt_3,
                "cgmlt_4": config.cgmlt_4,
                "cgfr_1": config.cgfr_1,
                "cgfr_2": config.cgfr_2,
                "do_new_acc_water": config.do_new_acc_water,
                "do_new_acc_ice": config.do_new_acc_ice,
                "mur": config.mur,
                "blinr": config.blinr,
                "mus": config.mus,
                "blins": config.blins,
                "qs_mlt": config.qs_mlt,
                "mug": mu_g,
                "bling": blin_g,
                "fi2s_fac": config.fi2s_fac,
                "fi2g_fac": config.fi2g_fac,
                "fs2g_fac": config.fs2g_fac,
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
        vterminal_water: FloatField,
        vterminal_rain: FloatField,
        vterminal_ice: FloatField,
        vterminal_snow: FloatField,
        vterminal_graupel: FloatField,
        h_var: FloatFieldIJ,
    ):
        """
        Ice cloud microphysics,
        Fortran name is ice_cloud
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
            vterminal_water (in):
            vterminal_rain (in):
            vterminal_ice (in):
            vterminal_snow (in):
            vterminal_graupel (in):
            h_var (in):
        """
        self._ice_cloud(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            density,
            density_factor,
            vterminal_water,
            vterminal_rain,
            vterminal_ice,
            vterminal_snow,
            vterminal_graupel,
            h_var,
        )
