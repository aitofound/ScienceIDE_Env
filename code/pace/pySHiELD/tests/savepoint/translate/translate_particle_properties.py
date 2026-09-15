from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.dsl.typing import FloatField
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.gfdl_cld_mp_driver import (
    calculate_particle_properties,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class CalcParticleProperties:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config: GFDLCloudMPConfig,
    ):
        self._idx = stencil_factory.grid_indexing

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

    def __call__(
        self,
        qliquid: FloatField,
        qrain: FloatField,
        qice: FloatField,
        qsnow: FloatField,
        qgraupel: FloatField,
        density: FloatField,
        particle_concentration_w: FloatField,
        particle_concentration_r: FloatField,
        particle_concentration_i: FloatField,
        particle_concentration_s: FloatField,
        particle_concentration_g: FloatField,
        effective_diameter_w: FloatField,
        effective_diameter_r: FloatField,
        effective_diameter_i: FloatField,
        effective_diameter_s: FloatField,
        effective_diameter_g: FloatField,
        optical_extinction_w: FloatField,
        optical_extinction_r: FloatField,
        optical_extinction_i: FloatField,
        optical_extinction_s: FloatField,
        optical_extinction_g: FloatField,
        radar_reflectivity_w: FloatField,
        radar_reflectivity_r: FloatField,
        radar_reflectivity_i: FloatField,
        radar_reflectivity_s: FloatField,
        radar_reflectivity_g: FloatField,
        terminal_velocity_w: FloatField,
        terminal_velocity_r: FloatField,
        terminal_velocity_i: FloatField,
        terminal_velocity_s: FloatField,
        terminal_velocity_g: FloatField,
    ):
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


class TranslateParticleProperties(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qliquid": {"serialname": "pp_ql", "shield": True},
            "qrain": {"serialname": "pp_qr", "shield": True},
            "qice": {"serialname": "pp_qi", "shield": True},
            "qsnow": {"serialname": "pp_qs", "shield": True},
            "qgraupel": {"serialname": "pp_qg", "shield": True},
            "density": {"serialname": "pp_den", "shield": True},
            "particle_concentration_w": {
                "serialname": "pp_pcw",
                "shield": True,
            },
            "effective_diameter_w": {
                "serialname": "pp_edw",
                "shield": True,
            },
            "optical_extinction_w": {
                "serialname": "pp_oew",
                "shield": True,
            },
            "radar_reflectivity_w": {
                "serialname": "pp_rrw",
                "shield": True,
            },
            "terminal_velocity_w": {
                "serialname": "pp_tvw",
                "shield": True,
            },
            "particle_concentration_r": {
                "serialname": "pp_pcr",
                "shield": True,
            },
            "effective_diameter_r": {
                "serialname": "pp_edr",
                "shield": True,
            },
            "optical_extinction_r": {
                "serialname": "pp_oer",
                "shield": True,
            },
            "radar_reflectivity_r": {
                "serialname": "pp_rrr",
                "shield": True,
            },
            "terminal_velocity_r": {
                "serialname": "pp_tvr",
                "shield": True,
            },
            "particle_concentration_i": {
                "serialname": "pp_pci",
                "shield": True,
            },
            "effective_diameter_i": {
                "serialname": "pp_edi",
                "shield": True,
            },
            "optical_extinction_i": {
                "serialname": "pp_oei",
                "shield": True,
            },
            "radar_reflectivity_i": {
                "serialname": "pp_rri",
                "shield": True,
            },
            "terminal_velocity_i": {
                "serialname": "pp_tvi",
                "shield": True,
            },
            "particle_concentration_s": {
                "serialname": "pp_pcs",
                "shield": True,
            },
            "effective_diameter_s": {
                "serialname": "pp_eds",
                "shield": True,
            },
            "optical_extinction_s": {
                "serialname": "pp_oes",
                "shield": True,
            },
            "radar_reflectivity_s": {
                "serialname": "pp_rrs",
                "shield": True,
            },
            "terminal_velocity_s": {
                "serialname": "pp_tvs",
                "shield": True,
            },
            "particle_concentration_g": {
                "serialname": "pp_pcg",
                "shield": True,
            },
            "effective_diameter_g": {
                "serialname": "pp_edg",
                "shield": True,
            },
            "optical_extinction_g": {
                "serialname": "pp_oeg",
                "shield": True,
            },
            "radar_reflectivity_g": {
                "serialname": "pp_rrg",
                "shield": True,
            },
            "terminal_velocity_g": {
                "serialname": "pp_tvg",
                "shield": True,
            },
        }

        self.out_vars = {
            "particle_concentration_w": {
                "serialname": "pp_pcw",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_w": {
                "serialname": "pp_edw",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_w": {
                "serialname": "pp_oew",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_w": {
                "serialname": "pp_rrw",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_w": {
                "serialname": "pp_tvw",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_r": {
                "serialname": "pp_pcr",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_r": {
                "serialname": "pp_edr",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_r": {
                "serialname": "pp_oer",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_r": {
                "serialname": "pp_rrr",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_r": {
                "serialname": "pp_tvr",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_i": {
                "serialname": "pp_pci",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_i": {
                "serialname": "pp_edi",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_i": {
                "serialname": "pp_oei",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_i": {
                "serialname": "pp_rri",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_i": {
                "serialname": "pp_tvi",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_s": {
                "serialname": "pp_pcs",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_s": {
                "serialname": "pp_eds",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_s": {
                "serialname": "pp_oes",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_s": {
                "serialname": "pp_rrs",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_s": {
                "serialname": "pp_tvs",
                "kend": self.config.npz,
                "shield": True,
            },
            "particle_concentration_g": {
                "serialname": "pp_pcg",
                "kend": self.config.npz,
                "shield": True,
            },
            "effective_diameter_g": {
                "serialname": "pp_edg",
                "kend": self.config.npz,
                "shield": True,
            },
            "optical_extinction_g": {
                "serialname": "pp_oeg",
                "kend": self.config.npz,
                "shield": True,
            },
            "radar_reflectivity_g": {
                "serialname": "pp_rrg",
                "kend": self.config.npz,
                "shield": True,
            },
            "terminal_velocity_g": {
                "serialname": "pp_tvg",
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

        compute_func = CalcParticleProperties(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
