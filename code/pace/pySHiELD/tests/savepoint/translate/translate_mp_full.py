from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.ice_cloud import IceCloud
from pyshield.stencils.gfdl_cld_microphysics.mp_full import (
    FullMicrophysics,
    accumulate_state_changes,
    add_fluxes_and_surface_tracers,
)
from pyshield.stencils.gfdl_cld_microphysics.sedimentation import Sedimentation
from pyshield.stencils.gfdl_cld_microphysics.subgrid_z_proc import (
    VerticalSubgridProcesses,
)
from pyshield.stencils.gfdl_cld_microphysics.warm_rain import WarmRain
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class SubMicrophysics:
    def __init__(
        self,
        stencil_factory,
        quantity_factory,
        config,
        timestep: float,
        convert_mm_day: float,
    ):

        self._idx = stencil_factory.grid_indexing

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
        cloud_condensation_nuclei,
        cloud_ice_nuclei,
        preflux_water,
        preflux_rain,
        preflux_ice,
        preflux_snow,
        preflux_graupel,
        h_var,
        rh_adj,
        column_energy_change,
        surface_water,
        surface_rain,
        surface_ice,
        surface_snow,
        surface_graupel,
        condensation,
        deposition,
        evaporation,
        sublimation,
    ):
        """
        Full Microphysics Loop
        executes ntimes:
        """
        for _i in range(self._ntimes):
            # self._sedimentation(
            #     qvapor,
            #     qliquid,
            #     qrain,
            #     qice,
            #     qsnow,
            #     qgraupel,
            #     ua,
            #     va,
            #     wa,
            #     temperature,
            #     delp,
            #     delz,
            #     density,
            #     density_factor,
            #     self._fluxw,
            #     self._fluxr,
            #     self._fluxi,
            #     self._fluxs,
            #     self._fluxg,
            #     self._vtw,
            #     self._vtr,
            #     self._vti,
            #     self._vts,
            #     self._vtg,
            #     column_energy_change,
            #     self._w1,
            #     self._r1,
            #     self._i1,
            #     self._s1,
            #     self._g1,
            # )

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

            # self._subgrid_z_proc(
            #     qvapor,
            #     qliquid,
            #     qrain,
            #     qice,
            #     qsnow,
            #     qgraupel,
            #     temperature,
            #     density,
            #     density_factor,
            #     delp,
            #     cloud_condensation_nuclei,
            #     cloud_ice_nuclei,
            #     self._cond,
            #     self._dep,
            #     self._reevap,
            #     self._sub,
            #     rh_adj,
            # )

            # self._accumulate_state_changes(
            #     self._cond,
            #     self._dep,
            #     self._reevap,
            #     self._sub,
            #     condensation,
            #     deposition,
            #     evaporation,
            #     sublimation,
            # )


