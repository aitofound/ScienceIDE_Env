from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM
from ndsl.stencils import copy
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.gfdl_cld_mp_driver import (
    calculate_density_factor,
    cloud_nuclei_subgrid_and_relative_humidity,
    convert_specific_to_mass_mixing_ratios_and_calculate_densities,
    convert_virtual_to_true_temperature_and_calc_total_energy,
    moist_total_energy_and_water,
    reset_initial_values_and_make_copies,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class PrelimCalcs:
    def __init__(
        self,
        stencil_factory,
        quantity_factory,
        config,
        consv_te,
    ):
        self.config = config
        self._idx = stencil_factory.grid_indexing
        self.consv_te = consv_te
        self.do_qa = config.do_qa
        self.hydrostatic = config.hydrostatic
        self.consv_checker = config.consv_checker
        self.do_inline_mp = config.do_inline_mp
        self.fix_negative = config.fix_negative
        self._te_err = config.te_err
        self._tw_err = config.tw_err

        def make_quantity2d(**kwargs):
            return quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")

        self._tot_energy_change = make_quantity2d()
        self._bottom_density = make_quantity2d()

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
                    "consv_te": consv_te,
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
                    "moist_q": True,
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
                    "moist_q": False,
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
        density,
        pz,
        density_factor,
        pt,
        ua,
        va,
        wa,
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
        column_energy_change,
        cond,
        adj_vmr,
        gsize,
        hs,
        qcloud_cond_nuclei,
        qcloud_ice_nuclei,
        column_vapor,
        column_water,
        column_rain,
        column_ice,
        column_snow,
        column_graupel,
        total_energy_wet_begin,
        total_water_wet_begin,
        total_energy_bot_wet_begin,
        total_water_bot_wet_begin,
        total_energy_dry_begin,
        total_water_dry_begin,
        total_energy_bot_dry_begin,
        total_water_bot_dry_begin,
        cloud_condensation_nuclei,
        cloud_ice_nuclei,
        h_var,
        rh_adj,
        rh_rain,
    ):
        self._reset_initial_values_and_make_copies(
            adj_vmr,
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            delp,
            pt,
            ua,
            va,
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
            column_energy_change,
            cond,
        )

        if not self.hydrostatic:
            self._copy_stencil(wa, w0)

        if self.do_inline_mp or self.consv_te:
            self._convert_virtual_to_true_temperature_and_calc_total_energy(
                qvapor,
                qliquid,
                qrain,
                qice,
                qsnow,
                qgraupel,
                pt,
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
                total_energy_wet_begin,
                total_water_wet_begin,
                total_energy_bot_wet_begin,
                total_water_bot_wet_begin,
            )

        self._convert_specific_to_mass_mixing_ratios_and_calculate_densities(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            delp,
            delz,
            pt,
            density,
            pz,
            self._bottom_density,
        )

        self._calculate_density_factor(
            density,
            density_factor,
            self._bottom_density,
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
                total_energy_dry_begin,
                total_water_dry_begin,
                total_energy_bot_dry_begin,
                total_water_bot_dry_begin,
            )

        self._cloud_nuclei_subgrid_and_relative_humidity(
            hs,
            qcloud_cond_nuclei,
            qcloud_ice_nuclei,
            density,
            cloud_condensation_nuclei,
            cloud_ice_nuclei,
            gsize,
            h_var,
            rh_adj,
            rh_rain,
        )


