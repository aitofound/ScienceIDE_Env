import numpy as np

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, interval, sqrt
from ndsl.dsl.typing import Bool, Float, FloatField, FloatFieldIJ
from ndsl.grid import GridData
from ndsl.performance.timer import NullTimer, Timer
from ndsl.stencils import copy
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.cloud_fraction import CloudFraction
from pyshield.stencils.gfdl_cld_microphysics.gfdl_cld_microphysics_state import (
    GFDLCloudMicrophysicsState,
)
from pyshield.stencils.gfdl_cld_microphysics.mp_fast import FastMicrophysics
from pyshield.stencils.gfdl_cld_microphysics.mp_full import FullMicrophysics
from pyshield.stencils.gfdl_cld_microphysics.neg_adj import AdjustNegativeTracers


def reset_initial_values_and_make_copies(
    adj_vmr: FloatField,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    delp: FloatField,
    temperature: FloatField,
    ua: FloatField,
    va: FloatField,
    qvapor0: FloatField,
    qliquid0: FloatField,
    qrain0: FloatField,
    qice0: FloatField,
    qsnow0: FloatField,
    qgraupel0: FloatField,
    dp0: FloatField,
    temperature0: FloatField,
    u0: FloatField,
    v0: FloatField,
    column_energy_change: FloatFieldIJ,
    cond: FloatFieldIJ,
):
    with computation(PARALLEL), interval(...):
        adj_vmr = 1.0
        qvapor0 = qvapor
        qliquid0 = qliquid
        qrain0 = qrain
        qice0 = qice
        qsnow0 = qsnow
        qgraupel0 = qgraupel
        dp0 = delp
        temperature0 = temperature
        u0 = ua
        v0 = va
    with computation(FORWARD), interval(-1, None):
        column_energy_change = 0.0
        cond = 0.0


def convert_virtual_to_true_temperature_and_calc_total_energy(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temperature: FloatField,
    delp: FloatField,
    total_energy: FloatField,
):
    """
    The FV3 dycore uses virtual temperature, while physics uses true temperature.
    This stencil converts them so microphysics can be called inside the dycore
    """
    from __externals__ import c_air, consv_te, do_inline_mp, hydrostatic

    with computation(PARALLEL), interval(...):
        if do_inline_mp:
            q_cond = qliquid + qrain + qice + qsnow + qgraupel
            temperature = temperature / (1 + constants.ZVIR * qvapor * (1.0 - q_cond))

        if consv_te:
            if hydrostatic:
                total_energy = -c_air * temperature * delp
            else:
                total_energy = (
                    -physfun.calc_moist_total_energy(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        temperature,
                        delp,
                        True,
                    )
                    * constants.GRAV
                )


def calc_sedimentation_energy_loss(
    energy_loss: FloatFieldIJ, column_energy_change: FloatFieldIJ, gsize: FloatFieldIJ
):
    """
    Last calculation of mtetw, split into a separate stencil to cover the
    if (present(te_loss)) conditional
    """
    with computation(FORWARD), interval(-1, None):
        energy_loss = column_energy_change * gsize**2.0


def moist_total_energy_and_water(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    temperature: FloatField,
    ua: FloatField,
    va: FloatField,
    wa: FloatField,
    delp: FloatField,
    gsize: FloatFieldIJ,
    column_energy_change: FloatFieldIJ,
    vapor: FloatFieldIJ,
    water: FloatFieldIJ,
    rain: FloatFieldIJ,
    ice: FloatFieldIJ,
    snow: FloatFieldIJ,
    graupel: FloatFieldIJ,
    sen: Float,
    stress: Float,
    tot_energy: FloatField,
    tot_water: FloatField,
    total_energy_bot: FloatFieldIJ,
    total_water_bot: FloatFieldIJ,
):
    """
    Fortran name is mtetw
    """
    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        c_air,
        hydrostatic,
        li00,
        lv00,
        moist_q,
        timestep,
    )

    with computation(PARALLEL), interval(...):
        q_liq = qliquid + qrain
        q_solid = qice + qsnow + qgraupel
        q_cond = q_liq + q_solid
        if moist_q:
            con = 1.0 - (qvapor + q_cond)
            cvm = con + qvapor * c1_vap + q_liq * c1_liq + q_solid * c1_ice
        else:
            cvm = 1.0 + qvapor * c1_vap + q_liq * c1_liq + q_solid * c1_ice
        tot_energy = (cvm * temperature + lv00 * qvapor - li00 * q_solid) * c_air
        if hydrostatic:
            tot_energy = tot_energy + 0.5 * (ua**2 + va**2)
        else:
            tot_energy = tot_energy + 0.5 * (ua**2 + va**2 + wa**2)
        tot_energy = constants.RGRAV * tot_energy * delp * gsize**2.0
        tot_water = constants.RGRAV * (qvapor + q_cond) * delp * gsize**2.0

    with computation(FORWARD), interval(-1, None):
        total_energy_bot = (
            column_energy_change
            + (lv00 * c_air * vapor - li00 * c_air * (ice + snow + graupel))
            * timestep
            / 86400
            + sen * timestep
            + stress * timestep
        ) * gsize**2.0
        total_water_bot = (
            (vapor + water + rain + ice + snow + graupel)
            * timestep
            / 86400
            * gsize**2.0
        )


