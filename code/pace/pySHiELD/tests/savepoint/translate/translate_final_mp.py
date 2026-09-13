from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.cloud_fraction import CloudFraction
from pyshield.stencils.gfdl_cld_microphysics.gfdl_cld_mp_driver import (
    calc_sedimentation_energy_loss,
    calculate_particle_properties,
    calculate_total_energy_change_and_convert_temp,
    convert_mass_mixing_to_specific_ratios_and_update_temperatures,
    moist_total_energy_and_water,
    total_energy_check,
    update_temperature_pre_delp_q,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class PostMP:
    def __init__(
        self,
        stencil_factory,
        config,
        quantity_factory,
        consv_te,
    ):
        self._idx = stencil_factory.grid_indexing
        self.consv_te = consv_te
        self.do_qa = config.do_qa
        self.hydrostatic = config.hydrostatic
        self.consv_checker = config.consv_checker
        self.do_inline_mp = config.do_inline_mp
        self.fix_negative = config.fix_negative
        self._te_err = config.te_err
        self._tw_err = config.tw_err
        self.do_sedi_uv = config.do_sedi_uv
        self.do_sedi_w = config.do_sedi_w

        self._tzuv = quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")
        self._tzw = quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")
        self._qcon = quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")
        self._cappa = quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_DIM], units="unknown"
        )

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

        if config.consv_checker:
            self._moist_total_energy_and_water_mq = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": config.hydrostatic,
                    "moist_q": True,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "lv00": config.lv00,
                    "li00": config.li00,
                    "c_air": config.c_air,
                    "timestep": config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._moist_total_energy_and_water = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": config.hydrostatic,
                    "moist_q": False,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "lv00": config.lv00,
                    "li00": config.li00,
                    "c_air": config.c_air,
                    "timestep": config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._calc_sedimentation_energy_loss = stencil_factory.from_origin_domain(
                func=calc_sedimentation_energy_loss,
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

        if self.do_qa:
            self._cloud_fraction = CloudFraction(stencil_factory, config)

    def __call__(
        self,
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        qcld,
        pt,
        density,
        delp,
        delz,
        pz,
        ua,
        va,
        wa,
        total_energy,
        particle_concentration_w,
        particle_concentration_r,
        particle_concentration_i,
        particle_concentration_s,
        particle_concentration_g,
        effective_diameter_w,
        effective_diameter_r,
        effective_diameter_i,
        effective_diameter_s,
        effective_diameter_g,
        optical_extinction_w,
        optical_extinction_r,
        optical_extinction_i,
        optical_extinction_s,
        optical_extinction_g,
        radar_reflectivity_w,
        radar_reflectivity_r,
        radar_reflectivity_i,
        radar_reflectivity_s,
        radar_reflectivity_g,
        terminal_velocity_w,
        terminal_velocity_r,
        terminal_velocity_i,
        terminal_velocity_s,
        terminal_velocity_g,
        qvapor0,
        qliquid0,
        qrain0,
        qice0,
        qsnow0,
        qgraupel0,
        u0,
        v0,
        w0,
        dp0,
        pt0,
        column_energy_change,
        column_energy_loss,
        column_vapor,
        column_water,
        column_rain,
        column_ice,
        column_snow,
        column_graupel,
        total_energy_dry_begin,
        total_energy_moist_begin,
        total_water_dry_begin,
        total_water_moist_begin,
        total_energy_bot_dry_begin,
        total_energy_bot_moist_begin,
        total_water_bot_dry_begin,
        total_water_bot_moist_begin,
        total_energy_dry_end,
        total_water_dry_end,
        total_energy_bot_dry_end,
        total_water_bot_dry_end,
        total_energy_moist_end,
        total_water_moist_end,
        total_energy_bot_moist_end,
        total_water_bot_moist_end,
        adj_vmr,
        h_var,
        gsize,
        last_step: bool,
    ):

        if (self.do_qa) and last_step:
            self._cloud_fraction(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                qcld,
                pt,
                density,
                pz,
                h_var,
                gsize,
            )

        self._calculate_particle_properties(
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            density,
            particle_concentration_w,
            particle_concentration_r,
            particle_concentration_i,
            particle_concentration_s,
            particle_concentration_g,
            effective_diameter_w,
            effective_diameter_r,
            effective_diameter_i,
            effective_diameter_s,
            effective_diameter_g,
            optical_extinction_w,
            optical_extinction_r,
            optical_extinction_i,
            optical_extinction_s,
            optical_extinction_g,
            radar_reflectivity_w,
            radar_reflectivity_r,
            radar_reflectivity_i,
            radar_reflectivity_s,
            radar_reflectivity_g,
            terminal_velocity_w,
            terminal_velocity_r,
            terminal_velocity_i,
            terminal_velocity_s,
            terminal_velocity_g,
        )

        if (self.do_sedi_uv) or (self.do_sedi_w):
            self._update_temperature_pre_delp_q(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                ua,
                va,
                wa,
                u0,
                v0,
                w0,
                pt,
                self._tzuv,
                self._tzw,
            )
        if self.consv_checker:
            self._moist_total_energy_and_water(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                pt,
                ua,
                va,
                wa,
                delp,
                gsize,
                column_energy_change,
                column_vapor,
                column_water,
                column_rain,
                column_ice,
                column_snow,
                column_graupel,
                0.0,
                0.0,
                total_energy_dry_end,
                total_water_dry_end,
                total_energy_bot_dry_end,
                total_water_bot_dry_end,
            )

            self._calc_sedimentation_energy_loss(
                column_energy_loss, column_energy_change, gsize
            )

        self._convert_mass_mixing_to_specific_ratios_and_update_temperatures(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            self._qcon,
            self._cappa,
            ua,
            va,
            wa,
            delp,
            qvapor0,
            qliquid0,
            qrain0,
            qice0,
            qsnow0,
            qgraupel0,
            u0,
            v0,
            w0,
            dp0,
            pt,
            self._tzuv,
            self._tzw,
            adj_vmr,
        )

        if self.consv_checker:
            self._moist_total_energy_and_water_mq(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                pt,
                ua,
                va,
                wa,
                delp,
                gsize,
                column_energy_change,
                column_vapor,
                column_water,
                column_rain,
                column_ice,
                column_snow,
                column_graupel,
                0.0,
                0.0,
                total_energy_moist_end,
                total_water_moist_end,
                total_energy_bot_moist_end,
                total_water_bot_moist_end,
            )

        self._calculate_total_energy_change_and_convert_temp(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            total_energy,
            pt,
            pt0,
            delp,
            delz,
        )

        if self.consv_checker:
            total_energy_check(
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
                self._idx.isc,
                self._idx.iec,
                self._idx.jsc,
                self._idx.jec,
                self._te_err,
                self._tw_err,
            )


class FinalCalcs:
    def __init__(
        self,
        stencil_factory,
        quantity_factory,
        config,
        consv_te,
    ):
        self._idx = stencil_factory.grid_indexing
        self.consv_te = consv_te
        self.do_qa = config.do_qa
        self.hydrostatic = config.hydrostatic
        self.consv_checker = config.consv_checker
        self.do_inline_mp = config.do_inline_mp
        self.fix_negative = config.fix_negative
        self._te_err = config.te_err
        self._tw_err = config.tw_err
        self.do_sedi_uv = config.do_sedi_uv
        self.do_sedi_w = config.do_sedi_w

        self._qcon = quantity_factory.zeros(dims=[I_DIM, J_DIM, K_DIM], units="unknown")
        self._cappa = quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_DIM], units="unknown"
        )

        if config.consv_checker:
            self._moist_total_energy_and_water_mq = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": config.hydrostatic,
                    "moist_q": True,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "lv00": config.lv00,
                    "li00": config.li00,
                    "c_air": config.c_air,
                    "timestep": config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._moist_total_energy_and_water = stencil_factory.from_origin_domain(
                func=moist_total_energy_and_water,
                externals={
                    "hydrostatic": config.hydrostatic,
                    "moist_q": False,
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "lv00": config.lv00,
                    "li00": config.li00,
                    "c_air": config.c_air,
                    "timestep": config.dt_full,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

            self._calc_sedimentation_energy_loss = stencil_factory.from_origin_domain(
                func=calc_sedimentation_energy_loss,
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

    def __call__(
        self,
        qvapor,
        qliquid,
        qrain,
        qice,
        qsnow,
        qgraupel,
        delp,
        delz,
        pt,
        ua,
        va,
        wa,
        total_energy,
        qvapor0,
        qliquid0,
        qrain0,
        qice0,
        qsnow0,
        qgraupel0,
        dp0,
        pt0,
        u0,
        v0,
        w0,
        tzuv,
        tzw,
        column_energy_change,
        adj_vmr,
        gsize,
        column_vapor,
        column_water,
        column_rain,
        column_ice,
        column_snow,
        column_graupel,
        total_energy_moist_begin,
        total_water_moist_begin,
        total_energy_bot_moist_begin,
        total_water_bot_moist_begin,
        total_energy_dry_begin,
        total_water_dry_begin,
        total_energy_bot_dry_begin,
        total_water_bot_dry_begin,
        total_energy_dry_end,
        total_energy_moist_end,
        total_water_dry_end,
        total_water_moist_end,
        total_energy_bot_dry_end,
        total_energy_bot_moist_end,
        total_water_bot_dry_end,
        total_water_bot_moist_end,
        column_energy_loss,
    ):
        if (self.do_sedi_uv) or (self.do_sedi_w):
            self._update_temperature_pre_delp_q(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                ua,
                va,
                wa,
                u0,
                v0,
                w0,
                pt,
                tzuv,
                tzw,
            )
        if self.consv_checker:
            self._moist_total_energy_and_water(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                pt,
                ua,
                va,
                wa,
                delp,
                gsize,
                column_energy_change,
                column_vapor,
                column_water,
                column_rain,
                column_ice,
                column_snow,
                column_graupel,
                0.0,
                0.0,
                total_energy_dry_end,
                total_water_dry_end,
                total_energy_bot_dry_end,
                total_water_bot_dry_end,
            )

            self._calc_sedimentation_energy_loss(
                column_energy_loss, column_energy_change, gsize
            )

        self._convert_mass_mixing_to_specific_ratios_and_update_temperatures(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            self._qcon,
            self._cappa,
            ua,
            va,
            wa,
            delp,
            qvapor0,
            qliquid0,
            qrain0,
            qice0,
            qsnow0,
            qgraupel0,
            u0,
            v0,
            w0,
            dp0,
            pt,
            tzuv,
            tzw,
            adj_vmr,
        )

        if self.consv_checker:
            self._moist_total_energy_and_water_mq(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                pt,
                ua,
                va,
                wa,
                delp,
                gsize,
                column_energy_change,
                column_vapor,
                column_water,
                column_rain,
                column_ice,
                column_snow,
                column_graupel,
                0.0,
                0.0,
                total_energy_moist_end,
                total_water_moist_end,
                total_energy_bot_moist_end,
                total_water_bot_moist_end,
            )

        self._calculate_total_energy_change_and_convert_temp(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            total_energy,
            pt,
            pt0,
            delp,
            delz,
        )

        if self.consv_checker:
            total_energy_check(
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
                self._idx.isc,
                self._idx.iec,
                self._idx.jsc,
                self._idx.jec,
                self._te_err,
                self._tw_err,
            )


class TranslateFinalCalculations(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "fin_qv", "shield": True},
            "qliquid": {"serialname": "fin_ql", "shield": True},
            "qrain": {"serialname": "fin_qr", "shield": True},
            "qice": {"serialname": "fin_qi", "shield": True},
            "qsnow": {"serialname": "fin_qs", "shield": True},
            "qgraupel": {"serialname": "fin_qg", "shield": True},
            "delp": {"serialname": "fin_delp", "shield": True},
            "delz": {"serialname": "fin_delz", "shield": True},
            "pt": {"serialname": "fin_pt", "shield": True},
            "ua": {"serialname": "fin_ua", "shield": True},
            "va": {"serialname": "fin_va", "shield": True},
            "wa": {"serialname": "fin_wa", "shield": True},
            "qvapor0": {"serialname": "fin_qv0", "shield": True},
            "qliquid0": {"serialname": "fin_ql0", "shield": True},
            "qrain0": {"serialname": "fin_qr0", "shield": True},
            "qice0": {"serialname": "fin_qi0", "shield": True},
            "qsnow0": {"serialname": "fin_qs0", "shield": True},
            "qgraupel0": {"serialname": "fin_qg0", "shield": True},
            "dp0": {"serialname": "fin_dp0", "shield": True},
            "pt0": {"serialname": "fin_pt0", "shield": True},
            "u0": {"serialname": "fin_u0", "shield": True},
            "v0": {"serialname": "fin_v0", "shield": True},
            "w0": {"serialname": "fin_w0", "shield": True},
            "column_energy_change": {"serialname": "fin_dte", "shield": True},
            "adj_vmr": {"serialname": "fin_adj_vmr", "shield": True},
            "gsize": {"serialname": "fin_gsize", "shield": True},
            "column_vapor": {"serialname": "fin_vapor", "shield": True},
            "column_water": {"serialname": "fin_water", "shield": True},
            "column_rain": {"serialname": "fin_rain", "shield": True},
            "column_ice": {"serialname": "fin_ice", "shield": True},
            "column_snow": {"serialname": "fin_snow", "shield": True},
            "column_graupel": {"serialname": "fin_graupel", "shield": True},
            "total_energy_moist_begin": {"serialname": "fin_ew0", "shield": True},
            "total_water_moist_begin": {"serialname": "fin_ww0", "shield": True},
            "total_energy_bot_moist_begin": {"serialname": "fin_bew0", "shield": True},
            "total_water_bot_moist_begin": {"serialname": "fin_bww0", "shield": True},
            "total_energy_dry_begin": {"serialname": "fin_ed0", "shield": True},
            "total_water_dry_begin": {"serialname": "fin_wd0", "shield": True},
            "total_energy_bot_dry_begin": {"serialname": "fin_bed0", "shield": True},
            "total_water_bot_dry_begin": {"serialname": "fin_bwd0", "shield": True},
            "total_energy_moist_end": {"serialname": "fin_ew", "shield": True},
            "total_water_moist_end": {"serialname": "fin_ww", "shield": True},
            "total_energy_bot_moist_end": {"serialname": "fin_bew", "shield": True},
            "total_water_bot_moist_end": {"serialname": "fin_bww", "shield": True},
            "total_energy_dry_end": {"serialname": "fin_ed", "shield": True},
            "total_water_dry_end": {"serialname": "fin_wd", "shield": True},
            "total_energy_bot_dry_end": {"serialname": "fin_bed", "shield": True},
            "total_water_bot_dry_end": {"serialname": "fin_bwd", "shield": True},
            "tzuv": {"serialname": "fin_tzuv", "shield": True},
            "tzw": {"serialname": "fin_tzw", "shield": True},
            "total_energy": {"serialname": "fin_te", "shield": True},
            "column_energy_loss": {"serialname": "fin_te_loss", "shield": True},
        }

        self.out_vars = {
            "qvapor": {"serialname": "fin_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {
                "serialname": "fin_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {"serialname": "fin_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "fin_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "fin_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "fin_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "delp": {"serialname": "fin_delp", "kend": self.config.npz, "shield": True},
            "delz": {"serialname": "fin_delz", "kend": self.config.npz, "shield": True},
            "pt": {"serialname": "fin_pt", "kend": self.config.npz, "shield": True},
            "ua": {"serialname": "fin_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "fin_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "fin_wa", "kend": self.config.npz, "shield": True},
            "column_energy_change": {"serialname": "fin_dte", "shield": True},
            "adj_vmr": {
                "serialname": "fin_adj_vmr",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_moist_end": {
                "serialname": "fin_ew",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_moist_end": {
                "serialname": "fin_ww",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_moist_end": {"serialname": "fin_bew", "shield": True},
            "total_water_bot_moist_end": {"serialname": "fin_bww", "shield": True},
            "total_energy_dry_end": {
                "serialname": "fin_ed",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_dry_end": {
                "serialname": "fin_wd",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_dry_end": {"serialname": "fin_bed", "shield": True},
            "total_water_bot_dry_end": {"serialname": "fin_bwd", "shield": True},
            "total_energy": {
                "serialname": "fin_te",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_energy_loss": {"serialname": "fin_te_loss", "shield": True},
        }

        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing
        self.mpconfig = GFDLCloudMPConfig(
            dt_full=self.config.dt_atmos,
            hydrostatic=self.config.hydrostatic,
            npx=self.config.npx,
            npy=self.config.npy,
            npz=self.config.npz,
            nwat=self.config.nwat,
            do_qa=self.config.do_qa,
            do_inline_mp=self.config.do_inline_mp,
            c_cracw=self.config.c_cracw,
            c_paut=self.config.c_paut,
            c_pgacs=self.config.c_pgacs,
            c_psaci=self.config.c_psaci,
            ccn_l=self.config.ccn_l,
            ccn_o=self.config.ccn_o,
            const_vg=self.config.const_vg,
            const_vi=self.config.const_vi,
            const_vr=self.config.const_vr,
            const_vs=self.config.const_vs,
            vs_fac=self.config.vs_fac,
            vg_fac=self.config.vg_fac,
            vi_fac=self.config.vi_fac,
            vr_fac=self.config.vr_fac,
            de_ice=self.config.de_ice,
            layout=self.config.layout,
            tau_imlt=self.config.tau_imlt,
            tau_i2s=self.config.tau_i2s,
            tau_g2v=self.config.tau_g2v,
            tau_v2g=self.config.tau_v2g,
            ql_mlt=self.config.ql_mlt,
            qs_mlt=self.config.qs_mlt,
            t_sub=self.config.t_sub,
            qi_gen=self.config.qi_gen,
            qi_lim=self.config.qi_lim,
            qi0_max=self.config.qi0_max,
            rad_snow=self.config.rad_snow,
            rad_rain=self.config.rad_rain,
            dw_ocean=self.config.dw_ocean,
            dw_land=self.config.dw_land,
            tau_l2v=self.config.tau_l2v,
            c2l_ord=self.config.c2l_ord,
            do_sedi_heat=self.config.do_sedi_heat,
            do_sedi_w=self.config.do_sedi_w,
            fast_sat_adj=self.config.fast_sat_adj,
            qc_crt=self.config.qc_crt,
            fix_negative=self.config.fix_negative,
            irain_f=self.config.irain_f,
            mp_time=self.config.mp_time,
            prog_ccn=self.config.prog_ccn,
            qi0_crt=self.config.qi0_crt,
            qs0_crt=self.config.qs0_crt,
            rh_inc=self.config.rh_inc,
            rh_inr=self.config.rh_inr,
            rthresh=self.config.rthresh,
            sedi_transport=self.config.sedi_transport,
            use_ppm=self.config.use_ppm,
            vg_max=self.config.vg_max,
            vi_max=self.config.vi_max,
            vr_max=self.config.vr_max,
            vs_max=self.config.vs_max,
            z_slope_ice=self.config.z_slope_ice,
            z_slope_liq=self.config.z_slope_liq,
            tice=self.config.tice,
            alin=self.config.alin,
            clin=self.config.clin,
        )
        self.ignore_near_zero_errors = {"fin_te_loss": True}

        self.sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npy - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            self.sizer, backend=self.stencil_factory.backend
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = FinalCalcs(
            self.stencil_factory, self.quantity_factory, self.mpconfig, consv_te=False
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslatePostMP(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "post_qv", "shield": True},
            "qliquid": {"serialname": "post_ql", "shield": True},
            "qrain": {"serialname": "post_qr", "shield": True},
            "qice": {"serialname": "post_qi", "shield": True},
            "qsnow": {"serialname": "post_qs", "shield": True},
            "qgraupel": {"serialname": "post_qg", "shield": True},
            "delp": {"serialname": "post_delp", "shield": True},
            "delz": {"serialname": "post_delz", "shield": True},
            "pt": {"serialname": "post_pt", "shield": True},
            "ua": {"serialname": "post_ua", "shield": True},
            "va": {"serialname": "post_va", "shield": True},
            "wa": {"serialname": "post_wa", "shield": True},
            "pz": {"serialname": "post_pz", "shield": True},
            "qvapor0": {"serialname": "post_qv0", "shield": True},
            "qliquid0": {"serialname": "post_ql0", "shield": True},
            "qrain0": {"serialname": "post_qr0", "shield": True},
            "qice0": {"serialname": "post_qi0", "shield": True},
            "qsnow0": {"serialname": "post_qs0", "shield": True},
            "qgraupel0": {"serialname": "post_qg0", "shield": True},
            "dp0": {"serialname": "post_dp0", "shield": True},
            "pt0": {"serialname": "post_pt0", "shield": True},
            "u0": {"serialname": "post_u0", "shield": True},
            "v0": {"serialname": "post_v0", "shield": True},
            "w0": {"serialname": "post_w0", "shield": True},
            "column_energy_change": {"serialname": "post_dte", "shield": True},
            "adj_vmr": {"serialname": "post_adj_vmr", "shield": True},
            "h_var": {"serialname": "post_h_var", "shield": True},
            "gsize": {"serialname": "post_gsize", "shield": True},
            "column_vapor": {"serialname": "post_vapor", "shield": True},
            "column_water": {"serialname": "post_water", "shield": True},
            "column_rain": {"serialname": "post_rain", "shield": True},
            "column_ice": {"serialname": "post_ice", "shield": True},
            "column_snow": {"serialname": "post_snow", "shield": True},
            "column_graupel": {"serialname": "post_graupel", "shield": True},
            "total_energy_moist_begin": {"serialname": "post_ew0", "shield": True},
            "total_water_moist_begin": {"serialname": "post_ww0", "shield": True},
            "total_energy_bot_moist_begin": {"serialname": "post_bew0", "shield": True},
            "total_water_bot_moist_begin": {"serialname": "post_bww0", "shield": True},
            "total_energy_dry_begin": {"serialname": "post_ed0", "shield": True},
            "total_water_dry_begin": {"serialname": "post_wd0", "shield": True},
            "total_energy_bot_dry_begin": {"serialname": "post_bed0", "shield": True},
            "total_water_bot_dry_begin": {"serialname": "post_bwd0", "shield": True},
            "total_energy_moist_end": {"serialname": "post_ew", "shield": True},
            "total_water_moist_end": {"serialname": "post_ww", "shield": True},
            "total_energy_bot_moist_end": {"serialname": "post_bew", "shield": True},
            "total_water_bot_moist_end": {"serialname": "post_bww", "shield": True},
            "total_energy_dry_end": {"serialname": "post_ed", "shield": True},
            "total_water_dry_end": {"serialname": "post_wd", "shield": True},
            "total_energy_bot_dry_end": {"serialname": "post_bed", "shield": True},
            "total_water_bot_dry_end": {"serialname": "post_bwd", "shield": True},
            "total_energy": {"serialname": "post_te", "shield": True},
            "column_energy_loss": {"serialname": "post_te_loss", "shield": True},
            "density": {"serialname": "post_den", "shield": True},
            "particle_concentration_w": {
                "serialname": "post_pcw",
                "shield": True,
            },
            "effective_diameter_w": {
                "serialname": "post_edw",
                "shield": True,
            },
            "optical_extinction_w": {
                "serialname": "post_oew",
                "shield": True,
            },
            "radar_reflectivity_w": {
                "serialname": "post_rrw",
                "shield": True,
            },
            "terminal_velocity_w": {
                "serialname": "post_tvw",
                "shield": True,
            },
            "particle_concentration_r": {
                "serialname": "post_pcr",
                "shield": True,
            },
            "effective_diameter_r": {
                "serialname": "post_edr",
                "shield": True,
            },
            "optical_extinction_r": {
                "serialname": "post_oer",
                "shield": True,
            },
            "radar_reflectivity_r": {
                "serialname": "post_rrr",
                "shield": True,
            },
            "terminal_velocity_r": {
                "serialname": "post_tvr",
                "shield": True,
            },
            "particle_concentration_i": {
                "serialname": "post_pci",
                "shield": True,
            },
            "effective_diameter_i": {
                "serialname": "post_edi",
                "shield": True,
            },
            "optical_extinction_i": {
                "serialname": "post_oei",
                "shield": True,
            },
            "radar_reflectivity_i": {
                "serialname": "post_rri",
                "shield": True,
            },
            "terminal_velocity_i": {
                "serialname": "post_tvi",
                "shield": True,
            },
            "particle_concentration_s": {
                "serialname": "post_pcs",
                "shield": True,
            },
            "effective_diameter_s": {
                "serialname": "post_eds",
                "shield": True,
            },
            "optical_extinction_s": {
                "serialname": "post_oes",
                "shield": True,
            },
            "radar_reflectivity_s": {
                "serialname": "post_rrs",
                "shield": True,
            },
            "terminal_velocity_s": {
                "serialname": "post_tvs",
                "shield": True,
            },
            "particle_concentration_g": {
                "serialname": "post_pcg",
                "shield": True,
            },
            "effective_diameter_g": {
                "serialname": "post_edg",
                "shield": True,
            },
            "optical_extinction_g": {
                "serialname": "post_oeg",
                "shield": True,
            },
            "radar_reflectivity_g": {
                "serialname": "post_rrg",
                "shield": True,
            },
            "terminal_velocity_g": {
                "serialname": "post_tvg",
                "shield": True,
            },
            "qcld": {"serialname": "post_qa", "shield": True},
        }

        self.out_vars = {
            "qvapor": {
                "serialname": "post_qv",
                "kend": self.config.npz,
                "shield": True,
            },
            "qliquid": {
                "serialname": "post_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {"serialname": "post_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "post_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "post_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "post_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "qcld": {"serialname": "post_qa", "kend": self.config.npz, "shield": True},
            "delp": {
                "serialname": "post_delp",
                "kend": self.config.npz,
                "shield": True,
            },
            "delz": {
                "serialname": "post_delz",
                "kend": self.config.npz,
                "shield": True,
            },
            "pt": {"serialname": "post_pt", "kend": self.config.npz, "shield": True},
            "ua": {"serialname": "post_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "post_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "post_wa", "kend": self.config.npz, "shield": True},
            "column_energy_change": {"serialname": "post_dte", "shield": True},
            "adj_vmr": {
                "serialname": "post_adj_vmr",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_moist_end": {
                "serialname": "post_ew",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_moist_end": {
                "serialname": "post_ww",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_moist_end": {"serialname": "post_bew", "shield": True},
            "total_water_bot_moist_end": {"serialname": "post_bww", "shield": True},
            "total_energy_dry_end": {
                "serialname": "post_ed",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_dry_end": {
                "serialname": "post_wd",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_dry_end": {"serialname": "post_bed", "shield": True},
            "total_water_bot_dry_end": {"serialname": "post_bwd", "shield": True},
            "total_energy": {
                "serialname": "post_te",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_energy_loss": {"serialname": "post_te_loss", "shield": True},
            "particle_concentration_w": {
                "serialname": "post_pcw",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_w": {
                "serialname": "post_edw",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_w": {
                "serialname": "post_oew",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_w": {
                "serialname": "post_rrw",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_w": {
                "serialname": "post_tvw",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_r": {
                "serialname": "post_pcr",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_r": {
                "serialname": "post_edr",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_r": {
                "serialname": "post_oer",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_r": {
                "serialname": "post_rrr",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_r": {
                "serialname": "post_tvr",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_i": {
                "serialname": "post_pci",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_i": {
                "serialname": "post_edi",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_i": {
                "serialname": "post_oei",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_i": {
                "serialname": "post_rri",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_i": {
                "serialname": "post_tvi",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_s": {
                "serialname": "post_pcs",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_s": {
                "serialname": "post_eds",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_s": {
                "serialname": "post_oes",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_s": {
                "serialname": "post_rrs",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_s": {
                "serialname": "post_tvs",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_g": {
                "serialname": "post_pcg",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_g": {
                "serialname": "post_edg",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_g": {
                "serialname": "post_oeg",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_g": {
                "serialname": "post_rrg",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_g": {
                "serialname": "post_tvg",
                "kend": self.config.npz,
                "shield": True,
            },
        }

        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing
        self.mpconfig = GFDLCloudMPConfig(
            dt_full=self.config.dt_atmos,
            hydrostatic=self.config.hydrostatic,
            npx=self.config.npx,
            npy=self.config.npy,
            npz=self.config.npz,
            nwat=self.config.nwat,
            do_qa=self.config.do_qa,
            do_inline_mp=self.config.do_inline_mp,
            c_cracw=self.config.c_cracw,
            c_paut=self.config.c_paut,
            c_pgacs=self.config.c_pgacs,
            c_psaci=self.config.c_psaci,
            ccn_l=self.config.ccn_l,
            ccn_o=self.config.ccn_o,
            const_vg=self.config.const_vg,
            const_vi=self.config.const_vi,
            const_vr=self.config.const_vr,
            const_vs=self.config.const_vs,
            vs_fac=self.config.vs_fac,
            vg_fac=self.config.vg_fac,
            vi_fac=self.config.vi_fac,
            vr_fac=self.config.vr_fac,
            de_ice=self.config.de_ice,
            layout=self.config.layout,
            tau_imlt=self.config.tau_imlt,
            tau_i2s=self.config.tau_i2s,
            tau_g2v=self.config.tau_g2v,
            tau_v2g=self.config.tau_v2g,
            ql_mlt=self.config.ql_mlt,
            qs_mlt=self.config.qs_mlt,
            t_sub=self.config.t_sub,
            qi_gen=self.config.qi_gen,
            qi_lim=self.config.qi_lim,
            qi0_max=self.config.qi0_max,
            rad_snow=self.config.rad_snow,
            rad_rain=self.config.rad_rain,
            dw_ocean=self.config.dw_ocean,
            dw_land=self.config.dw_land,
            tau_l2v=self.config.tau_l2v,
            c2l_ord=self.config.c2l_ord,
            do_sedi_heat=self.config.do_sedi_heat,
            do_sedi_w=self.config.do_sedi_w,
            fast_sat_adj=self.config.fast_sat_adj,
            qc_crt=self.config.qc_crt,
            fix_negative=self.config.fix_negative,
            irain_f=self.config.irain_f,
            mp_time=self.config.mp_time,
            prog_ccn=self.config.prog_ccn,
            qi0_crt=self.config.qi0_crt,
            qs0_crt=self.config.qs0_crt,
            rh_inc=self.config.rh_inc,
            rh_inr=self.config.rh_inr,
            rthresh=self.config.rthresh,
            sedi_transport=self.config.sedi_transport,
            use_ppm=self.config.use_ppm,
            vg_max=self.config.vg_max,
            vi_max=self.config.vi_max,
            vr_max=self.config.vr_max,
            vs_max=self.config.vs_max,
            z_slope_ice=self.config.z_slope_ice,
            z_slope_liq=self.config.z_slope_liq,
            tice=self.config.tice,
            alin=self.config.alin,
            clin=self.config.clin,
        )
        self.mpconfig.do_mp_table_emulation = True

        self.sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npy - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            self.sizer, backend=self.stencil_factory.backend
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        inputs["last_step"] = True

        compute_func = PostMP(
            self.stencil_factory, self.mpconfig, self.quantity_factory, consv_te=False
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
