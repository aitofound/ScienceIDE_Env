import datetime

import numpy as np
from pyrte_rrtmgp import rte
from pyrte_rrtmgp.config import DEFAULT_DIM_MAPPING
from pyrte_rrtmgp.input_mapping import AtmosphericMapping
from pyrte_rrtmgp.rrtmgp import CloudOptics, GasOptics
from pyrte_rrtmgp.rrtmgp_data_files import CloudOpticsFiles, GasOpticsFiles

import ndsl.constants as constants
from ndsl import QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import FORWARD, PARALLEL, computation
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log
from ndsl.dsl.typing import Bool, Float, FloatField, FloatFieldIJ
from pyshield.stencils.surface import SurfaceState

from ._config import RTE_RRTMGPConfig
from .rad_astro import coszmn, sol_init, solar_update
from .rad_clouds import cld_init, progcld4, progcld5
from .rad_gases import co2_update, gas_init, get_gases_bottomup, get_gases_topdown
from .rad_sfc import set_albedo, set_sfcemis, sfc_init
from .state import RTE_RRTMGPState

GRAV = 9.80665
CP_DRY = 1004.64
QMIN = 1.0e-10
QME5 = 1.0e-7
QME6 = 1.0e-7


@gtfunction
def calc_heating_rate(flux_up, flux_down, p_lev):
    """
    Calculates heating rates based on pressures and fluxes,
    assuming k increases with height

    Args:
        flux_up: upward flux
        flux_down: downward flux
        p_lev: model interface pressure
    Returns:
        heating_rate: layer heating rate
    """
    return (
        (flux_up[0, 0, 1] - flux_up - flux_down[0, 0, 1] + flux_down)
        * GRAV
        / (CP_DRY * (p_lev[0, 0, 1] - p_lev))
    )


def calc_tlvl_gfs(
    plyr: FloatField,
    plvl: FloatField,
    tgrs: FloatField,
    tskin: FloatFieldIJ,
    qvapor: FloatField,
    tvly: FloatField,
    tsfa: FloatFieldIJ,
    tlvl: FloatField,
):
    """
    Calculates interface(level) temperatures needed for radiation as in the gfs physics
    """
    with computation(FORWARD):
        with interval(0, 1):
            tsfa = tgrs
            tlvl = tskin
            tem2da = log(plyr)
            tem2db = log(plvl)
            qvapor = max(QME6, qvapor)
            tvly = tgrs * (1.0 + constants.ZVIR * qvapor)
        with interval(1, -1):
            qvapor = max(QME6, qvapor)
            tvly = tgrs * (1.0 + constants.ZVIR * qvapor)
            tlvl = tgrs[0, 0, -1] + (tgrs - tgrs[0, 0, -1]) * (
                log(plvl) - log(plyr[0, 0, -1])
            ) / (log(plyr) - log(plyr[0, 0, -1]))
        with interval(-1, None):
            tlvl = tgrs[0, 0, -1]


def calc_tlvl_am5(
    plyr: FloatField,
    plvl: FloatField,
    tlyr: FloatField,
    tskin: FloatFieldIJ,
    tlvl: FloatField,
):
    """
    Calculates interface(level) temperatures needed for radiation as in the am5 physics
    Assumes k=0 at the top of the atmosphere
    """
    with computation(FORWARD):
        with interval(0, 1):
            tlvl = tlyr
        with interval(1, -1):
            tlvl = (
                (plyr[0, 0, -1] * tlyr[0, 0, -1] * (plvl - plyr))
                + (plyr * tlyr * (plyr[0, 0, -1] - plvl))
            ) / (plvl * (plyr[0, 0, -1] - plyr))
        with interval(-1, None):
            tlvl = tskin