def convert_specific_to_mass_mixing_ratios_and_calculate_densities(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    delp: FloatField,
    delz: FloatField,
    temperature: FloatField,
    density: FloatField,
    pz: FloatField,
    bottom_density: FloatFieldIJ,
):
    from __externals__ import do_inline_mp

    with computation(PARALLEL), interval(...):
        if do_inline_mp:
            con_r8 = 1.0 - (qvapor + qliquid + qrain + qice + qsnow + qgraupel)
        else:
            con_r8 = 1.0 - qvapor
        delp = delp * con_r8
        rcon_r8 = 1.0 / con_r8
        qvapor = qvapor * rcon_r8
        qliquid = qliquid * rcon_r8
        qrain = qrain * rcon_r8
        qice = qice * rcon_r8
        qsnow = qsnow * rcon_r8
        qgraupel = qgraupel * rcon_r8

        # Dry air density and layer-mean pressure thickness
        density = -delp / (constants.GRAV * delz)
        pz = density * constants.RDGAS * temperature
    with computation(FORWARD), interval(-1, None):
        bottom_density = density


def calculate_density_factor(
    density: FloatField,
    density_factor: FloatField,
    bottom_density: FloatFieldIJ,
):
    with computation(BACKWARD), interval(...):
        density_factor = sqrt(bottom_density / density)


def cloud_nuclei_subgrid_and_relative_humidity(
    geopotential_surface_height: FloatFieldIJ,
    qnl: FloatField,
    qni: FloatField,
    density: FloatField,
    cloud_condensation_nuclei: FloatField,
    cloud_ice_nuclei: FloatField,
    gsize: FloatFieldIJ,
    h_var: FloatFieldIJ,
    rh_adj: FloatFieldIJ,
    rh_rain: FloatFieldIJ,
):
    from __externals__ import ccn_l, ccn_o, dw_land, dw_ocean, prog_ccn, rh_inc, rh_inr

    with computation(PARALLEL), interval(...):
        if prog_ccn:
            # boucher and lohmann (1995)
            nl = min(
                1.0, abs(geopotential_surface_height / (10.0 * constants.GRAV))
            ) * (10.0**2.24 * (qnl * density * 1.0e9) ** 0.257) + (
                1.0 - min(1.0, abs(geopotential_surface_height) / (10 * constants.GRAV))
            ) * (
                10.0**2.06 * (qnl * density * 1.0e9) ** 0.48
            )
            ni = qni
            cloud_condensation_nuclei = (max(10.0, nl) * 1.0e6) / density
            cloud_ice_nuclei = (max(10.0, ni) * 1.0e6) / density
        else:
            cloud_condensation_nuclei = (
                (
                    ccn_l
                    * min(
                        1.0, abs(geopotential_surface_height) / (10.0 * constants.GRAV)
                    )
                    + ccn_o
                    * (
                        1.0
                        - min(
                            1.0,
                            abs(geopotential_surface_height) / (10.0 * constants.GRAV),
                        )
                    )
                )
                * 1.0e6
                / density
            )
            cloud_ice_nuclei = 0.0 / density

    with computation(FORWARD), interval(-1, None):
        t_lnd = dw_land * sqrt(gsize / 1.0e5)
        t_ocn = dw_ocean * sqrt(gsize / 1.0e5)
        tmp = min(1.0, abs(geopotential_surface_height) / (10.0 * constants.GRAV))
        h_var = t_lnd * tmp + t_ocn * (1.0 - tmp)
        h_var = min(0.20, max(0.01, h_var))

        rh_adj = 1.0 - h_var - rh_inc
        rh_rain = max(0.35, rh_adj - rh_inr)


