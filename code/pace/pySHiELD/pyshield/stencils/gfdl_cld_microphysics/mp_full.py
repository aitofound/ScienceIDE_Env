from ndsl import GridIndexing, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import FORWARD, computation, interval
from ndsl.dsl.typing import Bool, FloatField, FloatFieldIJ
from pyshield.stencils.gfdl_cld_microphysics._config import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.ice_cloud import IceCloud
from pyshield.stencils.gfdl_cld_microphysics.sedimentation import Sedimentation
from pyshield.stencils.gfdl_cld_microphysics.subgrid_z_proc import (
    VerticalSubgridProcesses,
)
from pyshield.stencils.gfdl_cld_microphysics.warm_rain import WarmRain


def add_fluxes_and_surface_tracers(
    prefluxw: FloatField,
    prefluxr: FloatField,
    prefluxi: FloatField,
    prefluxs: FloatField,
    prefluxg: FloatField,
    pfw: FloatField,
    pfr: FloatField,
    pfi: FloatField,
    pfs: FloatField,
    pfg: FloatField,
    water: FloatFieldIJ,
    rain: FloatFieldIJ,
    ice: FloatFieldIJ,
    snow: FloatFieldIJ,
    graupel: FloatFieldIJ,
    evaporation: FloatFieldIJ,
    w1: FloatFieldIJ,
    r1: FloatFieldIJ,
    i1: FloatFieldIJ,
    s1: FloatFieldIJ,
    g1: FloatFieldIJ,
    reevap: FloatFieldIJ,
):
    from __externals__ import convt

    with computation(FORWARD), interval(...):
        prefluxw += pfw * convt
        prefluxr += pfr * convt
        prefluxi += pfi * convt
        prefluxs += pfs * convt
        prefluxg += pfg * convt
    with computation(FORWARD), interval(-1, None):
        water += w1 * convt
        rain += r1 * convt
        ice += i1 * convt
        snow += s1 * convt
        graupel += g1 * convt
        evaporation += reevap * convt


def accumulate_state_changes(
    cond: FloatFieldIJ,
    dep: FloatFieldIJ,
    reevap: FloatFieldIJ,
    sub: FloatFieldIJ,
    condensation: FloatFieldIJ,
    deposition: FloatFieldIJ,
    evaporation: FloatFieldIJ,
    sublimation: FloatFieldIJ,
):
    from __externals__ import convt

    with computation(FORWARD), interval(-1, None):
        condensation += cond * convt
        deposition += dep * convt
        evaporation += reevap * convt
        sublimation += sub * convt


class FullMicrophysics:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: GFDLCloudMPConfig,
        timestep: float,
        convert_mm_day: float,
    ):

        self._idx: GridIndexing = stencil_factory.grid_indexing

        self._do_warm_rain = config.do_warm_rain_mp
        self._ntimes = config.ntimes

        def make_quantity():
            return quantity_factory.zeros([I_DIM, J_DIM, K_DIM], units="unknown")

        def make_quantity_2D():
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown")

        self._fluxw = make_quantity()
        self._fluxr = make_quantity()
        self._fluxi = make_quantity()
        self._fluxs = make_quantity()
        self._fluxg = make_quantity()

        self._vtw = make_quantity()
        self._vtr = make_quantity()
        self._vti = make_quantity()
        self._vts = make_quantity()
        self._vtg = make_quantity()

        self._w1 = make_quantity_2D()
        self._r1 = make_quantity_2D()
        self._i1 = make_quantity_2D()
        self._s1 = make_quantity_2D()
        self._g1 = make_quantity_2D()

        self._cond = make_quantity_2D()
        self._dep = make_quantity_2D()
        self._reevap = make_quantity_2D()
        self._sub = make_quantity_2D()

        self._sedimentation = Sedimentation(
            stencil_factory,
            quantity_factory,
            config,
            timestep,
        )

        self._add_fluxes_and_surface_tracers = stencil_factory.from_origin_domain(
            func=add_fluxes_and_surface_tracers,
            externals={
                "convt": convert_mm_day,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )

        self._warm_rain = WarmRain(stencil_factory, config, timestep)

        if not self._do_warm_rain:
            self._ice_cloud = IceCloud(stencil_factory, config, timestep)

        self._subgrid_z_proc = VerticalSubgridProcesses(
            stencil_factory, config, timestep
        )

        self._accumulate_state_changes = stencil_factory.from_origin_domain(
            func=accumulate_state_changes,
            externals={
                "convt": convert_mm_day,
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
        ua: FloatField,
        va: FloatField,
        wa: FloatField,
        temperature: FloatField,
        delp: FloatField,
        delz: FloatField,
        density: FloatField,
        density_factor: FloatField,
        cloud_condensation_nuclei: FloatField,
        cloud_ice_nuclei: FloatField,
        preflux_water: FloatField,
        preflux_rain: FloatField,
        preflux_ice: FloatField,
        preflux_snow: FloatField,
        preflux_graupel: FloatField,
        h_var: FloatFieldIJ,
        rh_adj: FloatFieldIJ,
        column_energy_change: FloatFieldIJ,
        surface_water: FloatFieldIJ,
        surface_rain: FloatFieldIJ,
        surface_ice: FloatFieldIJ,
        surface_snow: FloatFieldIJ,
        surface_graupel: FloatFieldIJ,
        condensation: FloatFieldIJ,
        deposition: FloatFieldIJ,
        evaporation: FloatFieldIJ,
        sublimation: FloatFieldIJ,
        last_step: Bool,
    ):
        """
        Full Microphysics Loop
        executes ntimes:
        """
        for _i in range(self._ntimes):
            self._sedimentation(
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
                density,
                density_factor,
                self._fluxw,
                self._fluxr,
                self._fluxi,
                self._fluxs,
                self._fluxg,
                self._vtw,
                self._vtr,
                self._vti,
                self._vts,
                self._vtg,
                column_energy_change,
                self._w1,
                self._r1,
                self._i1,
                self._s1,
                self._g1,
            )

            self._warm_rain(
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
                self._vtw,
                self._vtr,
                cloud_condensation_nuclei,
                self._reevap,
                h_var,
            )

            self._add_fluxes_and_surface_tracers(
                preflux_water,
                preflux_rain,
                preflux_ice,
                preflux_snow,
                preflux_graupel,
                self._fluxw,
                self._fluxr,
                self._fluxi,
                self._fluxs,
                self._fluxg,
                surface_water,
                surface_rain,
                surface_ice,
                surface_snow,
                surface_graupel,
                evaporation,
                self._w1,
                self._r1,
                self._i1,
                self._s1,
                self._g1,
                self._reevap,
            )

            if not self._do_warm_rain:
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
                    self._vtw,
                    self._vtr,
                    self._vti,
                    self._vts,
                    self._vtg,
                    h_var,
                )

            self._subgrid_z_proc(
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
                self._cond,
                self._dep,
                self._reevap,
                self._sub,
                rh_adj,
                last_step,
            )

            self._accumulate_state_changes(
                self._cond,
                self._dep,
                self._reevap,
                self._sub,
                condensation,
                deposition,
                evaporation,
                sublimation,
            )
