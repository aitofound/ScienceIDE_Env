from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from pyshield.stencils.gfdl_cld_microphysics import (
    GFDLCloudMicrophysics,
    GFDLCloudMicrophysicsState,
    GFDLCloudMPConfig,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslateMicrophysics3(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "mp_qv", "shield": True},
            "qliquid": {"serialname": "mp_ql", "shield": True},
            "qrain": {"serialname": "mp_qr", "shield": True},
            "qice": {"serialname": "mp_qi", "shield": True},
            "qsnow": {"serialname": "mp_qs", "shield": True},
            "qgraupel": {"serialname": "mp_qg", "shield": True},
            "qcld": {"serialname": "mp_qa", "shield": True},
            "qcloud_cond_nuclei": {"serialname": "mp_qnl", "shield": True},
            "qcloud_ice_nuclei": {"serialname": "mp_qni", "shield": True},
            "pt": {"serialname": "mp_pt", "shield": True},
            "ua": {"serialname": "mp_ua", "shield": True},
            "va": {"serialname": "mp_va", "shield": True},
            "wa": {"serialname": "mp_wa", "shield": True},
            "delz": {"serialname": "mp_delz", "shield": True},
            "delp": {"serialname": "mp_delp", "shield": True},
            "gsize": {"serialname": "mp_gsize", "shield": True},
            "geopotential_surface_height": {"serialname": "mp_hs", "shield": True},
            "column_water": {"serialname": "mp_water", "shield": True},
            "column_rain": {"serialname": "mp_rain", "shield": True},
            "column_ice": {"serialname": "mp_ice", "shield": True},
            "column_snow": {"serialname": "mp_snow", "shield": True},
            "column_graupel": {"serialname": "mp_graupel", "shield": True},
            "qcon": {"serialname": "mp_q_con", "shield": True},
            "cappa": {"serialname": "mp_cappa", "shield": True},
            "total_energy": {"serialname": "mp_te", "shield": True},
            "preflux_water": {"serialname": "mp_prefluxw", "shield": True},
            "preflux_rain": {"serialname": "mp_prefluxr", "shield": True},
            "preflux_ice": {"serialname": "mp_prefluxi", "shield": True},
            "preflux_snow": {"serialname": "mp_prefluxs", "shield": True},
            "preflux_graupel": {"serialname": "mp_prefluxg", "shield": True},
            "condensation": {"serialname": "mp_cond", "shield": True},
            "deposition": {"serialname": "mp_dep", "shield": True},
            "evaporation": {"serialname": "mp_reevap", "shield": True},
            "sublimation": {"serialname": "mp_sub", "shield": True},
        }
        self.in_vars["parameters"] = [
            "timestep",
            "consv_te",
            "last_step",
        ]

        self.out_vars = {
            "qvapor": {"serialname": "mp_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "mp_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "mp_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "mp_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "mp_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "mp_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "qcld": {"serialname": "mp_qa", "kend": self.config.npz, "shield": True},
            "pt": {"serialname": "mp_pt", "kend": self.config.npz, "shield": True},
            "ua": {"serialname": "mp_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "mp_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "mp_wa", "kend": self.config.npz, "shield": True},
            "delz": {"serialname": "mp_delz", "kend": self.config.npz, "shield": True},
            "delp": {"serialname": "mp_delp", "kend": self.config.npz, "shield": True},
            "column_water": {"serialname": "mp_water", "shield": True},
            "column_rain": {"serialname": "mp_rain", "shield": True},
            "column_ice": {"serialname": "mp_ice", "shield": True},
            "column_snow": {"serialname": "mp_snow", "shield": True},
            "column_graupel": {"serialname": "mp_graupel", "shield": True},
            "qcon": {"serialname": "mp_q_con", "kend": self.config.npz, "shield": True},
            "cappa": {
                "serialname": "mp_cappa",
                "kend": self.config.npz,
                "shield": True,
            },
            "adj_vmr": {
                "serialname": "mp_adj_vmr",
                "kend": self.config.npz,
                "shield": True,
            },
            "total_energy": {
                "serialname": "mp_te",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_energy_change": {"serialname": "mp_dte", "shield": True},
            "preflux_water": {
                "serialname": "mp_prefluxw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "mp_prefluxr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "mp_prefluxi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "mp_prefluxs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "mp_prefluxg",
                "kend": self.config.npz,
                "shield": True,
            },
            "deposition": {"serialname": "mp_dep", "shield": True},
            "evaporation": {"serialname": "mp_reevap", "shield": True},
            "sublimation": {"serialname": "mp_sub", "shield": True},
            "particle_concentration_w": {
                "serialname": "mp_pcw",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_w": {
                "serialname": "mp_edw",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_w": {
                "serialname": "mp_oew",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_w": {
                "serialname": "mp_rrw",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_w": {
                "serialname": "mp_tvw",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_r": {
                "serialname": "mp_pcr",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_r": {
                "serialname": "mp_edr",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_r": {
                "serialname": "mp_oer",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_r": {
                "serialname": "mp_rrr",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_r": {
                "serialname": "mp_tvr",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_i": {
                "serialname": "mp_pci",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_i": {
                "serialname": "mp_edi",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_i": {
                "serialname": "mp_oei",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_i": {
                "serialname": "mp_rri",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_i": {
                "serialname": "mp_tvi",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_s": {
                "serialname": "mp_pcs",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_s": {
                "serialname": "mp_eds",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_s": {
                "serialname": "mp_oes",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_s": {
                "serialname": "mp_rrs",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_s": {
                "serialname": "mp_tvs",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_g": {
                "serialname": "mp_pcg",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_g": {
                "serialname": "mp_edg",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_g": {
                "serialname": "mp_oeg",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_g": {
                "serialname": "mp_rrg",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_g": {
                "serialname": "mp_tvg",
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

        microphysics_state = GFDLCloudMicrophysicsState.init_from_storages(
            inputs,
            sizer=self.sizer,
            quantity_factory=self.quantity_factory,
        )

        microphysics = GFDLCloudMicrophysics(
            self.stencil_factory,
            self.quantity_factory,
            self.grid.grid_data,
            self.mpconfig,
            consv_te=inputs["consv_te"],
        )

        microphysics(
            microphysics_state,
            last_step=inputs["last_step"],
        )

        # copy microphysics state back to inputs
        inputs["qvapor"] = microphysics_state.qvapor
        inputs["qliquid"] = microphysics_state.qliquid
        inputs["qrain"] = microphysics_state.qrain
        inputs["qice"] = microphysics_state.qice
        inputs["qsnow"] = microphysics_state.qsnow
        inputs["qgraupel"] = microphysics_state.qgraupel
        inputs["qcld"] = microphysics_state.qcld
        inputs["pt"] = microphysics_state.pt
        inputs["ua"] = microphysics_state.ua
        inputs["va"] = microphysics_state.va
        inputs["wa"] = microphysics_state.wa
        inputs["delz"] = microphysics_state.delz
        inputs["delp"] = microphysics_state.delp
        inputs["column_water"][:] = microphysics_state.column_water
        inputs["column_rain"][:] = microphysics_state.column_rain
        inputs["column_ice"][:] = microphysics_state.column_ice
        inputs["column_snow"][:] = microphysics_state.column_snow
        inputs["column_graupel"] = microphysics_state.column_graupel
        inputs["condensation"][:] = microphysics_state.condensation
        inputs["deposition"][:] = microphysics_state.deposition
        inputs["sublimation"][:] = microphysics_state.sublimation
        inputs["evaporation"][:] = microphysics_state.evaporation
        inputs["column_energy_change"] = microphysics_state.column_energy_change
        inputs["adj_vmr"] = microphysics_state.adj_vmr
        inputs["particle_concentration_w"] = microphysics_state.particle_concentration_w
        inputs["effective_diameter_w"] = microphysics_state.effective_diameter_w
        inputs["optical_extinction_w"] = microphysics_state.optical_extinction_w
        inputs["radar_reflectivity_w"] = microphysics_state.radar_reflectivity_w
        inputs["terminal_velocity_w"] = microphysics_state.terminal_velocity_w
        inputs["particle_concentration_r"] = microphysics_state.particle_concentration_r
        inputs["effective_diameter_r"] = microphysics_state.effective_diameter_r
        inputs["optical_extinction_r"] = microphysics_state.optical_extinction_r
        inputs["radar_reflectivity_r"] = microphysics_state.radar_reflectivity_r
        inputs["terminal_velocity_r"] = microphysics_state.terminal_velocity_r
        inputs["particle_concentration_i"] = microphysics_state.particle_concentration_i
        inputs["effective_diameter_i"] = microphysics_state.effective_diameter_i
        inputs["optical_extinction_i"] = microphysics_state.optical_extinction_i
        inputs["radar_reflectivity_i"] = microphysics_state.radar_reflectivity_i
        inputs["terminal_velocity_i"] = microphysics_state.terminal_velocity_i
        inputs["particle_concentration_s"] = microphysics_state.particle_concentration_s
        inputs["effective_diameter_s"] = microphysics_state.effective_diameter_s
        inputs["optical_extinction_s"] = microphysics_state.optical_extinction_s
        inputs["radar_reflectivity_s"] = microphysics_state.radar_reflectivity_s
        inputs["terminal_velocity_s"] = microphysics_state.terminal_velocity_s
        inputs["particle_concentration_g"] = microphysics_state.particle_concentration_g
        inputs["effective_diameter_g"] = microphysics_state.effective_diameter_g
        inputs["optical_extinction_g"] = microphysics_state.optical_extinction_g
        inputs["radar_reflectivity_g"] = microphysics_state.radar_reflectivity_g
        inputs["terminal_velocity_g"] = microphysics_state.terminal_velocity_g

        return self.slice_output(inputs)