def calculate_particle_properties(
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    density: FloatField,
    pc_liquid: FloatField,
    pc_rain: FloatField,
    pc_ice: FloatField,
    pc_snow: FloatField,
    pc_graupel: FloatField,
    ed_liquid: FloatField,
    ed_rain: FloatField,
    ed_ice: FloatField,
    ed_snow: FloatField,
    ed_graupel: FloatField,
    oe_liquid: FloatField,
    oe_rain: FloatField,
    oe_ice: FloatField,
    oe_snow: FloatField,
    oe_graupel: FloatField,
    rr_liquid: FloatField,
    rr_rain: FloatField,
    rr_ice: FloatField,
    rr_snow: FloatField,
    rr_graupel: FloatField,
    tv_liquid: FloatField,
    tv_rain: FloatField,
    tv_ice: FloatField,
    tv_snow: FloatField,
    tv_graupel: FloatField,
):
    """
    calculation of particle concentration (pc), effective diameter (ed),
    optical extinction (oe), radar reflectivity factor (rr), and
    mass-weighted terminal velocity (tv)
    """
    from __externals__ import (
        bling,
        blini,
        blinr,
        blins,
        blinw,
        edag,
        edai,
        edar,
        edas,
        edaw,
        edbg,
        edbi,
        edbr,
        edbs,
        edbw,
        mug,
        mui,
        mur,
        mus,
        muw,
        oeag,
        oeai,
        oear,
        oeas,
        oeaw,
        oebg,
        oebi,
        oebr,
        oebs,
        oebw,
        pcag,
        pcai,
        pcar,
        pcas,
        pcaw,
        pcbg,
        pcbi,
        pcbr,
        pcbs,
        pcbw,
        rrag,
        rrai,
        rrar,
        rras,
        rraw,
        rrbg,
        rrbi,
        rrbr,
        rrbs,
        rrbw,
        tvag,
        tvai,
        tvar,
        tvas,
        tvaw,
        tvbg,
        tvbi,
        tvbr,
        tvbs,
        tvbw,
    )

    with computation(PARALLEL), interval(...):
        if qliquid > mpcons.QCMIN:
            pc_liquid = physfun.calc_particle_concentration(
                qliquid, density, pcaw, pcbw, muw
            )
            ed_liquid = physfun.calc_effective_diameter(
                qliquid, density, edaw, edbw, muw
            )
            oe_liquid = physfun.calc_optical_extinction(
                qliquid, density, oeaw, oebw, muw
            )
            rr_liquid = physfun.calc_radar_reflectivity(
                qliquid, density, rraw, rrbw, muw
            )
            tv_liquid = physfun.calc_terminal_velocity(
                qliquid, density, tvaw, tvbw, muw, blinw
            )

        if qice > mpcons.QCMIN:
            pc_ice = physfun.calc_particle_concentration(qice, density, pcai, pcbi, mui)
            ed_ice = physfun.calc_effective_diameter(qice, density, edai, edbi, mui)
            oe_ice = physfun.calc_optical_extinction(qice, density, oeai, oebi, mui)
            rr_ice = physfun.calc_radar_reflectivity(qice, density, rrai, rrbi, mui)
            tv_ice = physfun.calc_terminal_velocity(
                qice, density, tvai, tvbi, mui, blini
            )

        if qrain > mpcons.QCMIN:
            pc_rain = physfun.calc_particle_concentration(
                qrain, density, pcar, pcbr, mur
            )
            ed_rain = physfun.calc_effective_diameter(qrain, density, edar, edbr, mur)
            oe_rain = physfun.calc_optical_extinction(qrain, density, oear, oebr, mur)
            rr_rain = physfun.calc_radar_reflectivity(qrain, density, rrar, rrbr, mur)
            tv_rain = physfun.calc_terminal_velocity(
                qrain, density, tvar, tvbr, mur, blinr
            )

        if qsnow > mpcons.QCMIN:
            pc_snow = physfun.calc_particle_concentration(
                qsnow, density, pcas, pcbs, mus
            )
            ed_snow = physfun.calc_effective_diameter(qsnow, density, edas, edbs, mus)
            oe_snow = physfun.calc_optical_extinction(qsnow, density, oeas, oebs, mus)
            rr_snow = physfun.calc_radar_reflectivity(qsnow, density, rras, rrbs, mus)
            tv_snow = physfun.calc_terminal_velocity(
                qsnow, density, tvas, tvbs, mus, blins
            )

        if qgraupel > mpcons.QCMIN:
            pc_graupel = physfun.calc_particle_concentration(
                qgraupel, density, pcag, pcbg, mug
            )
            ed_graupel = physfun.calc_effective_diameter(
                qgraupel, density, edag, edbg, mug
            )
            oe_graupel = physfun.calc_optical_extinction(
                qgraupel, density, oeag, oebg, mug
            )
            rr_graupel = physfun.calc_radar_reflectivity(
                qgraupel, density, rrag, rrbg, mug
            )
            tv_graupel = physfun.calc_terminal_velocity(
                qgraupel, density, tvag, tvbg, mug, bling
            )


def update_temperature_pre_delp_q(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    ua: FloatField,
    va: FloatField,
    wa: FloatField,
    u0: FloatField,
    v0: FloatField,
    w0: FloatField,
    temperature: FloatField,
    tzuv: FloatField,
    tzw: FloatField,
):
    """
    momentum transportation during sedimentation
    update temperature before delp and q update
    """

    from __externals__ import c1_ice, c1_liq, c1_vap, c_air, do_sedi_uv, do_sedi_w

    with computation(PARALLEL), interval(...):
        if do_sedi_uv:
            c8 = (
                1.0
                + qvapor * c1_vap
                + (qliquid + qrain) * c1_liq
                + (qice + qsnow + qgraupel) * c1_ice
            ) * c_air
            tzuv = 0.5 * (u0**2 + v0**2 - (ua**2 + va**2)) / c8
            temperature += tzuv

        if do_sedi_w:
            c8 = (
                1.0
                + qvapor * c1_vap
                + (qliquid + qrain) * c1_liq
                + (qice + qsnow + qgraupel) * c1_ice
            ) * c_air
            tzw = 0.5 * (w0**2 - wa**2) / c8
            temperature += tzw