class TranslatePreliminaryCalculations(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "pre_qv", "shield": True},
            "qliquid": {"serialname": "pre_ql", "shield": True},
            "qrain": {"serialname": "pre_qr", "shield": True},
            "qice": {"serialname": "pre_qi", "shield": True},
            "qsnow": {"serialname": "pre_qs", "shield": True},
            "qgraupel": {"serialname": "pre_qg", "shield": True},
            "density": {"serialname": "pre_den", "shield": True},
            "delp": {"serialname": "pre_delp", "shield": True},
            "delz": {"serialname": "pre_delz", "shield": True},
            "pz": {"serialname": "pre_pz", "shield": True},
            "density_factor": {"serialname": "pre_denfac", "shield": True},
            "pt": {"serialname": "pre_pt", "shield": True},
            "ua": {"serialname": "pre_ua", "shield": True},
            "va": {"serialname": "pre_va", "shield": True},
            "wa": {"serialname": "pre_wa", "shield": True},
            "qvapor0": {"serialname": "pre_qv0", "shield": True},
            "qliquid0": {"serialname": "pre_ql0", "shield": True},
            "qrain0": {"serialname": "pre_qr0", "shield": True},
            "qice0": {"serialname": "pre_qi0", "shield": True},
            "qsnow0": {"serialname": "pre_qs0", "shield": True},
            "qgraupel0": {"serialname": "pre_qg0", "shield": True},
            "dp0": {"serialname": "pre_dp0", "shield": True},
            "pt0": {"serialname": "pre_pt0", "shield": True},
            "u0": {"serialname": "pre_u0", "shield": True},
            "v0": {"serialname": "pre_v0", "shield": True},
            "w0": {"serialname": "pre_w0", "shield": True},
            "column_energy_change": {"serialname": "pre_dte", "shield": True},
            "cond": {"serialname": "pre_cond", "shield": True},
            "adj_vmr": {"serialname": "pre_adj_vmr", "shield": True},
            "gsize": {"serialname": "pre_gsize", "shield": True},
            "hs": {"serialname": "pre_hs", "shield": True},
            "qcloud_cond_nuclei": {"serialname": "pre_qnl", "shield": True},
            "qcloud_ice_nuclei": {"serialname": "pre_qni", "shield": True},
            "column_vapor": {"serialname": "pre_vapor", "shield": True},
            "column_water": {"serialname": "pre_water", "shield": True},
            "column_rain": {"serialname": "pre_rain", "shield": True},
            "column_ice": {"serialname": "pre_ice", "shield": True},
            "column_snow": {"serialname": "pre_snow", "shield": True},
            "column_graupel": {"serialname": "pre_graupel", "shield": True},
            "total_energy_wet_begin": {"serialname": "pre_ew0", "shield": True},
            "total_water_wet_begin": {"serialname": "pre_ww0", "shield": True},
            "total_energy_bot_wet_begin": {"serialname": "pre_bew0", "shield": True},
            "total_water_bot_wet_begin": {"serialname": "pre_bww0", "shield": True},
            "total_energy_dry_begin": {"serialname": "pre_ed0", "shield": True},
            "total_water_dry_begin": {"serialname": "pre_wd0", "shield": True},
            "total_energy_bot_dry_begin": {"serialname": "pre_bed0", "shield": True},
            "total_water_bot_dry_begin": {"serialname": "pre_bwd0", "shield": True},
            "cloud_condensation_nuclei": {"serialname": "pre_ccn", "shield": True},
            "cloud_ice_nuclei": {"serialname": "pre_cin", "shield": True},
            "h_var": {"serialname": "pre_h_var", "shield": True},
            "rh_adj": {"serialname": "pre_rh_adj", "shield": True},
            "rh_rain": {"serialname": "pre_rh_rain", "shield": True},
        }

        self.out_vars = {
            "qvapor": {"serialname": "pre_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {
                "serialname": "pre_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {"serialname": "pre_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "pre_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "pre_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "pre_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "density": {
                "serialname": "pre_den",
                "kend": self.config.npz,
                "shield": True,
            },
            "delp": {"serialname": "pre_delp", "kend": self.config.npz, "shield": True},
            "delz": {"serialname": "pre_delz", "kend": self.config.npz, "shield": True},
            "pz": {"serialname": "pre_pz", "kend": self.config.npz, "shield": True},
            "density_factor": {
                "serialname": "pre_denfac",
                "kend": self.config.npz,
                "shield": True,
            },
            "pt": {"serialname": "pre_pt", "kend": self.config.npz, "shield": True},
            "ua": {"serialname": "pre_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "pre_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "pre_wa", "kend": self.config.npz, "shield": True},
            "qvapor0": {
                "serialname": "pre_qv0",
                "kend": self.config.npz,
                "shield": True,
            },
            "qliquid0": {
                "serialname": "pre_ql0",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain0": {
                "serialname": "pre_qr0",
                "kend": self.config.npz,
                "shield": True,
            },
            "qice0": {"serialname": "pre_qi0", "kend": self.config.npz, "shield": True},
            "qsnow0": {
                "serialname": "pre_qs0",
                "kend": self.config.npz,
                "shield": True,
            },
            "qgraupel0": {
                "serialname": "pre_qg0",
                "kend": self.config.npz,
                "shield": True,
            },
            "dp0": {"serialname": "pre_dp0", "kend": self.config.npz, "shield": True},
            "pt0": {"serialname": "pre_pt0", "kend": self.config.npz, "shield": True},
            "u0": {"serialname": "pre_u0", "kend": self.config.npz, "shield": True},
            "v0": {"serialname": "pre_v0", "kend": self.config.npz, "shield": True},
            "w0": {"serialname": "pre_w0", "kend": self.config.npz, "shield": True},
            "column_energy_change": {"serialname": "pre_dte", "shield": True},
            "cond": {"serialname": "pre_cond", "shield": True},
            "adj_vmr": {
                "serialname": "pre_adj_vmr",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_wet_begin": {
                "serialname": "pre_ew0",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_wet_begin": {
                "serialname": "pre_ww0",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_wet_begin": {"serialname": "pre_bew0", "shield": True},
            "total_water_bot_wet_begin": {"serialname": "pre_bww0", "shield": True},
            "total_energy_dry_begin": {
                "serialname": "pre_ed0",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_water_dry_begin": {
                "serialname": "pre_wd0",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy_bot_dry_begin": {"serialname": "pre_bed0", "shield": True},
            "total_water_bot_dry_begin": {"serialname": "pre_bwd0", "shield": True},
            "cloud_condensation_nuclei": {
                "serialname": "pre_ccn",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_ice_nuclei": {
                "serialname": "pre_cin",
                "kend": self.config.npz,
                "shield": True,
            },
            "h_var": {"serialname": "pre_h_var", "shield": True},
            "rh_adj": {"serialname": "pre_rh_adj", "shield": True},
            "rh_rain": {"serialname": "pre_rh_rain", "shield": True},
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

        compute_func = PrelimCalcs(
            self.stencil_factory, self.quantity_factory, self.mpconfig, consv_te=False
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