def calc_net_flux_and_heating(
    sw_flux_up: FloatField,
    sw_flux_down: FloatField,
    lw_flux_up: FloatField,
    lw_flux_down: FloatField,
    sw_flux_up_clear: FloatField,
    sw_flux_down_clear: FloatField,
    lw_flux_up_clear: FloatField,
    lw_flux_down_clear: FloatField,
    sw_flux_net: FloatFieldIJ,
    p_lev: FloatField,
    sw_heating_rate: FloatField,
    lw_heating_rate: FloatField,
    sw_heating_rate_clear: FloatField,
    lw_heating_rate_clear: FloatField,
):
    """
    Calculates heating rates and net shortwave surface flux
    """
    with computation(FORWARD), interval(0, 1):
        sw_flux_net = sw_flux_down - sw_flux_up
        sw_heating_rate = calc_heating_rate(sw_flux_up, sw_flux_down, p_lev)
        lw_heating_rate = calc_heating_rate(lw_flux_up, lw_flux_down, p_lev)
        sw_heating_rate_clear = calc_heating_rate(
            sw_flux_up_clear, sw_flux_down_clear, p_lev
        )
        lw_heating_rate_clear = calc_heating_rate(
            lw_flux_up_clear, lw_flux_down_clear, p_lev
        )
    with computation(PARALLEL), interval(1, -1):
        sw_heating_rate = calc_heating_rate(sw_flux_up, sw_flux_down, p_lev)
        lw_heating_rate = calc_heating_rate(lw_flux_up, lw_flux_down, p_lev)
        sw_heating_rate_clear = calc_heating_rate(
            sw_flux_up_clear, sw_flux_down_clear, p_lev
        )
        lw_heating_rate_clear = calc_heating_rate(
            lw_flux_up_clear, lw_flux_down_clear, p_lev
        )