def convert_mass_mixing_to_specific_ratios_and_update_temperatures(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qcon: FloatField,
    cappa: FloatField,
    ua: FloatField,
    va: FloatField,
    wa: FloatField,
    delp: FloatField,
    qvapor0: FloatField,
    qliquid0: FloatField,
    qrain0: FloatField,
    qice0: FloatField,
    qsnow0: FloatField,
    qgraupel0: FloatField,
    u0: FloatField,
    v0: FloatField,
    w0: FloatField,
    dp0: FloatField,
    temperature: FloatField,
    tzuv: FloatField,
    tzw: FloatField,
    adj_vmr: FloatField,
):
    """
    Convert back from mass mixing ratios to specific ratios at the end of
    the microphysics call, calculate some other quantities to save, and
    update temperatures from sedimentation momentum after the delp and q
    conversions
    """
    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        c_air,
        do_inline_mp,
        do_sedi_uv,
        do_sedi_w,
    )

    with computation(PARALLEL), interval(...):
        # convert mass mixing ratios back to specific ratios
        if do_inline_mp:
            q_cond = qliquid + qrain + qice + qsnow + qgraupel
            con = 1.0 + qvapor + q_cond
        else:
            con = 1.0 + qvapor

        delp = delp * con
        con = 1.0 / con
        qvapor = qvapor * con
        qliquid = qliquid * con
        qrain = qrain * con
        qice = qice * con
        qsnow = qsnow * con
        qgraupel = qgraupel * con

        q1 = qvapor0 + qliquid0 + qrain0 + qice0 + qsnow0 + qgraupel0
        q2 = qvapor + qliquid + qrain + qice + qsnow + qgraupel
        adj_vmr = ((1.0 - q1) / (1.0 - q2)) / (1.0 + q2 - q1)

        # calculate some more variables needed outside
        q_liq = qliquid + qrain
        q_sol = qice + qsnow + qgraupel
        q_cond = q_liq + q_sol
        con_r8 = 1.0 - (qvapor + q_cond)
        c8 = (con_r8 + qvapor * c1_vap + q_liq * c1_liq + q_sol * c1_ice) * c_air

        # ifdef USE_COND
        qcon = q_cond
        # endif
        # ifdef MOIST_CAPPA
        tmp = constants.RDGAS * (1.0 + constants.ZVIR * qvapor)
        cappa = tmp / (tmp + c8)
        # endif

        # momentum transportation during sedimentation
        # update temperature after delp and q update
        if do_sedi_uv:
            temperature = temperature - tzuv
            tzuv = (
                (0.5 * (u0**2 + v0**2) * dp0 - 0.5 * (ua**2 + va**2) * delp) / c8 / delp
            )
            temperature = temperature + tzuv

        if do_sedi_w:
            temperature = temperature - tzw
            tzw = (0.5 * (w0**2) * dp0 - 0.5 * (wa**2) * delp) / c8 / delp
            temperature = temperature + tzw


def calculate_total_energy_change_and_convert_temp(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    te: FloatField,
    temperature: FloatField,
    temperature0: FloatField,
    delp: FloatField,
    delz: FloatField,
):
    from __externals__ import (
        c1_ice,
        c1_liq,
        c1_vap,
        c_air,
        consv_te,
        cp_heating,
        do_inline_mp,
        hydrostatic,
    )

    with computation(PARALLEL), interval(...):
        if consv_te:
            if hydrostatic:
                te = te + c_air * temperature * delp
            else:
                te = (
                    te
                    + physfun.calc_moist_total_energy(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        temperature,
                        delp,
                        True,
                    )
                    * constants.GRAV
                )

        # If microphysics is inlined in the dycore convert to virtual temperature,
        # otherwise update temp based on heat capacities
        if do_inline_mp:
            q_cond = qliquid + qrain + qice + qsnow + qgraupel
            if cp_heating:
                con_r8 = 1.0 - (qvapor + q_cond)
                c8 = (
                    con_r8
                    + qvapor * c1_vap
                    + (qliquid + qrain) * c1_liq
                    + (qice + qsnow + qgraupel) * c1_ice
                ) * c_air
                cp8 = (
                    con_r8 * constants.CP_AIR
                    + qvapor * constants.CP_VAP
                    + (qliquid + qrain) * mpcons.C_LIQ
                    + (qice + qsnow + qgraupel) * mpcons.C_ICE
                )
                delz = delz / temperature0
                temperature = (
                    temperature0
                    + (
                        temperature * ((1.0 + constants.ZVIR * qvapor) * (1.0 - q_cond))
                        - temperature0
                    )
                    * c8
                    / cp8
                )
                delz = delz * temperature
            else:
                temperature = temperature * (
                    (1.0 + constants.ZVIR * qvapor) * (1.0 - q_cond)
                )
        else:
            q_cond = qliquid + qrain + qice + qsnow + qgraupel
            con_r8 = 1.0 - (qvapor + q_cond)
            c8 = (
                con_r8
                + qvapor * c1_vap
                + (qliquid + qrain) * c1_liq
                + (qice + qsnow + qgraupel) * c1_ice
            ) * c_air
            temperature = (
                temperature0 + (temperature - temperature0) * c8 / constants.CP_AIR
            )


