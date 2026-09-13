import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics._config import AdjustNegativeTracerConfig


def adjust_negative_tracers(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temperature: FloatField,
    delp: FloatField,
    condensation: FloatFieldIJ,
):
    from __externals__ import convt, ntimes

    with computation(PARALLEL), interval(...):
        upper_fix = 0.0  # type: FloatField
        lower_fix = 0.0  # type: FloatField

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
            qvapor, qliquid, qrain, qice, qsnow, qgraupel, temperature
        )

        # if cloud ice < 0, borrow from snow
        if qice < 0.0:
            sink = min(-qice, max(0, qsnow))
            qice += sink
            qsnow -= sink

        # if snow < 0, borrow from graupel
        if qsnow < 0.0:
            sink = min(-qsnow, max(0, qgraupel))
            qsnow += sink
            qgraupel -= sink

        # if graupel < 0, borrow from rain
        if qgraupel < 0.0:
            sink = min(-qgraupel, max(0, qrain))
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
                0.0,
                sink,
                te,
            )

        # if rain < 0, borrow from cloud water
        if qrain < 0.0:
            sink = min(-qrain, max(0, qliquid))
            qrain += sink
            qliquid -= sink

        # if cloud water < 0, borrow from vapor
        if qliquid < 0.0:
            sink = min(-qliquid, max(0, qvapor))
            condensation += (sink * delp) * convt * ntimes
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
                sink,
                0.0,
                0.0,
                0.0,
                0.0,
                te,
            )

    with computation(BACKWARD):
        with interval(1, 2):
            if qvapor[0, 0, -1] < 0:  # top level is negative
                # reduce level 1 by that amount to compensate:
                qvapor = qvapor + qvapor[0, 0, -1] * delp[0, 0, -1] / delp
        with interval(0, 1):
            if qvapor < 0.0:
                qvapor = 0.0  # top level is now 0

    with computation(FORWARD):
        with interval(1, -1):
            # if we borrowed from this level to fix the upper level, account for that:
            if lower_fix[0, 0, -1] != 0:
                qvapor += lower_fix[0, 0, -1]
            if qvapor < 0:  # If negative borrow from below
                lower_fix = qvapor * delp / delp[0, 0, 1]
                qvapor = 0
        with interval(-1, None):
            # if we borrowed from this level to fix the upper level, account for that:
            if lower_fix[0, 0, -1] != 0:
                qvapor += lower_fix[0, 0, -1]

    with computation(BACKWARD):
        with interval(-1, None):
            if (qvapor < 0) and (qvapor[0, 0, -1] > 0):
                # If we can, fix the bottom layer by borrowing from above
                upper_fix = min(-qvapor * delp, qvapor[0, 0, -1] * delp[0, 0, -1])
                qvapor += upper_fix / delp
        with interval(-2, -1):
            if upper_fix[0, 0, 1] != 0.0:
                qvapor -= upper_fix / delp


class AdjustNegativeTracers:
    """
    Fortran name is neg_adj
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: AdjustNegativeTracerConfig,
        convert_mm_day: float,
    ):

        self._idx: GridIndexing = stencil_factory.grid_indexing

        self._adjust_negative_tracers = stencil_factory.from_origin_domain(
            func=adjust_negative_tracers,
            externals={
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
                "ntimes": config.ntimes,
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
        condensation: FloatFieldIJ,
    ):
        """
        Adjust tracer mixing ratios to fix negative values

        Args:

            qvapor (inout): Water vapor mass mixing ratio
            qliquid (inout): Cloud water mass mixing ratio
            qrain (inout): Rain mass mixing ratio
            qice (inout): Cloud ice mass mixing ratio
            qsnow (inout): Snow mass mixing ratio
            qgraupel (inout): Graupel mass mixing ratio
            temperature (inout): Grid cell temperature
            delp (in): Grid cell pressure thickness
            condensation (inout): Total column condensate
        """

        self._adjust_negative_tracers(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            delp,
            condensation,
        )