class TranslateMPFull(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "mpf_qv", "shield": True},
            "qliquid": {"serialname": "mpf_ql", "shield": True},
            "qrain": {"serialname": "mpf_qr", "shield": True},
            "qice": {"serialname": "mpf_qi", "shield": True},
            "qsnow": {"serialname": "mpf_qs", "shield": True},
            "qgraupel": {"serialname": "mpf_qg", "shield": True},
            "ua": {"serialname": "mpf_u", "shield": True},
            "va": {"serialname": "mpf_v", "shield": True},
            "wa": {"serialname": "mpf_w", "shield": True},
            "temperature": {"serialname": "mpf_pt", "shield": True},
            "delp": {"serialname": "mpf_delp", "shield": True},
            "delz": {"serialname": "mpf_delz", "shield": True},
            "density": {"serialname": "mpf_den", "shield": True},
            "density_factor": {"serialname": "mpf_denfac", "shield": True},
            "cloud_condensation_nuclei": {"serialname": "mpf_ccn", "shield": True},
            "cloud_ice_nuclei": {"serialname": "mpf_cin", "shield": True},
            "preflux_water": {"serialname": "mpf_pfw", "shield": True},
            "preflux_rain": {"serialname": "mpf_pfr", "shield": True},
            "preflux_ice": {"serialname": "mpf_pfi", "shield": True},
            "preflux_snow": {"serialname": "mpf_pfs", "shield": True},
            "preflux_graupel": {"serialname": "mpf_pfg", "shield": True},
            "h_var": {"serialname": "mpf_h_var", "shield": True},
            "rh_adj": {"serialname": "mpf_rh_adj", "shield": True},
            "column_energy_change": {"serialname": "mpf_dte", "shield": True},
            "surface_water": {"serialname": "mpf_water", "shield": True},
            "surface_rain": {"serialname": "mpf_rain", "shield": True},
            "surface_ice": {"serialname": "mpf_ice", "shield": True},
            "surface_snow": {"serialname": "mpf_snow", "shield": True},
            "surface_graupel": {"serialname": "mpf_graupel", "shield": True},
            "condensation": {"serialname": "mpf_cond", "shield": True},
            "deposition": {"serialname": "mpf_dep", "shield": True},
            "evaporation": {"serialname": "mpf_evap", "shield": True},
            "sublimation": {"serialname": "mpf_sub", "shield": True},
        }

        self.in_vars["parameters"] = ["convt", "dt"]

        self.out_vars = {
            "qvapor": {"serialname": "mpf_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {
                "serialname": "mpf_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {"serialname": "mpf_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "mpf_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "mpf_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "mpf_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "ua": {"serialname": "mpf_u", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "mpf_v", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "mpf_w", "kend": self.config.npz, "shield": True},
            "temperature": {
                "serialname": "mpf_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "delp": {"serialname": "mpf_delp", "kend": self.config.npz, "shield": True},
            "delz": {"serialname": "mpf_delz", "kend": self.config.npz, "shield": True},
            "density": {
                "serialname": "mpf_den",
                "kend": self.config.npz,
                "shield": True,
            },
            "density_factor": {
                "serialname": "mpf_denfac",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_condensation_nuclei": {
                "serialname": "mpf_ccn",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_ice_nuclei": {
                "serialname": "mpf_cin",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_water": {
                "serialname": "mpf_pfw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "mpf_pfr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "mpf_pfi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "mpf_pfs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "mpf_pfg",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_energy_change": {"serialname": "mpf_dte", "shield": True},
            "surface_water": {"serialname": "mpf_water", "shield": True},
            "surface_rain": {"serialname": "mpf_rain", "shield": True},
            "surface_ice": {"serialname": "mpf_ice", "shield": True},
            "surface_snow": {"serialname": "mpf_snow", "shield": True},
            "surface_graupel": {"serialname": "mpf_graupel", "shield": True},
            "condensation": {"serialname": "mpf_cond", "shield": True},
            "deposition": {"serialname": "mpf_dep", "shield": True},
            "evaporation": {"serialname": "mpf_evap", "shield": True},
            "sublimation": {"serialname": "mpf_sub", "shield": True},
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

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npy - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = FullMicrophysics(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
            inputs.pop("dt"),
            inputs.pop("convt"),
        )
        inputs["last_step"] = True

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateMPSub(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "mpsub_qv", "shield": True},
            "qliquid": {"serialname": "mpsub_ql", "shield": True},
            "qrain": {"serialname": "mpsub_qr", "shield": True},
            "qice": {"serialname": "mpsub_qi", "shield": True},
            "qsnow": {"serialname": "mpsub_qs", "shield": True},
            "qgraupel": {"serialname": "mpsub_qg", "shield": True},
            "ua": {"serialname": "mpsub_u", "shield": True},
            "va": {"serialname": "mpsub_v", "shield": True},
            "wa": {"serialname": "mpsub_w", "shield": True},
            "temperature": {"serialname": "mpsub_pt", "shield": True},
            "delp": {"serialname": "mpsub_delp", "shield": True},
            "delz": {"serialname": "mpsub_delz", "shield": True},
            "density": {"serialname": "mpsub_den", "shield": True},
            "density_factor": {"serialname": "mpsub_denfac", "shield": True},
            "cloud_condensation_nuclei": {"serialname": "mpsub_ccn", "shield": True},
            "cloud_ice_nuclei": {"serialname": "mpsub_cin", "shield": True},
            "preflux_water": {"serialname": "mpsub_pfw", "shield": True},
            "preflux_rain": {"serialname": "mpsub_pfr", "shield": True},
            "preflux_ice": {"serialname": "mpsub_pfi", "shield": True},
            "preflux_snow": {"serialname": "mpsub_pfs", "shield": True},
            "preflux_graupel": {"serialname": "mpsub_pfg", "shield": True},
            "h_var": {"serialname": "mpsub_h_var", "shield": True},
            "rh_adj": {"serialname": "mpsub_rh_adj", "shield": True},
            "column_energy_change": {"serialname": "mpsub_dte", "shield": True},
            "surface_water": {"serialname": "mpsub_water", "shield": True},
            "surface_rain": {"serialname": "mpsub_rain", "shield": True},
            "surface_ice": {"serialname": "mpsub_ice", "shield": True},
            "surface_snow": {"serialname": "mpsub_snow", "shield": True},
            "surface_graupel": {"serialname": "mpsub_graupel", "shield": True},
            "condensation": {"serialname": "mpsub_cond", "shield": True},
            "deposition": {"serialname": "mpsub_dep", "shield": True},
            "evaporation": {"serialname": "mpsub_evap", "shield": True},
            "sublimation": {"serialname": "mpsub_sub", "shield": True},
        }

        self.in_vars["parameters"] = ["convt", "dt"]

        self.out_vars = {
            "qvapor": {
                "serialname": "mpsub_qv",
                "kend": self.config.npz,
                "shield": True,
            },
            "qliquid": {
                "serialname": "mpsub_ql",
                "kend": self.config.npz,
                "shield": True,
            },
            "qrain": {
                "serialname": "mpsub_qr",
                "kend": self.config.npz,
                "shield": True,
            },
            "qice": {"serialname": "mpsub_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {
                "serialname": "mpsub_qs",
                "kend": self.config.npz,
                "shield": True,
            },
            "qgraupel": {
                "serialname": "mpsub_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "ua": {"serialname": "mpsub_u", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "mpsub_v", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "mpsub_w", "kend": self.config.npz, "shield": True},
            "temperature": {
                "serialname": "mpsub_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_condensation_nuclei": {
                "serialname": "mpsub_ccn",
                "kend": self.config.npz,
                "shield": True,
            },
            "cloud_ice_nuclei": {
                "serialname": "mpsub_cin",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_water": {
                "serialname": "mpsub_pfw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "mpsub_pfr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "mpsub_pfi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "mpsub_pfs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "mpsub_pfg",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_energy_change": {"serialname": "mpsub_dte", "shield": True},
            "surface_water": {"serialname": "mpsub_water", "shield": True},
            "surface_rain": {"serialname": "mpsub_rain", "shield": True},
            "surface_ice": {"serialname": "mpsub_ice", "shield": True},
            "surface_snow": {"serialname": "mpsub_snow", "shield": True},
            "surface_graupel": {"serialname": "mpsub_graupel", "shield": True},
            "condensation": {"serialname": "mpsub_cond", "shield": True},
            "deposition": {"serialname": "mpsub_dep", "shield": True},
            "evaporation": {"serialname": "mpsub_evap", "shield": True},
            "sublimation": {"serialname": "mpsub_sub", "shield": True},
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
        self.mpconfig.do_mp_table_emulation = True

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npy - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = SubMicrophysics(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
            inputs.pop("dt"),
            inputs.pop("convt"),
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