def total_energy_check(
    total_energy_dry_end,
    total_energy_moist_end,
    total_water_dry_end,
    total_water_moist_end,
    total_energy_dry_begin,
    total_energy_moist_begin,
    total_water_dry_begin,
    total_water_moist_begin,
    total_energy_bot_dry_end,
    total_energy_bot_moist_end,
    total_water_bot_dry_end,
    total_water_bot_moist_end,
    total_energy_bot_dry_begin,
    total_energy_bot_moist_begin,
    total_water_bot_dry_begin,
    total_water_bot_moist_begin,
    i_start,
    i_end,
    j_start,
    j_end,
    te_err,
    tw_err,
):
    """
    Checks the change in energy and water in a column at the end
    of the microphysics call
    """
    for i in range(i_start, i_end + 1):
        for j in range(j_start, j_end + 1):
            dry_energy_change = abs(
                sum(total_energy_dry_end[i, j, :])
                + total_energy_bot_dry_end[i, j]
                - sum(total_energy_dry_begin[i, j, :])
                - total_energy_bot_dry_begin[i, j]
            ) / (
                sum(total_energy_dry_begin[i, j, :]) + total_energy_bot_dry_begin[i, j]
            )
            if dry_energy_change > te_err:
                print(f"GFDL-MP-DRY TE: {dry_energy_change}")
            moist_energy_change = abs(
                sum(total_energy_moist_end[i, j, :])
                + total_energy_bot_moist_end[i, j]
                - sum(total_energy_moist_begin[i, j, :])
                - total_energy_bot_moist_begin[i, j]
            ) / (
                sum(total_energy_moist_begin[i, j, :])
                + total_energy_bot_moist_begin[i, j]
            )
            if moist_energy_change > te_err:
                print(f"GFDL-MP-MOIST TE: {moist_energy_change}")
            dry_water_change = abs(
                sum(total_water_dry_end[i, j, :])
                + total_water_bot_dry_end[i, j]
                - sum(total_water_dry_begin[i, j, :])
                - total_water_bot_dry_begin[i, j]
            ) / (sum(total_water_dry_begin[i, j, :]) + total_water_bot_dry_begin[i, j])
            if dry_water_change > tw_err:
                print(f"GFDL-MP-DRY TW: {dry_water_change}")
            moist_water_change = abs(
                sum(total_water_moist_end[i, j, :])
                + total_water_bot_moist_end[i, j]
                - sum(total_water_moist_begin[i, j, :])
                - total_water_bot_moist_begin[i, j]
            ) / (
                sum(total_water_moist_begin[i, j, :])
                + total_water_bot_moist_begin[i, j]
            )
            if moist_water_change > tw_err:
                print(f"GFDL-MP-MOIST TW: {moist_water_change}")


