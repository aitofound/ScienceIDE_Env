import math

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, StencilFactory
from ndsl.dsl.gt4py import FORWARD, PARALLEL, computation, exp, interval, log
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig


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
        do_mp_table_emulation,
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

        # calculate supersaturation and subgrid variability of water
        qpz = qvapor + qliquid
        if do_mp_table_emulation:
            qsat, dqdt = physfun.wqs(tin, density)
        else:
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
            if qsat > q_plus:
                dq = qsat - qpz
            else:
                dq = 0.25 * (qsat - q_minus) ** 2 / dqh
            qden = qrain * density
            t2 = tin * tin
            sink = physfun.sublimation_function(
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
            sink = min(
                qrain, min(timestep * fac_revap * sink, dqv / (1.0 + lcpk * dqdt))
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


def accrete_rain(
    qliquid: FloatField,
    qrain: FloatField,
    temperature: FloatField,
    density: FloatField,
    density_factor: FloatField,
    vterminal_water: FloatField,
    vterminal_rain: FloatField,
):
    """
    Rain accretion with cloud water, Lin et al. (1983)
    Fortran name is pracw
    """
    from __externals__ import (
        acc8,
        acc9,
        acco0_4,
        acco1_4,
        acco2_4,
        blinr,
        cracw,
        do_new_acc_water,
        mur,
        t_wfr,
        timestep,
    )

    with computation(PARALLEL), interval(...):
        if (
            (temperature > t_wfr)
            and (qrain > mpcons.QCMIN)
            and (qliquid > mpcons.QCMIN)
        ):
            qden = qrain * density

            if do_new_acc_water:
                sink = timestep * physfun.accretion_3d(
                    vterminal_rain,
                    vterminal_water,
                    qliquid,
                    qrain,
                    density,
                    cracw,
                    acco0_4,
                    acco1_4,
                    acco2_4,
                    acc8,
                    acc9,
                )
            else:
                sink = timestep * physfun.accretion_2d(
                    qden, cracw, density_factor, blinr, mur
                )
                sink = sink / (1.0 + sink) * qliquid

            qliquid -= sink
            qrain += sink


def autoconvert_water_rain(
    qliquid: FloatField,
    qrain: FloatField,
    cloud_condensation_nuclei: FloatField,
    temperature: FloatField,
    density: FloatField,
    h_var: FloatFieldIJ,
):
    """
    Cloud water to rain autoconversion, Hong et al. (2004)
    Fortran name is praut
    """
    from __externals__ import (
        cpaut,
        do_psd_water_num,
        fac_rc,
        irain_f,
        muw,
        pcaw,
        pcbw,
        t_wfr,
        timestep,
        z_slope_liq,
    )

    # linear_prof
    with computation(FORWARD):
        with interval(0, 1):
            if (irain_f == 0) and (z_slope_liq):
                dl = 0.0
        with interval(1, None):
            if (irain_f == 0) and (z_slope_liq):
                dq = 0.5 * (qliquid - qliquid[0, 0, -1])
    with computation(FORWARD):
        with interval(1, -1):
            if (irain_f == 0) and (z_slope_liq):
                # Use twice the strength of the
                # positive definiteness limiter (lin et al 1994)
                dl = 0.5 * min(abs(dq + dq[0, 0, 1]), 0.5 * qliquid)
                if dq * dq[0, 0, 1] <= 0.0:
                    if dq > 0.0:  # Local maximum
                        dl = min(dl, min(dq, -dq[0, 0, 1]))
                    else:  # Local minimum
                        dl = 0.0
        with interval(-1, None):
            if (irain_f == 0) and (z_slope_liq):
                dl = 0.0
    with computation(PARALLEL), interval(...):
        if irain_f == 0:
            if z_slope_liq:
                # Impose a presumed background horizontal variability that is
                # proportional to the value itself
                dl = max(max(dl, h_var * qliquid), 0.0)
            else:
                dl = max(0.0, h_var * qliquid)

    with computation(PARALLEL), interval(...):
        if irain_f == 0:
            # rest of praut
            if (temperature > t_wfr) and (qliquid > mpcons.QCMIN):
                if do_psd_water_num:
                    cloud_condensation_nuclei = physfun.calc_particle_concentration(
                        qliquid, density, pcaw, pcbw, muw
                    )
                    cloud_condensation_nuclei /= density
                qc = fac_rc * cloud_condensation_nuclei
                dl = min(max(mpcons.QCMIN, dl), 0.5 * qliquid)
                dq = 0.5 * (qliquid + dl - qc)

                if dq > 0.0:
                    c_praut = cpaut * exp(
                        (-1.0 / 3.0) * log(cloud_condensation_nuclei * mpcons.RHO_W)
                    )
                    sink = (
                        min(1.0, dq / dl)
                        * timestep
                        * c_praut
                        * density
                        * exp((7.0 / 3.0) * log(qliquid))
                    )
                    sink = min(qliquid, sink)

                    qliquid -= sink
                    qrain += sink
        else:  # if irain_f == 1:
            if (temperature > t_wfr) and (qliquid > mpcons.QCMIN):
                if do_psd_water_num:
                    cloud_condensation_nuclei = physfun.calc_particle_concentration(
                        qliquid, density, pcaw, pcbw, muw
                    )
                    cloud_condensation_nuclei /= density

                qc = fac_rc * cloud_condensation_nuclei
                dq = qliquid - qc

                if dq > 0.0:
                    c_praut = cpaut * exp(
                        (-1.0 / 3.0) * log(cloud_condensation_nuclei * mpcons.RHO_W)
                    )
                    sink = min(
                        dq,
                        timestep * c_praut * density * exp((7.0 / 3.0) * log(qliquid)),
                    )
                    sink = min(sink, qliquid)

                    qliquid -= sink
                    qrain += sink


class WarmRain:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: GFDLCloudMPConfig,
        timestep: float,
    ):

        self._idx: GridIndexing = stencil_factory.grid_indexing
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
                "do_mp_table_emulation": config.do_mp_table_emulation,
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
        )

        self._accrete_rain(
            qliquid,
            qrain,
            temperature,
            density,
            density_factor,
            vterminal_water,
            vterminal_rain,
        )

        self._autoconvert_water_rain(
            qliquid,
            qrain,
            cloud_condensation_nuclei,
            temperature,
            density,
            h_var,
        )
