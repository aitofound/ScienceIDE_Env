from typing import Literal

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons
import pyshield.stencils.gfdl_cld_microphysics.physical_functions as physfun
from ndsl import GridIndexing, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ, IntFieldIJ
from ndsl.stencils import copy
from pyfv3.stencils.remap_profile import RemapProfile
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig


def prep_terminal_fall(
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
    from __externals__ import do_sedi_w

    with computation(BACKWARD), interval(...):
        if q_fall > mpcons.QFMIN:
            no_fall = 0.0
    with computation(FORWARD), interval(...):
        if no_fall == 0.0:
            if do_sedi_w:
                dm = delp * (1.0 + qvapor + qliquid + qrain + qice + qsnow + qgraupel)
            tot_e_initial += physfun.calc_moist_total_energy(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                delp,
                False,
            )


def fall_implicit(
    q_fall: FloatField,
    v_terminal: FloatField,
    delp: FloatField,
    z_edge: FloatField,
    no_fall: FloatFieldIJ,
    flux: FloatField,
    precip: FloatFieldIJ,
):
    from __externals__ import timestep

    with computation(FORWARD):
        with interval(...):
            if no_fall == 0.0:
                delz = z_edge - z_edge[0, 0, 1]
                dd = timestep * v_terminal
                q_fall = q_fall * delp

    with computation(FORWARD):
        with interval(0, 1):
            if no_fall == 0.0:
                qm = q_fall / (delz + dd)

        with interval(1, None):
            if no_fall == 0.0:
                qm = (q_fall + qm[0, 0, -1] * dd[0, 0, -1]) / (delz + dd)

    with computation(FORWARD):
        with interval(...):
            if no_fall == 0.0:
                qm = qm * delz

    with computation(FORWARD):
        with interval(0, 1):
            if no_fall == 0.0:
                flux = q_fall - qm

        with interval(1, None):
            if no_fall == 0.0:
                flux = flux[0, 0, -1] + q_fall - qm

    with computation(FORWARD):
        with interval(-1, None):
            if no_fall == 0.0:
                precip += flux

    with computation(FORWARD):
        with interval(...):
            if no_fall == 0.0:
                q_fall = qm / delp


def pre_lagrangian(
    q: FloatField,
    delz: FloatField,
    q4_1: FloatField,
    delp: FloatField,
    z_terminal: FloatField,
    no_fall: FloatFieldIJ,
):
    with computation(PARALLEL), interval(...):
        if no_fall == 0.0:
            delz = z_terminal - z_terminal[0, 0, 1]
            q *= delp
            q4_1 = q / delz


def fall_lagrangian(
    q: FloatField,
    z_edge: FloatField,
    z_terminal: FloatField,
    q4_1: FloatField,
    q4_2: FloatField,
    q4_3: FloatField,
    q4_4: FloatField,
    delz: FloatField,
    delp: FloatField,
    flux: FloatField,
    precipitation: FloatFieldIJ,
    lev: IntFieldIJ,
    no_fall: FloatFieldIJ,
):
    """
    equivalent to MapSingle's lagrangian_contributions stencil
    but using height instead of pressure
    Args:
        q (out):
        z_edge (in):
        z_terminal (in):
        q4_1 (in):
        q4_2 (in):
        q4_3 (in):
        q4_4 (in):
        delp (in):
        delz (in):
        qm (out):
        flux (out)
        precipitation (inout):
        lev (inout):
        no_fall (in):
    """
    # TODO: Can we make lev a 2D temporary?
    with computation(FORWARD), interval(...):
        if no_fall == 0.0:
            pl = (z_terminal[0, 0, lev] - z_edge) / delz[0, 0, lev]
            if z_terminal[0, 0, lev + 1] <= z_edge[0, 0, 1]:
                pr = (z_terminal[0, 0, lev] - z_edge[0, 0, 1]) / delz[0, 0, lev]
                qm = (
                    q4_2[0, 0, lev]
                    + 0.5
                    * (q4_4[0, 0, lev] + q4_3[0, 0, lev] - q4_2[0, 0, lev])
                    * (pr + pl)
                    - q4_4[0, 0, lev] * 1.0 / 3.0 * (pr * (pr + pl) + pl * pl)
                )
                qm = qm * (z_edge - z_edge[0, 0, 1])
            else:
                qm = (z_edge - z_terminal[0, 0, lev + 1]) * (
                    q4_2[0, 0, lev]
                    + 0.5
                    * (q4_4[0, 0, lev] + q4_3[0, 0, lev] - q4_2[0, 0, lev])
                    * (1.0 + pl)
                    - q4_4[0, 0, lev] * (1.0 / 3.0 * (1.0 + pl * (1.0 + pl)))
                )
                lev = lev + 1
                while z_edge[0, 0, 1] < z_terminal[0, 0, lev + 1]:
                    qm += q[0, 0, lev]
                    lev = lev + 1
                dz = z_terminal[0, 0, lev] - z_edge[0, 0, 1]
                esl = dz / delz[0, 0, lev]
                qm += dz * (
                    q4_2[0, 0, lev]
                    + 0.5
                    * esl
                    * (
                        q4_3[0, 0, lev]
                        - q4_2[0, 0, lev]
                        + q4_4[0, 0, lev] * (1.0 - (2.0 / 3.0) * esl)
                    )
                )
            lev = lev - 1

    with computation(BACKWARD), interval(0, 1):
        if no_fall == 0.0:
            flux = q - qm
            q = qm / delp

    with computation(FORWARD), interval(1, None):
        if no_fall == 0.0:
            flux = flux[0, 0, -1] + q - qm
            q = qm / delp

    with computation(FORWARD), interval(-1, None):
        if no_fall == 0.0:
            precipitation += flux


def finish_implicit_lagrangian(
    q: FloatField,
    q_post_implicit: FloatField,
    q_post_lagrangian: FloatField,
    flux: FloatField,
    m_post_implicit: FloatField,
    m_post_lagrangian: FloatField,
    precipitation: FloatFieldIJ,
    precipitation_post_implicit: FloatFieldIJ,
    precipitation_post_lagrangian: FloatFieldIJ,
    no_fall: FloatFieldIJ,
):
    from __externals__ import sed_fac

    with computation(PARALLEL), interval(...):
        if no_fall == 0.0:
            q = q_post_implicit * sed_fac + q_post_lagrangian * (1.0 - sed_fac)
            flux = m_post_implicit * sed_fac + m_post_lagrangian * (1.0 - sed_fac)
    with computation(FORWARD), interval(-1, None):
        if no_fall == 0.0:
            precipitation = (
                precipitation_post_implicit * sed_fac
                + precipitation_post_lagrangian * (1.0 - sed_fac)
            )


def update_energy_wind_heat_post_fall(
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

    from __externals__ import cw, do_sedi_heat, do_sedi_uv, do_sedi_w

    # TODO: tmp_energy2 should be a 2d temporary
    with computation(FORWARD), interval(...):
        if no_fall == 0.0:
            tmp_energy2 += physfun.calc_moist_total_energy(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                delp,
                False,
            )

    with computation(FORWARD), interval(-1, None):
        column_energy_change = column_energy_change + tmp_energy1 - tmp_energy2
        tmp_energy1 = 0.0
        tmp_energy2 = 0.0

    with computation(FORWARD), interval(1, None):
        if do_sedi_uv:
            if no_fall == 0.0:
                ua = (delp * ua + flux[0, 0, -1] * ua[0, 0, -1]) / (
                    delp + flux[0, 0, -1]
                )
                va = (delp * va + flux[0, 0, -1] * va[0, 0, -1]) / (
                    delp + flux[0, 0, -1]
                )

    with computation(FORWARD):
        with interval(0, 1):
            if do_sedi_w:
                if no_fall == 0.0:
                    wa = wa + flux * v_terminal / dm

        with interval(1, None):
            if do_sedi_w:
                if no_fall == 0.0:
                    wa = (
                        dm * wa
                        + flux[0, 0, -1] * (wa[0, 0, -1] - v_terminal[0, 0, -1])
                        + flux * v_terminal
                    ) / (dm + flux[0, 0, -1])

    # energy change during sedimentation heating
    with computation(FORWARD), interval(...):
        if no_fall == 0.0:
            tmp_energy1 += physfun.calc_moist_total_energy(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                delp,
                False,
            )

    # sedi_heat
    with computation(FORWARD), interval(1, None):
        if do_sedi_heat:
            if no_fall == 0.0:
                dgz = -0.5 * constants.GRAV * (delz[0, 0, -1] + delz)
                cv0 = dm * (
                    constants.CV_AIR
                    + qvapor * constants.CV_VAP
                    + (qrain + qliquid) * mpcons.C_LIQ
                    + (qice + qsnow + qgraupel) * mpcons.C_ICE
                ) + cw * (flux - flux[0, 0, -1])

    with computation(FORWARD), interval(1, None):
        if do_sedi_heat:
            if no_fall == 0.0:
                temperature = (
                    cv0 * temperature
                    + flux[0, 0, -1] * (cw * temperature[0, 0, -1] + dgz)
                ) / (cv0 + cw * flux[0, 0, -1])

    # energy change during sedimentation heating
    with computation(FORWARD), interval(...):
        if no_fall == 0.0:
            tmp_energy2 += physfun.calc_moist_total_energy(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                temperature,
                delp,
                False,
            )

    with computation(FORWARD), interval(-1, None):
        if no_fall == 0.0:
            column_energy_change = column_energy_change + tmp_energy1 - tmp_energy2


class TerminalFall:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: GFDLCloudMPConfig,
        timestep: float,
    ):
        self._sedflag = config.sedflag
        assert self._sedflag in [
            1,
            2,
            3,
            4,
        ], f"sedimentation flag {self._sedflag} must be 1, 2, 3, or 4"

        if self._sedflag == 2:
            raise NotImplementedError(
                f"sedimentation flag {self._sedflag}: "
                "Explicit Fall has not been implemented"
            )

        self._idx: GridIndexing = stencil_factory.grid_indexing

        self._timestep = timestep
        dims = [I_DIM, J_DIM, K_DIM]

        # allocate internal storages
        self._q_fall = quantity_factory.zeros(dims=dims, units="unknown")
        self._no_fall = quantity_factory.ones(dims=[I_DIM, J_DIM], units="unknown")
        self._dm = quantity_factory.zeros(dims=dims, units="Pa")
        self._tmp_energy1 = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")
        self._tmp_energy2 = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")

        if (self._sedflag == 3) or (self._sedflag == 4):
            self._q4_1 = quantity_factory.zeros(dims=dims, units="unknown")
            self._q4_2 = quantity_factory.zeros(dims=dims, units="unknown")
            self._q4_3 = quantity_factory.zeros(dims=dims, units="unknown")
            self._q4_4 = quantity_factory.zeros(dims=dims, units="unknown")
            self._lev = quantity_factory.zeros([I_DIM, J_DIM], units="", dtype=int)

        if self._sedflag == 4:
            self._m0 = quantity_factory.zeros(dims=dims, units="unknown")
            self._m1 = quantity_factory.zeros(dims=dims, units="unknown")
            # Need extra qs and precips for the sed_fac calculation
            self._q0 = quantity_factory.zeros(dims=dims, units="unknown")
            self._q1 = quantity_factory.zeros(dims=dims, units="unknown")
            self._precip0 = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")
            self._precip1 = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")

        # compile stencils

        self._copy_stencil = stencil_factory.from_dims_halo(
            copy,
            compute_dims=dims,
        )

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

        if (self._sedflag == 1) or (self._sedflag == 4):
            self._fall_implicit = stencil_factory.from_dims_halo(
                func=fall_implicit,
                externals={"timestep": timestep},
                compute_dims=dims,
            )

        if (self._sedflag == 3) or (self._sedflag == 4):
            self._pre_lagrangian = stencil_factory.from_dims_halo(
                func=pre_lagrangian,
                compute_dims=dims,
            )

            self._remap_profile = RemapProfile(
                stencil_factory,
                quantity_factory,
                kord=9,
                iv=0,
                dims=dims,
            )

            self._fall_lagrangian = stencil_factory.from_dims_halo(
                fall_lagrangian,
                compute_dims=dims,
            )

        if self._sedflag == 4:
            self._finish_implicit_lagrangian = stencil_factory.from_dims_halo(
                finish_implicit_lagrangian,
                externals={"sed_fac": config.sed_fac},
                compute_dims=dims,
            )

        self._update_energy_wind_heat_post_fall = stencil_factory.from_dims_halo(
            update_energy_wind_heat_post_fall,
            externals={
                "do_sedi_uv": config.do_sedi_uv,
                "do_sedi_w": config.do_sedi_w,
                "do_sedi_heat": config.do_sedi_heat,
                "cw": mpcons.C_ICE,
                "c1_ice": config.c1_ice,
                "c1_liq": config.c1_liq,
                "c1_vap": config.c1_vap,
                "c_air": config.c_air,
            },
            compute_dims=dims,
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
        v_terminal: FloatField,
        z_edge: FloatField,
        z_terminal: FloatField,
        flux: FloatField,
        precipitation: FloatFieldIJ,
        column_energy_change: FloatFieldIJ,
        tracer: Literal["liquid", "rain", "ice", "snow", "graupel"],
    ):
        """
        Executes sedimentation for the named tracer.
        Args:
            qvapor (inout): vapor mass mixing ratio
            qliquid (inout): cloud water mass mixing ratio
            qrain (inout): rain mass mixing ratio
            qice (inout): cloud ice mass mixing ratio
            qsnow (inout): snow mass mixing ratio
            qgraupel (inout): graupel mass mixing ratio
            ua (inout): a-grid u wind
            va (inout): a-grid v wind
            wa (inout): a-grid w wind
            temperature (inout):
            delp (in): grid level pressure thickness
            delz (in): grid level height difference
            v_terminal (in): tracer terminal velocity
            z_edge (in): grid level edge heights
            z_terminal (in): height reachable at terminal velocity from each grid level
            flux (inout): flux through each grid level
            precipitation (inout): total tracer amount precipitated to ground
            column_energy_change (inout): column energy change due to sedimentation
            tracer (in): which tracer to sediment down
        """
        # TODO: is it best to copy one q into q_fall or to pass it in separately?
        if tracer == "liquid":
            self._copy_stencil(qliquid, self._q_fall)
        elif tracer == "rain":
            self._copy_stencil(qrain, self._q_fall)
        elif tracer == "ice":
            self._copy_stencil(qice, self._q_fall)
        elif tracer == "snow":
            self._copy_stencil(qsnow, self._q_fall)
        elif tracer == "graupel":
            self._copy_stencil(qgraupel, self._q_fall)

        self._prep_terminal_fall(
            self._q_fall,
            delp,
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            self._dm,
            self._tmp_energy1,
            self._no_fall,
        )

        if self._sedflag == 1:
            self._fall_implicit(
                self._q_fall,
                v_terminal,
                delp,
                z_edge,
                self._no_fall,
                flux,
                precipitation,
            )
        elif self._sedflag == 2:
            raise NotImplementedError("explicit fall has not been implemented")
        elif self._sedflag == 3:
            self._fall_lagrangian(
                self._q_fall,
                z_edge,
                z_terminal,
                self._q4_1,
                self._q4_2,
                self._q4_3,
                self._q4_4,
                delz,
                delp,
                flux,
                precipitation,
                self._lev,
                self._no_fall,
            )
        elif self._sedflag == 4:
            self._copy_stencil(self._q_fall, self._q0)
            self._copy_stencil(precipitation, self._precip0)

            self._fall_implicit(
                self._q0,
                v_terminal,
                delp,
                z_edge,
                self._no_fall,
                self._m0,
                self._precip0,
            )

            self._copy_stencil(self._q_fall, self._q1)
            self._copy_stencil(precipitation, self._precip1)

            self._fall_lagrangian(
                self._q1,
                z_edge,
                z_terminal,
                self._q4_1,
                self._q4_2,
                self._q4_3,
                self._q4_4,
                delz,
                delp,
                self._m1,
                self._precip1,
                self._lev,
                self._no_fall,
            )

            self._finish_implicit_lagrangian(
                self._q_fall,
                self._q0,
                self._q1,
                flux,
                self._m0,
                self._m1,
                precipitation,
                self._precip0,
                self._precip1,
                self._no_fall,
            )

        else:
            raise ValueError(
                f"Sedimentation flag must be 1, 2, 3 or 4, got {self._sedflag}"
            )

        if tracer == "liquid":
            self._copy_stencil(self._q_fall, qliquid)
        elif tracer == "rain":
            self._copy_stencil(self._q_fall, qrain)
        elif tracer == "ice":
            self._copy_stencil(self._q_fall, qice)
        elif tracer == "snow":
            self._copy_stencil(self._q_fall, qsnow)
        elif tracer == "graupel":
            self._copy_stencil(self._q_fall, qgraupel)

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
            self._dm,
            v_terminal,
            self._tmp_energy1,
            self._tmp_energy2,
            column_energy_change,
            self._no_fall,
        )