class GFDLCloudMicrophysics:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        grid_data: GridData,
        config: GFDLCloudMPConfig,
        full_timestep: float = None,
        do_mp_fast: bool = False,
        do_mp_full: bool = True,
        consv_te: bool = False,
    ):
        if full_timestep is None:
            full_timestep = config.dt_full
        self.config = config
        self._idx: GridIndexing = stencil_factory.grid_indexing
        self._max_timestep = self.config.mp_time
        self.do_mp_fast = do_mp_fast
        self.do_mp_full = do_mp_full
        self.consv_te = consv_te
        self.do_qa = config.do_qa
        self.hydrostatic = config.hydrostatic
        self.do_sedi_uv = config.do_sedi_uv
        self.do_sedi_w = config.do_sedi_w
        self.consv_checker = config.consv_checker
        self.do_inline_mp = config.do_inline_mp
        self.fix_negative = config.fix_negative
        self._te_err = config.te_err
        self._tw_err = config.tw_err

        if config.do_hail:
            pcag = config.pcah
            pcbg = config.pcbh
            edag = config.edah
            edbg = config.edbh
            oeag = config.oeah
            oebg = config.oebh
            rrag = config.rrah
            rrbg = config.rrbh
            tvag = config.tvah
            tvbg = config.tvbh
            mug = config.muh
            bling = config.blinh
        else:
            pcag = config.pcag
            pcbg = config.pcbg
            edag = config.edag
            edbg = config.edbg
            oeag = config.oeag
            oebg = config.oebg
            rrag = config.rrag
            rrbg = config.rrbg
            tvag = config.tvag
            tvbg = config.tvbg
            mug = config.mug
            bling = config.bling

        # allocate memory, compile stencils, etc.
        def make_quantity(**kwargs):
            return quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")

        def make_quantity2d(**kwargs):
            return quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")

        self._tot_energy_change = make_quantity2d()
        self._bottom_density = make_quantity2d()

        self._adj_vmr = quantity_factory.ones(
            dims=[I_DIM, J_DIM, K_DIM], units="unknown"
        )
        self._qvapor0 = make_quantity()
        self._qliquid0 = make_quantity()
        self._qrain0 = make_quantity()
        self._qice0 = make_quantity()
        self._qsnow0 = make_quantity()
        self._qgraupel0 = make_quantity()
        self._dp0 = make_quantity()
        self._temperature0 = make_quantity()
        self._u0 = make_quantity()
        self._v0 = make_quantity()
        self._density = make_quantity()
        self._density_factor = make_quantity()
        self._pz = make_quantity()
        self._cloud_condensation_nuclei = make_quantity()
        self._cloud_ice_nuclei = make_quantity()
        self._tzuv = make_quantity()
        self._tzw = make_quantity()

        if self.hydrostatic:
            assert (
                not config.do_sedi_w
            ), "Cannot do_sedi_w in a hydrostatic configuration"
        else:
            self._w0 = make_quantity()

        if self.consv_checker:
            self._column_vapor = make_quantity2d()  # Needed for mtetw
            self._column_energy_loss = make_quantity2d()
            self._total_energy_dry_end = make_quantity()
            self._total_energy_moist_end = make_quantity()
            self._total_water_dry_end = make_quantity()
            self._total_water_moist_end = make_quantity()
            self._total_energy_dry_begin = make_quantity()
            self._total_energy_moist_begin = make_quantity()
            self._total_water_dry_begin = make_quantity()
            self._total_water_moist_begin = make_quantity()
            self._total_energy_bot_dry_end = make_quantity()
            self._total_energy_bot_moist_end = make_quantity()
            self._total_water_bot_dry_end = make_quantity()
            self._total_water_bot_moist_end = make_quantity()
            self._total_energy_bot_dry_begin = make_quantity()
            self._total_energy_bot_moist_begin = make_quantity()
            self._total_water_bot_dry_begin = make_quantity()
            self._total_water_bot_moist_begin = make_quantity()

        self._h_var = make_quantity2d()
        self._rh_adj = make_quantity2d()
        self._rh_rain = make_quantity2d()
        self._cond = make_quantity2d()

        self._gsize = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="m")

        self._gsize[:] = np.sqrt(grid_data.area[:])

        self._update_timestep_if_needed(full_timestep)  # will change from dt_atmos
        # for inline microphysics

        self._convert_mm_day = 86400.0 * constants.RGRAV / self.config.dt_split

        self._copy_stencil = stencil_factory.from_origin_domain(
            copy,
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._reset_initial_values_and_make_copies = stencil_factory.from_origin_domain(
            func=reset_initial_values_and_make_copies,
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._convert_virtual_to_true_temperature_and_calc_total_energy = (
            stencil_factory.from_origin_domain(
                func=convert_virtual_to_true_temperature_and_calc_total_energy,
                externals={
                    "consv_te": bool(consv_te),
                    "do_inline_mp": self.do_inline_mp,
                    "hydrostatic": self.config.hydrostatic,
                    "c_air": self.config.c_air,
                    "c1_vap": self.config.c1_vap,
                    "c1_liq": self.config.c1_liq,
                    "c1_ice": self.config.c1_ice,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )
        )

        if self.config.consv_checker:
            self._moist_total_energy_and_water_mq = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": self.config.hydrostatic,
                    "moist_q": bool(True),
                    "c1_vap": self.config.c1_vap,
                    "c1_liq": self.config.c1_liq,
                    "c1_ice": self.config.c1_ice,
                    "lv00": self.config.lv00,
                    "li00": self.config.li00,
                    "c_air": self.config.c_air,
                    "timestep": self.config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._moist_total_energy_and_water = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": self.config.hydrostatic,
                    "moist_q": bool(False),
                    "c1_vap": self.config.c1_vap,
                    "c1_liq": self.config.c1_liq,
                    "c1_ice": self.config.c1_ice,
                    "lv00": self.config.lv00,
                    "li00": self.config.li00,
                    "c_air": self.config.c_air,
                    "timestep": self.config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._calc_sedimentation_energy_loss = stencil_factory.from_origin_domain(
                func=calc_sedimentation_energy_loss,
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

        self._convert_specific_to_mass_mixing_ratios_and_calculate_densities = (
            stencil_factory.from_origin_domain(
                func=convert_specific_to_mass_mixing_ratios_and_calculate_densities,
                externals={
                    "do_inline_mp": self.config.do_inline_mp,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )
        )

        self._calculate_density_factor = stencil_factory.from_origin_domain(
            func=calculate_density_factor,
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._cloud_nuclei_subgrid_and_relative_humidity = (
            stencil_factory.from_origin_domain(
                func=cloud_nuclei_subgrid_and_relative_humidity,
                externals={
                    "prog_ccn": self.config.prog_ccn,
                    "ccn_l": self.config.ccn_l,
                    "ccn_o": self.config.ccn_o,
                    "dw_land": self.config.dw_land,
                    "dw_ocean": self.config.dw_ocean,
                    "rh_inc": self.config.rh_inc,
                    "rh_inr": self.config.rh_inr,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )
        )

        if self.fix_negative:
            self._neg_adj = AdjustNegativeTracers(
                stencil_factory,
                self.config.adjustnegative,
                self._convert_mm_day,
            )

        if self.do_mp_fast:
            self._mp_fast = FastMicrophysics(
                stencil_factory,
                self.config.fastmp,
                self.config.dt_full,
                self._convert_mm_day,
            )

        if self.do_mp_full:
            self._mp_full = FullMicrophysics(
                stencil_factory,
                quantity_factory,
                config,
                self.config.dt_split,
                self._convert_mm_day,
            )

        if self.do_qa:
            self._cloud_fraction = CloudFraction(stencil_factory, config)

        self._calculate_particle_properties = stencil_factory.from_origin_domain(
            func=calculate_particle_properties,
            externals={
                "pcaw": config.pcaw,
                "pcbw": config.pcbw,
                "pcai": config.pcai,
                "pcbi": config.pcbi,
                "pcar": config.pcar,
                "pcbr": config.pcbr,
                "pcas": config.pcas,
                "pcbs": config.pcbs,
                "pcag": pcag,
                "pcbg": pcbg,
                "edaw": config.edaw,
                "edbw": config.edbw,
                "edai": config.edai,
                "edbi": config.edbi,
                "edar": config.edar,
                "edbr": config.edbr,
                "edas": config.edas,
                "edbs": config.edbs,
                "edag": edag,
                "edbg": edbg,
                "oeaw": config.oeaw,
                "oebw": config.oebw,
                "oeai": config.oeai,
                "oebi": config.oebi,
                "oear": config.oear,
                "oebr": config.oebr,
                "oeas": config.oeas,
                "oebs": config.oebs,
                "oeag": oeag,
                "oebg": oebg,
                "rraw": config.rraw,
                "rrbw": config.rrbw,
                "rrai": config.rrai,
                "rrbi": config.rrbi,
                "rrar": config.rrar,
                "rrbr": config.rrbr,
                "rras": config.rras,
                "rrbs": config.rrbs,
                "rrag": rrag,
                "rrbg": rrbg,
                "tvaw": config.tvaw,
                "tvbw": config.tvbw,
                "tvai": config.tvai,
                "tvbi": config.tvbi,
                "tvar": config.tvar,
                "tvbr": config.tvbr,
                "tvas": config.tvas,
                "tvbs": config.tvbs,
                "tvag": tvag,
                "tvbg": tvbg,
                "muw": config.muw,
                "mui": config.mui,
                "mur": config.mur,
                "mus": config.mus,
                "mug": mug,
                "blinw": config.blinw,
                "blini": config.blini,
                "blinr": config.blinr,
                "blins": config.blins,
                "bling": bling,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        if (self.do_sedi_uv) or (self.do_sedi_w):
            self._update_temperature_pre_delp_q = stencil_factory.from_origin_domain(
                func=update_temperature_pre_delp_q,
                externals={
                    "do_sedi_uv": self.do_sedi_uv,
                    "do_sedi_w": self.do_sedi_w,
                    "c_air": config.c_air,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

        self._convert_mass_mixing_to_specific_ratios_and_update_temperatures = (
            stencil_factory.from_origin_domain(
                func=convert_mass_mixing_to_specific_ratios_and_update_temperatures,
                externals={
                    "do_inline_mp": config.do_inline_mp,
                    "c_air": config.c_air,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "do_sedi_uv": config.do_sedi_uv,
                    "do_sedi_w": config.do_sedi_w,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )
        )

        self._calculate_total_energy_change_and_convert_temp = (
            stencil_factory.from_origin_domain(
                func=calculate_total_energy_change_and_convert_temp,
                externals={
                    "consv_te": consv_te,
                    "hydrostatic": self.hydrostatic,
                    "c_air": config.c_air,
                    "do_inline_mp": config.do_inline_mp,
                    "cp_heating": config.cp_heating,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )
        )

    def _update_timestep_if_needed(self, timestep: float):
        if (timestep != self.config.dt_full) or (timestep > self._max_timestep):
            self._set_timestepping(timestep)

    def _set_timestepping(self, full_timestep: float):
        """
        sets full and split timesteps
        full_timestep is equivalent to dtm
        split_timestep is equivalent to dts
        """
        self.config.ntimes = int(
            max(
                self.config.ntimes,
                full_timestep / min(full_timestep, self._max_timestep),
            )
        )
        self.config.dt_split = full_timestep / self.config.ntimes
        self.config.dt_full = full_timestep

    def __call__(
        self,
        state: GFDLCloudMicrophysicsState,
        last_step: Bool = True,
        timer: Timer | None = None,
    ):
        if timer is None:
            timer = NullTimer()

        self._reset_initial_values_and_make_copies(
            state.adj_vmr,
            state.qvapor,
            state.qliquid,
            state.qrain,
            state.qice,
            state.qsnow,
            state.qgraupel,
            state.delp,
            state.pt,
            state.ua,
            state.va,
            self._qvapor0,
            self._qliquid0,
            self._qrain0,
            self._qice0,
            self._qsnow0,
            self._qgraupel0,
            self._dp0,
            self._temperature0,
            self._u0,
            self._v0,
            state.column_energy_change,
            self._cond,
        )

        if not self.hydrostatic:
            self._copy_stencil(state.wa, self._w0)

        if self.do_inline_mp or self.consv_te:
            self._convert_virtual_to_true_temperature_and_calc_total_energy(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
            )

        if self.consv_checker:
            self._moist_total_energy_and_water_mq(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.ua,
                state.va,
                state.wa,
                state.delp,
                self._gsize,
                state.column_energy_change,
                self._column_vapor,
                state.column_water,
                state.column_rain,
                state.column_ice,
                state.column_snow,
                state.column_graupel,
                0.0,
                0.0,
                self._total_energy_moist_begin,
                self._total_water_moist_begin,
                self._total_energy_bot_moist_begin,
                self._total_water_bot_moist_begin,
            )

        self._convert_specific_to_mass_mixing_ratios_and_calculate_densities(
            state.qvapor,
            state.qliquid,
            state.qrain,
            state.qice,
            state.qsnow,
            state.qgraupel,
            state.delp,
            state.delz,
            state.pt,
            self._density,
            self._pz,
            self._bottom_density,
        )

        self._calculate_density_factor(
            self._density,
            self._density_factor,
            self._bottom_density,
        )

        if self.consv_checker:
            self._moist_total_energy_and_water(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.ua,
                state.va,
                state.wa,
                state.delp,
                self._gsize,
                state.column_energy_change,
                self._column_vapor,
                state.column_water,
                state.column_rain,
                state.column_ice,
                state.column_snow,
                state.column_graupel,
                0.0,
                0.0,
                self._total_energy_dry_begin,
                self._total_water_dry_begin,
                self._total_energy_bot_dry_begin,
                self._total_water_bot_dry_begin,
            )

        self._cloud_nuclei_subgrid_and_relative_humidity(
            state.geopotential_surface_height,
            state.qcloud_cond_nuclei,
            state.qcloud_ice_nuclei,
            self._density,
            self._cloud_condensation_nuclei,
            self._cloud_ice_nuclei,
            self._gsize,
            self._h_var,
            self._rh_adj,
            self._rh_rain,
        )

        if self.fix_negative:
            self._neg_adj(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.delp,
                state.condensation,
            )

        if self.do_mp_fast:
            self._mp_fast(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.delp,
                self._density,
                self._density_factor,
                self._cloud_condensation_nuclei,
                self._cloud_ice_nuclei,
                state.condensation,
                state.deposition,
                state.evaporation,
                state.sublimation,
                last_step,
            )

        if self.do_mp_full:
            self._mp_full(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.ua,
                state.va,
                state.wa,
                state.pt,
                state.delp,
                state.delz,
                self._density,
                self._density_factor,
                self._cloud_condensation_nuclei,
                self._cloud_ice_nuclei,
                state.preflux_water,
                state.preflux_rain,
                state.preflux_ice,
                state.preflux_snow,
                state.preflux_graupel,
                self._h_var,
                self._rh_adj,
                state.column_energy_change,
                state.column_water,
                state.column_rain,
                state.column_ice,
                state.column_snow,
                state.column_graupel,
                state.condensation,
                state.deposition,
                state.evaporation,
                state.sublimation,
                last_step,
            )

        if (self.do_qa) and last_step:
            self._cloud_fraction(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.qcld,
                state.pt,
                self._density,
                self._pz,
                self._h_var,
                self._gsize,
            )

        self._calculate_particle_properties(
            state.qliquid,
            state.qrain,
            state.qice,
            state.qsnow,
            state.qgraupel,
            self._density,
            state.particle_concentration_w,
            state.particle_concentration_r,
            state.particle_concentration_i,
            state.particle_concentration_s,
            state.particle_concentration_g,
            state.effective_diameter_w,
            state.effective_diameter_r,
            state.effective_diameter_i,
            state.effective_diameter_s,
            state.effective_diameter_g,
            state.optical_extinction_w,
            state.optical_extinction_r,
            state.optical_extinction_i,
            state.optical_extinction_s,
            state.optical_extinction_g,
            state.radar_reflectivity_w,
            state.radar_reflectivity_r,
            state.radar_reflectivity_i,
            state.radar_reflectivity_s,
            state.radar_reflectivity_g,
            state.terminal_velocity_w,
            state.terminal_velocity_r,
            state.terminal_velocity_i,
            state.terminal_velocity_s,
            state.terminal_velocity_g,
        )

        if (self.do_sedi_uv) or (self.do_sedi_w):
            self._update_temperature_pre_delp_q(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.ua,
                state.va,
                state.wa,
                self._u0,
                self._v0,
                self._w0,
                state.pt,
                self._tzuv,
                self._tzw,
            )
        if self.consv_checker:
            self._moist_total_energy_and_water(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.ua,
                state.va,
                state.wa,
                state.delp,
                self._gsize,
                state.column_energy_change,
                self._column_vapor,
                state.column_water,
                state.column_rain,
                state.column_ice,
                state.column_snow,
                state.column_graupel,
                0.0,
                0.0,
                self._total_energy_dry_end,
                self._total_water_dry_end,
                self._total_energy_bot_dry_end,
                self._total_water_bot_dry_end,
            )

            self._calc_sedimentation_energy_loss(
                self._column_energy_loss, state.column_energy_change, self._gsize
            )

        self._convert_mass_mixing_to_specific_ratios_and_update_temperatures(
            state.qvapor,
            state.qliquid,
            state.qrain,
            state.qice,
            state.qsnow,
            state.qgraupel,
            state.qcon,
            state.cappa,
            state.ua,
            state.va,
            state.wa,
            state.delp,
            self._qvapor0,
            self._qliquid0,
            self._qrain0,
            self._qice0,
            self._qsnow0,
            self._qgraupel0,
            self._u0,
            self._v0,
            self._w0,
            self._dp0,
            state.pt,
            self._tzuv,
            self._tzw,
            state.adj_vmr,
        )

        if self.consv_checker:
            self._moist_total_energy_and_water_mq(
                state.qvapor,
                state.qliquid,
                state.qrain,
                state.qice,
                state.qsnow,
                state.qgraupel,
                state.pt,
                state.ua,
                state.va,
                state.wa,
                state.delp,
                self._gsize,
                state.column_energy_change,
                self._column_vapor,
                state.column_water,
                state.column_rain,
                state.column_ice,
                state.column_snow,
                state.column_graupel,
                0.0,
                0.0,
                self._total_energy_moist_end,
                self._total_water_moist_end,
                self._total_energy_bot_moist_end,
                self._total_water_bot_moist_end,
            )

        self._calculate_total_energy_change_and_convert_temp(
            state.qvapor,
            state.qliquid,
            state.qrain,
            state.qice,
            state.qsnow,
            state.qgraupel,
            state.total_energy,
            state.pt,
            self._temperature0,
            state.delp,
            state.delz,
        )

        if self.consv_checker:
            total_energy_check(
                self._total_energy_dry_end,
                self._total_energy_moist_end,
                self._total_water_dry_end,
                self._total_water_moist_end,
                self._total_energy_dry_begin,
                self._total_energy_moist_begin,
                self._total_water_dry_begin,
                self._total_water_moist_begin,
                self._total_energy_bot_dry_end,
                self._total_energy_bot_moist_end,
                self._total_water_bot_dry_end,
                self._total_water_bot_moist_end,
                self._total_energy_bot_dry_begin,
                self._total_energy_bot_moist_begin,
                self._total_water_bot_dry_begin,
                self._total_water_bot_moist_begin,
                self._idx.isc,
                self._idx.iec,
                self._idx.jsc,
                self._idx.jec,
                self._te_err,
                self._tw_err,
            )