class RTE_RRTMGPDriver:
    def __init__(
        self,
        config: RTE_RRTMGPConfig,
        gridlon: FloatFieldIJ,
        gridlat: FloatFieldIJ,
        sigma: np.ndarray,
        quantity_factory: QuantityFactory,
        stencil_factory: StencilFactory,
    ):
        grid_indexing = stencil_factory.grid_indexing
        iyear = config.date.year
        imonth = config.date.month
        iday = config.date.day
        ihr = config.date.hour
        self.saved_iyear = iyear
        self.saved_imonth = imonth
        self.saved_iday = iday
        self.deltsw = config.deltsw
        self.delt_rad = config.delt_rad
        self.isolar = config.isolar
        self.ico2flg = config.ico2flg
        self.ictmflg = config.ictmflg
        self.ialbflg = config.ialbflg
        self.ldisable_radiation_quasi_sea_ice = config.ldisable_radiation_quasi_sea_ice
        self._first_step = True

        self.solhr = ihr + config.date.minute / 60.0 + config.date.second / 3600.0
        self.slag = 0.0
        self.sdec = 0.0
        self.cdec = 0.0
        self.anginc = 0.0
        self.solcon = 0.0
        self.solc0 = 0.0
        self.nstp = 0

        # Allocate quantities
        self._coszdg = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "radians",
            dtype=Float,
        )
        self._daymask = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "",
            dtype=Bool,
        )
        self._co2_cyc = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "",
            dtype=Bool,
        )
        self._co2_arr = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "",
            dtype=Bool,
        )
        self._tvly = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            "degK",
            dtype=Float,
        )
        self._tsfca = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "degK",
            dtype=Float,
        )
        self._cnvw = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            "",
            dtype=Float,
        )
        self._cnvc = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            "",
            dtype=Float,
        )
        self._coslat = quantity_factory.zeros(
            [I_DIM, J_DIM],
            "",
            dtype=Float,
        )

        self.gridlon = gridlon
        self.gridlat = gridlat
        config.input_dir.joinpath(config.solar_constant_file)
        # Init solar params
        self.isolflg, self._solar_constants, self.solc0 = sol_init(
            self.isolar,
            config.input_dir.joinpath(config.solar_constant_file),
            iyear,
        )
        # Here is where we will initialize aerosols once they're supported

        # Init gases
        (
            self.n2o,
            self.ch4,
            self.o2,
            self.co,
            self.n2,
            self.cfc11,
            self.cfc12,
            self.cfc22,
            self.cfc113,
            self.ccl4,
            self.co2_glb,
            co2_arr,
            co2_cyc,
            self.co2_mvr_data,
            self.co2_glb_data,
            self.co2_cyc_data,
        ) = gas_init(
            config.input_dir,
            config.ico2flg,
            config.ioznflg,
            config.ictmflg,
            iyear,
            imonth,
            gridlon.view[:],
            gridlat.view[:],
        )

        self._co2_cyc.view[:] = co2_cyc
        self._co2_arr.view[:] = co2_arr

        # Init sfc albedo and emissivity
        self.albedo = np.zeros((gridlon.view[:].shape[0], gridlon.view[:].shape[1], 4))
        self.sfcemis = np.zeros((gridlon.view[:].shape[0], gridlon.view[:].shape[1]))
        sfcemis_datafile = config.input_dir.joinpath("sfc_emissivity_idx.txt")
        self.iemslw, self._sfcemis_map = sfc_init(
            config.ialbflg,
            config.iemsflg,
            config.ldisable_radiation_quasi_sea_ice,
            sfcemis_datafile,
        )

        # Init clouds:
        self._llyr = cld_init(sigma, config.ivflip)

        self._cloud_optics_lw = CloudOptics(cloud_optics_file=CloudOpticsFiles.LW_BND)
        self._gas_optics_lw = GasOptics(gas_optics_file=GasOpticsFiles.LW_G256)

        self._cloud_optics_sw = CloudOptics(cloud_optics_file=CloudOpticsFiles.SW_BND)
        self._gas_optics_sw = GasOptics(gas_optics_file=GasOpticsFiles.SW_G224)
        self._gas_mapping = {
            "h2o": "qvapor",
            "o3": "qo3mr",
            "co": "co",
            "n2o": "n2o",
            "o2": "o2",
            "co2": "co2",
            "n2": "n2",
        }
        self._var_mapping = {
            "pres_layer": "prsl",
            "pres_level": "prsi",
            "temp_layer": "tlyr",
            "temp_level": "tlvl",
            "surface_temperature": "tsfc",
            "lwp": "clwp",
            "iwp": "cip",
            "rel": "clwr",
            "rei": "cir",
            "solar_zenith_angle": "solar_zenith_angle",
            "surface_albedo": "surface_albedo",
            "surface_albedo_direct": "surface_albedo_direct",
            "surface_albedo_diffuse": "surface_albedo_diffuse",
            "surface_emissivity": "surface_emissivity",
            "surface_emissivity_jacobian": "surface_emissivity_jacobian",
        }
        self._atm_map = AtmosphericMapping(
            dim_mapping=DEFAULT_DIM_MAPPING,
            var_mapping=self._var_mapping,
        )

        self._calc_tlvl = stencil_factory.from_origin_domain(
            func=calc_tlvl_gfs,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        if config.icmphys == 4:
            self._cldscheme = 4
            self._progcld4 = stencil_factory.from_origin_domain(
                func=progcld4,
                externals={
                    "ivflip": config.ivflip,
                    "lcrick": config.lcrick,
                    "lcnorm": config.lcnorm,
                },
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
        elif config.icmphys == 5:
            self._cldscheme = 5
            self._progcld5 = stencil_factory.from_origin_domain(
                func=progcld5,
                externals={
                    "gfs_cloud_overlap": config.gfs_cloud_overlap,
                    "ivflip": config.ivflip,
                    "lcrick": config.lcrick,
                    "lcnorm": config.lcnorm,
                },
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
            raise NotImplementedError(
                f"radiation cloud microphysics control flag {config.icmphys} "
                "does not have cnvw or cnvc yet"
            )
        else:
            raise NotImplementedError(
                f"radiation cloud microphysics control flag {config.icmphys} "
                "not implemented, please choose 4 or 5"
            )
        self._coszmn = stencil_factory.from_origin_domain(
            func=coszmn,
            externals={
                "daily_mean": config.daily_mean,
                "fixed_sollat": config.fixed_sollat,
                "nstp": config.nstp,
                "sollat": config.sollat,
            },
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        if config.ictmflg == -2:
            if config.ivflip == 0:
                self._get_gases = stencil_factory.from_origin_domain(
                    func=get_gases_topdown,
                    externals={
                        "ico2flg": config.ico2flg,
                    },
                    origin=grid_indexing.origin_compute(),
                    domain=grid_indexing.domain_compute(),
                )
            else:
                self._get_gases = stencil_factory.from_origin_domain(
                    func=get_gases_bottomup,
                    externals={
                        "ico2flg": config.ico2flg,
                    },
                    origin=grid_indexing.origin_compute(),
                    domain=grid_indexing.domain_compute(),
                )

        self._calc_net_flux_and_heating = stencil_factory.from_origin_domain(
            func=calc_net_flux_and_heating,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )

    def _accumulate_radiation_inputs(
        self, state: RTE_RRTMGPState, sfc_state: SurfaceState, sdate: datetime.datetime
    ):
        """
        For RTE-RRTMGP we need level and layer profiles of temperature and pressure,
        the species used for the spectral calculations:
            humidity, cloud water (and size), cloud ice (and size),
            CO2, O3, N2O, N2, O2, CH4, CO
        albedo and surface emissivities, the solar zenith angle
        Here we extract that info from the model state and time,
        and make sure units are correct.
        """

        self._update_inputs_if_needed(state, sdate)

        self._calc_tlvl(
            state.prsl,
            state.prsi,
            state.tlyr,
            state.tsfc,
            state.qvapor,
            self._tvly,
            self._tsfca,
            state.tlvl,
        )

        self._coszmn(
            self.gridlon,
            self.gridlat,
            self._coslat,
            Float(sdate.hour),
            self.slag,
            self.sdec,
            self.cdec,
            self.anginc,
            state.mu0,
            self._coszdg,
            self._daymask,
        )

        if self.ictmflg == -2:
            self._get_gases(
                state.co2,
                state.prsl,
                self.co2_glb,
                self._co2_cyc,
                self._co2_arr,
            )

        if self._cldscheme == 4:
            self._progcld4(
                state.prsl,
                state.prsi,
                state.tlyr,
                self._tvly,
                state.qliquid,
                sfc_state.islmsk,
                state.qcld,
                state.clwp,
                state.clwr,
                state.cip,
                state.cir,
            )
        elif self._cldscheme == 5:
            self._progcld5(
                state.prsl,
                state.prsi,
                state.tlyr,
                self._tvly,
                state.qliquid,
                self._cnvw,
                self._cnvc,
                sfc_state.islmsk,
                state.qcld,
                state.clwp,
                state.clwr,
                state.cip,
                state.cir,
            )

        set_albedo(
            self.ialbflg,
            sfc_state.islmsk.field,
            sfc_state.snowd.field,
            sfc_state.sncovr.field,
            sfc_state.snoalb.field,
            sfc_state.zorl.field,
            state.mu0.field,
            sfc_state.tsfc.field,
            sfc_state.hprim.field,
            sfc_state.alvsf.field,
            sfc_state.alnsf.field,
            sfc_state.alvwf.field,
            sfc_state.alnwf.field,
            sfc_state.facsf.field,
            sfc_state.facwf.field,
            sfc_state.fice.field,
            sfc_state.tisfc.field,
            sfc_state.albedo.field,
            self.albedo,
            self.ldisable_radiation_quasi_sea_ice,
        )
        if self.ialbflg == -1 or self.ialbflg == -2:
            state.albedo.view[:] = self.albedo[:, :, 0]
            # TODO: Add support for diffuse and direct albedos,
            # other values for ialbflg abnf the rest of the sfc parameterization code
        set_sfcemis(
            self.gridlon.view[:],
            self.gridlat.view[:],
            sfc_state.islmsk.field,
            sfc_state.snowd.field,
            sfc_state.sncovr.field,
            sfc_state.zorl.field,
            sfc_state.tsfc.field,
            sfc_state.hprim.field,
            self.iemslw,
            self.ialbflg,
            self.ldisable_radiation_quasi_sea_ice,
            self.sfcemis,
            self._sfcemis_map,
            sfc_state.sfcemis.field,
        )
        state.sfc_emis.view[:] = self.sfcemis

    def _update_inputs_if_needed(
        self, state: RTE_RRTMGPState, sdate: datetime.datetime
    ):
        """
        Updates input data from external sources when model date differs
        from the saved date
        """
        lsol_chg = False
        if (self.isolflg not in [0, 10]) and sdate.year != self.saved_iyear:
            lsol_chg = True
        (
            self.slag,
            self.sdec,
            self.cdec,
            self.anginc,
            self.solcon,
            self.solc0,
            self.nstp,
            self.saved_iyear,
        ) = solar_update(
            sdate,
            self.solc0,
            self.deltsw,
            self.delt_rad,
            lsol_chg,
            self.saved_iyear,
            self.isolflg,
            self._solar_constants,
        )

        # Here is where we update ozone and aerosols when enabled
        update_co2 = False
        if (sdate.month != self.saved_imonth) or (self._first_step):
            update_co2 = True
            self.saved_imonth = sdate.month

        (
            self.co2_glb,
            self._co2_arr.view[:],
            self._co2_cyc.view[:],
        ) = co2_update(
            sdate.year,
            sdate.month,
            self.ico2flg,
            update_co2,
            self.ictmflg,
            self.co2_glb,
            self._co2_arr.view[:],
            self._co2_cyc.view[:],
            self.gridlon.view[:],
            self.gridlat.view[:],
            self.co2_glb_data,
            self.co2_mvr_data,
            self.co2_cyc_data,
        )

    def _assign_constant_gases(self, xds):
        xds["ch4"] = Float(self.ch4)
        xds["n2o"] = Float(self.n2o)
        xds["n2"] = Float(self.n2)
        xds["o2"] = Float(self.o2)
        xds["co"] = Float(self.co)
        xds["ch4"] = Float(self.ch4)
        xds["cfc11"] = Float(
            self.cfc11,
        )
        xds["cfc12"] = Float(
            self.cfc12,
        )
        xds["cfc22"] = Float(
            self.cfc22,
        )
        xds["cfc113"] = Float(
            self.cfc113,
        )
        xds["ccl4"] = Float(
            self.ccl4,
        )

    def prep_radiation(
        self, state: RTE_RRTMGPState, sfc_state: SurfaceState, date: datetime.datetime
    ):
        """
        Method to prepare radiation inputs for flux calculations. Updates solar, gas,
        and surface variables, gathers the necessary atmospheric data, and packages it
        as an xarray dataset that can be ingested by pyRTE-RRTMGP.

        Args:
            state (RTE_RRTMGPState): input state containing atmospheric information
            sfc_state (SurfaceState): contains surface properties such as
                surface type, snow cover, etc.
            date (datetime.datetime): datetime for radiation calculations

        Returns:
            radx: (xarray.Dataset): An xarray dataset ready to be passed into
                pyRTE-RRTMGP
        """
        self._accumulate_radiation_inputs(state, sfc_state, date)
        radx = state.to_rterrtmgp_xr()
        self._assign_constant_gases(radx)
        return radx

    def step_radiation(
        self, state: RTE_RRTMGPState, sfc_state: SurfaceState, date: datetime.datetime
    ):
        """
        Method to compute radiative fluxes for a given atmospheric and surface state
        at a given date and time. The Radiation State is updated with longwave and
        shortwave fluxes and heating rates in-place.

        Args:
            state (RTE_RRTMGPState): input state containing atmospheric information,
                will be updated with radiative fluxes and heating rates
            sfc_state (SurfaceState): contains surface properties such as
                surface type, snow cover, etc.
            date (datetime.datetime): datetime for radiation calculations
        """
        self.solhr = date.hour + date.minute / 60.0 + date.second / 3600.0
        radx = self.prep_radiation(state, sfc_state, date)

        # Do SW fluxes:
        sw_optics = self._gas_optics_sw.compute(
            radx,
            problem_type=rte.OpticsTypes.TWO_STREAM,
            add_to_input=False,
            gas_name_map=self._gas_mapping,
            variable_mapping=self._atm_map,
        )
        sw_optics["surface_albedo"] = radx["albedo"]
        sw_optics["mu0"] = radx["mu0"]
        clr_fluxes_sw = sw_optics.rte.solve(add_to_input=False)
        state.fswd_clr.view[:] = clr_fluxes_sw.sw_flux_down[:].reshape(
            state.fswd_clr.view[:].shape
        )
        state.fswu_clr.view[:] = clr_fluxes_sw.sw_flux_up[:].reshape(
            state.fswu_clr.view[:].shape
        )

        sw_cloud_optical_props = self._cloud_optics_sw.compute(
            radx,
            problem_type=rte.OpticsTypes.TWO_STREAM,
            add_to_input=False,
            variable_mapping=self._atm_map,
        )
        sw_cloud_optical_props.rte.add_to(sw_optics)
        fluxes_sw = sw_optics.rte.solve(add_to_input=False)
        state.fswd.view[:] = fluxes_sw.sw_flux_down[:].reshape(state.fswd.view[:].shape)
        state.fswu.view[:] = fluxes_sw.sw_flux_up[:].reshape(state.fswu.view[:].shape)

        # And do LW fluxes
        lw_optics = self._gas_optics_lw.compute(
            radx,
            problem_type=rte.OpticsTypes.ABSORPTION,
            add_to_input=False,
            gas_name_map=self._gas_mapping,
            variable_mapping=self._atm_map,
        )
        lw_optics["surface_emissivity"] = radx["sfc_emis"]
        clr_fluxes_lw = lw_optics.rte.solve(add_to_input=False)
        state.flwd_clr.view[:] = clr_fluxes_lw.lw_flux_down[:].reshape(
            state.flwd_clr.view[:].shape
        )
        state.flwu_clr.view[:] = clr_fluxes_lw.lw_flux_up[:].reshape(
            state.flwu_clr.view[:].shape
        )
        lw_cloud_optical_props = self._cloud_optics_lw.compute(
            radx,
            problem_type=rte.OpticsTypes.ABSORPTION,
            add_to_input=False,
            variable_mapping=self._atm_map,
        )
        lw_cloud_optical_props.rte.add_to(lw_optics)
        fluxes_lw = lw_optics.rte.solve(add_to_input=False)

        state.flwd.view[:] = fluxes_lw.lw_flux_down[:].reshape(state.flwd.view[:].shape)
        state.flwu.view[:] = fluxes_lw.lw_flux_up[:].reshape(state.flwu.view[:].shape)

        self._calc_net_flux_and_heating(
            state.fswu,
            state.fswd,
            state.flwu,
            state.flwd,
            state.fswu_clr,
            state.fswd_clr,
            state.flwu_clr,
            state.flwd_clr,
            state.fswn,
            state.prsi,
            state.hrtsw,
            state.hrtlw,
            state.hrtsw_clr,
            state.hrtlw_clr,
        )

        if self._first_step:
            self._first_step = False
