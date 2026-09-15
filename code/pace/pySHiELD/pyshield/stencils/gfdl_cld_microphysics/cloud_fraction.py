import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import PARALLEL, computation, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log, log10, max, min
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig


@gtfunction
def cloud_scheme_1(
    qpz,
    qstar,
    q_cond,
    qa,
    pz,
    rh,
    h_var,
):
    """
    GFDL cloud scheme
    """
    from __externals__ import cld_min, do_cld_adj, f_dq_m, f_dq_p, icloud_f, rh_thres

    if (rh > rh_thres) and (qpz > mpcons.QCMIN):
        dq = h_var * qpz
        if do_cld_adj:
            q_plus = qpz + dq * f_dq_p * min(
                1.0, max(0.0, (pz - 200.0e2) / (1000.0e2 - 200.0e2))
            )
        else:
            q_plus = qpz + dq * f_dq_p
        q_minus = qpz - dq * f_dq_m
        if icloud_f == 2:
            if qstar < qpz:
                qa = 1.0
            else:
                qa = 0.0
        elif icloud_f == 3:
            if qstar < qpz:
                qa = 1.0
            else:
                if qstar < q_plus:
                    qa = (q_plus - qstar) / (dq * f_dq_p)
                else:
                    qa = 0.0
                if q_cond > mpcons.QCMIN:
                    qa = max(cld_min, qa)
                qa = min(1.0, qa)
        else:
            if qstar < q_minus:
                qa = 1.0
            else:
                if qstar < q_plus:
                    if icloud_f == 0:
                        qa = (q_plus - qstar) / (dq * f_dq_p + dq * f_dq_m)
                    else:  # icloud_f == 1:
                        qa = (q_plus - qstar) / (
                            (dq * f_dq_p + dq * f_dq_m) * (1.0 - q_cond)
                        )
                else:
                    qa = 0.0
                if q_cond > mpcons.QCMIN:
                    qa = max(cld_min, qa)
                qa = min(1.0, qa)
    else:
        qa = 0.0

    return qa


@gtfunction
def cloud_scheme_2(
    qstar,
    q_cond,
    qa,
    rh,
):
    """
    Xu and Randall (1996)
    """
    from __externals__ import rh_thres, xr_a, xr_b, xr_c

    if rh >= 1.0:
        qa = 1.0
    elif (rh > rh_thres) and (q_cond > mpcons.QCMIN):
        qa = exp(xr_a * log(rh)) * (
            1.0
            - exp(
                -xr_b
                * max(0.0, q_cond)
                / max(1.0e-5, exp(xr_c * log(max(1.0e-10, 1.0 - rh) * qstar)))
            )
        )
        qa = max(0.0, min(1.0, qa))
    else:
        qa = 0.0

    return qa


@gtfunction
def cloud_scheme_3(
    q_cond,
    q_liquid,
    q_solid,
    qa,
    gsize,
):
    """
    Park et al. 2016
    """
    if q_cond > mpcons.QCMIN:
        qa = (
            1.0
            / 50.0
            * (
                5.77
                * (100.0 - gsize / 1000.0)
                * exp(1.07 * log(max(mpcons.QCMIN * 1000.0, q_cond * 1000.0)))
                + 4.82
                * (gsize / 1000.0 - 50.0)
                * exp(0.94 * log(max(mpcons.QCMIN * 1000.0, q_cond * 1000.0)))
            )
        )
        qa = qa * (0.92 / 0.96 * q_liquid / q_cond + 1.0 / 0.96 * q_solid / q_cond)
        qa = max(0.0, min(1.0, qa))
    else:
        qa = 0.0

    return qa


@gtfunction
def cloud_scheme_4(
    q_cond,
    qa,
    gsize,
):
    """
    Gultepe and Isaac (2007)
    """
    # TODO: fix log10 when possible
    sigma = 0.28 + exp(0.49 * log(max(mpcons.QCMIN * 1000.0, q_cond * 1000.0)))
    gam = max(0.0, q_cond * 1000.0) / sigma
    if gam < 0.18:
        qa10 = 0.0
    elif gam > 2.0:
        qa10 = 1.0
    else:
        qa10 = -0.1754 + 0.9811 * gam - 0.2223 * gam**2 + 0.0104 * gam**3
        qa10 = max(0.0, min(1.0, qa10))
    if gam < 0.12:
        qa100 = 0.0
    elif gam > 1.85:
        qa100 = 1.0
    else:
        qa100 = -0.0913 + 0.7213 * gam + 0.1060 * gam**2 - 0.0946 * gam**3
        qa100 = max(0.0, min(1.0, qa100))

    qa = qa10 + ((log10(gsize / 1000.0)) - 1) * (qa100 - qa10)
    qa = max(0.0, min(1.0, qa))
    return qa


def cloud_fraction(
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
):

    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        cfflag,
        do_mp_table_emulation,
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
            if do_mp_table_emulation:
                qstar, dqdt = physfun.iqs(tin, density)
            else:
                qstar, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
        elif tin >= mpcons.TICE0:
            if do_mp_table_emulation:
                qstar, dqdt = physfun.wqs(tin, density)
            else:
                qstar, dqdt = physfun.sat_spec_hum_water(tin, density)
        else:
            if do_mp_table_emulation:
                qsi, dqdt = physfun.iqs(tin, density)
                qsw, dqdt = physfun.wqs(tin, density)
            else:
                qsi, dqdt = physfun.sat_spec_hum_water_ice(tin, density)
                qsw, dqdt = physfun.sat_spec_hum_water(tin, density)
            if q_cond > mpcons.QCMIN:
                rqi = q_solid / q_cond
            else:
                rqi = (mpcons.TICE0 - tin) / (mpcons.TICE0 - t_wfr)
            qstar = rqi * qsi + (1.0 - rqi) * qsw

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


class CloudFraction:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: GFDLCloudMPConfig,
    ):
        self._idx: GridIndexing = stencil_factory.grid_indexing

        self._cloud_fraction = stencil_factory.from_origin_domain(
            func=cloud_fraction,
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
        qa: FloatField,
        temperature: FloatField,
        density: FloatField,
        pz: FloatField,
        h_var: FloatFieldIJ,
        gsize: FloatFieldIJ,
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
            h_var,
            gsize,
        )
