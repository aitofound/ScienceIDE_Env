from ndsl import Quantity, QuantityFactory, StencilFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.typing import Int
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.sedimentation import (
    Sedimentation,
    calc_terminal_velocity_ice,
    calc_terminal_velocity_rsg,
    init_zeros_heat_cap_latent_heat_precip,
    sedi_melt,
    sedi_melt_python,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class InitSed:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config,
    ):
        self._idx = stencil_factory.grid_indexing

        self._init_zeros_heat_cap_latent_heat_precip = (
            stencil_factory.from_origin_domain(
                func=init_zeros_heat_cap_latent_heat_precip,
                externals={
                    "c1_vap": config.c1_vap,
                    "c1_liq": config.c1_liq,
                    "c1_ice": config.c1_ice,
                    "lv00": config.lv00,
                    "li00": config.li00,
                    "li20": config.li20,
                    "d1_vap": config.d1_vap,
                    "d1_ice": config.d1_ice,
                    "t_wfr": config.t_wfr,
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
        temperature,
        icpk,
        preflux_water,
        preflux_rain,
        preflux_ice,
        preflux_snow,
        preflux_graupel,
        vterminal_water,
        vterminal_rain,
        vterminal_ice,
        vterminal_snow,
        vterminal_graupel,
        column_water,
        column_rain,
        column_ice,
        column_snow,
        column_graupel,
    ):
        self._init_zeros_heat_cap_latent_heat_precip(
            qvapor,
            qliquid,
            qrain,
            qice,
            qsnow,
            qgraupel,
            temperature,
            icpk,
            preflux_water,
            preflux_rain,
            preflux_ice,
            preflux_snow,
            preflux_graupel,
            vterminal_water,
            vterminal_rain,
            vterminal_ice,
            vterminal_snow,
            vterminal_graupel,
            column_water,
            column_rain,
            column_ice,
            column_snow,
            column_graupel,
        )


class CalcVT:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        config,
    ):
        self._idx = stencil_factory.grid_indexing
        self.config = config
        if config.do_psd_ice_fall is False:
            self._calc_terminal_ice_velocity = stencil_factory.from_origin_domain(
                func=calc_terminal_velocity_ice,
                externals={
                    "ifflag": config.ifflag,
                    "constant_v": config.const_vi,
                    "v_fac": config.vi_fac,
                    "v_max": config.vi_max,
                    "aa": -4.14122e-5,
                    "bb": -0.00538922,
                    "cc": -0.0516344,
                    "dd": 0.00216078,
                    "ee": 1.9714,
                },
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

        if False in [
            config.const_vr,
            config.const_vi,
            config.const_vs,
            config.const_vg,
        ]:
            self._calc_terminal_rsg_velocity = stencil_factory.from_origin_domain(
                func=calc_terminal_velocity_rsg,
                externals={"const_v": False},
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

        if True in [config.const_vr, config.const_vi, config.const_vs, config.const_vg]:
            self._calc_terminal_rsg_velocity_const = stencil_factory.from_origin_domain(
                func=calc_terminal_velocity_rsg,
                externals={"const_v": True},
                origin=self._idx.origin_compute(),
                domain=self._idx.domain_compute(),
            )

    def __call__(
        self,
        qfall,
        density,
        temperature,
        vterminal,
        density_factor,
        mode,
    ):
        if mode == "ice":
            if self.config.do_psd_ice_fall:
                if self.config.const_vi is False:
                    self._calc_terminal_rsg_velocity(
                        qfall,
                        density,
                        density_factor,
                        vterminal,
                        self.config.vi_fac,
                        self.config.tvai,
                        self.config.tvbi,
                        self.config.mui,
                        self.config.blini,
                        self.config.vi_max,
                    )
                else:
                    self._calc_terminal_rsg_velocity_const(
                        qfall,
                        density,
                        density_factor,
                        vterminal,
                        self.config.vi_fac,
                        self.config.tvai,
                        self.config.tvbi,
                        self.config.mui,
                        self.config.blini,
                        self.config.vi_max,
                    )
            else:
                self._calc_terminal_ice_velocity(qfall, temperature, density, vterminal)
        elif mode == "snow":
            if self.config.const_vs is False:
                self._calc_terminal_rsg_velocity(
                    qfall,
                    density,
                    density_factor,
                    vterminal,
                    self.config.vs_fac,
                    self.config.tvas,
                    self.config.tvbs,
                    self.config.mus,
                    self.config.blins,
                    self.config.vs_max,
                )
            else:
                self._calc_terminal_rsg_velocity_const(
                    qfall,
                    density,
                    density_factor,
                    vterminal,
                    self.config.vs_fac,
                    self.config.tvas,
                    self.config.tvbs,
                    self.config.mus,
                    self.config.blins,
                    self.config.vs_max,
                )
        elif mode == "rain":
            if self.config.const_vr is False:
                self._calc_terminal_rsg_velocity(
                    qfall,
                    density,
                    density_factor,
                    vterminal,
                    self.config.vr_fac,
                    self.config.tvar,
                    self.config.tvbr,
                    self.config.mur,
                    self.config.blinr,
                    self.config.vr_max,
                )
            else:
                self._calc_terminal_rsg_velocity_const(
                    qfall,
                    density,
                    density_factor,
                    vterminal,
                    self.config.vr_fac,
                    self.config.tvar,
                    self.config.tvbr,
                    self.config.mur,
                    self.config.blinr,
                    self.config.vr_max,
                )
        else:
            raise ValueError(f"calc_vt mode {mode} not ice or snow")


class SediMelt:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config,
        timestep,
    ):
        self._idx = stencil_factory.grid_indexing
        self.config = config

        self.li00 = config.li00
        self._timestep = timestep

        self._idx = stencil_factory.grid_indexing
        self._is_ = self._idx.isc
        self._ie = self._idx.iec
        self._js = self._idx.jsc
        self._je = self._idx.jec
        self._ks = 0
        self._ke = config.npz - 1
        self.c1_vap = config.c1_vap
        self.c1_liq = config.c1_liq
        self.c1_ice = config.c1_ice
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_INTERFACE_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(self._idx.domain[2] + 1):
            self._k_mask[:, :, k] = k

        self._sedi_melt_ice = stencil_factory.from_origin_domain(
            func=sedi_melt,
            externals={
                "c1_vap": config.c1_vap,
                "c1_liq": config.c1_liq,
                "c1_ice": config.c1_ice,
                "li00": config.li00,
                "timestep": self._timestep,
                "mode": 0,
                "tau_mlt": config.tau_imlt,
                "k_end": config.npz,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )
        self._sedi_melt_snow = stencil_factory.from_origin_domain(
            func=sedi_melt,
            externals={
                "c1_vap": config.c1_vap,
                "c1_liq": config.c1_liq,
                "c1_ice": config.c1_ice,
                "li00": config.li00,
                "timestep": self._timestep,
                "mode": 1,
                "tau_mlt": config.tau_smlt,
                "k_end": config.npz,
            },
            origin=self._idx.origin_compute(),
            domain=self._idx.domain_compute(),
        )
        self._sedi_melt_graupel = stencil_factory.from_origin_domain(
            func=sedi_melt,
            externals={
                "c1_vap": config.c1_vap,
                "c1_liq": config.c1_liq,
                "c1_ice": config.c1_ice,
                "li00": config.li00,
                "timestep": self._timestep,
                "mode": 2,
                "tau_mlt": config.tau_gmlt,
                "k_end": config.npz,
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
        cvm,
        temperature,
        delp,
        z_edge,
        z_terminal,
        z_surface,
        icpk,
        vterminal,
        column_rain,
        mode: str,
        stencil: bool = False,
    ):
        if self.config.do_sedi_melt:
            if stencil:
                if mode == "ice":
                    self._sedi_melt_ice(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        vterminal,
                        column_rain,
                        icpk,
                        self._k_mask,
                    )
                elif mode == "snow":
                    self._sedi_melt_snow(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        vterminal,
                        column_rain,
                        icpk,
                        self._k_mask,
                    )
                elif mode == "graupel":
                    self._sedi_melt_graupel(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        vterminal,
                        column_rain,
                        icpk,
                        self._k_mask,
                    )
                else:
                    raise ValueError(f"sedi_melt mode {mode} not ice, snow, or graupel")
            else:
                if mode == "ice":
                    sedi_melt_python(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        self._timestep,
                        vterminal,
                        column_rain,
                        self.config.tau_imlt,
                        icpk,
                        self.li00,
                        self.c1_vap,
                        self.c1_liq,
                        self.c1_ice,
                        self._ks,
                        self._ke,
                        self._is_,
                        self._ie,
                        self._js,
                        self._je,
                        mode,
                    )
                elif mode == "snow":
                    sedi_melt_python(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        self._timestep,
                        vterminal,
                        column_rain,
                        self.config.tau_smlt,
                        icpk,
                        self.li00,
                        self.c1_vap,
                        self.c1_liq,
                        self.c1_ice,
                        self._ks,
                        self._ke,
                        self._is_,
                        self._ie,
                        self._js,
                        self._je,
                        mode,
                    )
                elif mode == "graupel":
                    sedi_melt_python(
                        qvapor,
                        qliquid,
                        qrain,
                        qice,
                        qsnow,
                        qgraupel,
                        cvm,
                        temperature,
                        delp,
                        z_edge,
                        z_terminal,
                        z_surface,
                        self._timestep,
                        vterminal,
                        column_rain,
                        self.config.tau_gmlt,
                        icpk,
                        self.li00,
                        self.c1_vap,
                        self.c1_liq,
                        self.c1_ice,
                        self._ks,
                        self._ke,
                        self._is_,
                        self._ie,
                        self._js,
                        self._je,
                        mode,
                    )
                else:
                    raise ValueError(f"sedi_melt mode {mode} not ice, snow, or graupel")


class TranslateSedimentation(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "sd_qv", "shield": True},
            "qliquid": {"serialname": "sd_ql", "shield": True},
            "qrain": {"serialname": "sd_qr", "shield": True},
            "qice": {"serialname": "sd_qi", "shield": True},
            "qsnow": {"serialname": "sd_qs", "shield": True},
            "qgraupel": {"serialname": "sd_qg", "shield": True},
            "temperature": {"serialname": "sd_pt", "shield": True},
            "delp": {"serialname": "sd_delp", "shield": True},
            "delz": {"serialname": "sd_delz", "shield": True},
            "density": {"serialname": "sd_den", "shield": True},
            "density_factor": {"serialname": "sd_denfac", "shield": True},
            "ua": {"serialname": "sd_u", "shield": True},
            "va": {"serialname": "sd_v", "shield": True},
            "wa": {"serialname": "sd_w", "shield": True},
            "column_energy_change": {"serialname": "sd_dte", "shield": True},
            "preflux_water": {"serialname": "sd_pfw", "shield": True},
            "preflux_rain": {"serialname": "sd_pfr", "shield": True},
            "preflux_ice": {"serialname": "sd_pfi", "shield": True},
            "preflux_snow": {"serialname": "sd_pfs", "shield": True},
            "preflux_graupel": {"serialname": "sd_pfg", "shield": True},
            "vterminal_water": {"serialname": "sd_vtw", "shield": True},
            "vterminal_rain": {"serialname": "sd_vtr", "shield": True},
            "vterminal_ice": {"serialname": "sd_vti", "shield": True},
            "vterminal_snow": {"serialname": "sd_vts", "shield": True},
            "vterminal_graupel": {"serialname": "sd_vtg", "shield": True},
            "column_water": {"serialname": "sd_w1", "shield": True},
            "column_rain": {"serialname": "sd_r1", "shield": True},
            "column_ice": {"serialname": "sd_i1", "shield": True},
            "column_snow": {"serialname": "sd_s1", "shield": True},
            "column_graupel": {"serialname": "sd_g1", "shield": True},
        }

        self.in_vars["parameters"] = ["dt"]

        self.out_vars = {
            "qvapor": {"serialname": "sd_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "sd_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "sd_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "sd_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "sd_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "sd_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "sd_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "ua": {"serialname": "sd_u", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "sd_v", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "sd_w", "kend": self.config.npz, "shield": True},
            "preflux_water": {
                "serialname": "sd_pfw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "sd_pfr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "sd_pfi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "sd_pfs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "sd_pfg",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_water": {
                "serialname": "sd_vtw",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_rain": {
                "serialname": "sd_vtr",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_ice": {
                "serialname": "sd_vti",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_snow": {
                "serialname": "sd_vts",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_graupel": {
                "serialname": "sd_vtg",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_water": {"serialname": "sd_w1", "shield": True},
            "column_rain": {"serialname": "sd_r1", "shield": True},
            "column_ice": {"serialname": "sd_i1", "shield": True},
            "column_snow": {"serialname": "sd_s1", "shield": True},
            "column_graupel": {"serialname": "sd_g1", "shield": True},
            "column_energy_change": {
                "serialname": "sd_dte",
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

        compute_func = Sedimentation(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
            timestep=inputs.pop("dt"),
        )

        for var in inputs.keys():
            if len(inputs[var].shape) == 3:
                inputs[var] = Quantity(
                    inputs[var],
                    dims=[I_DIM, J_DIM, K_DIM],
                    units="unknown",
                    backend=self.quantity_factory.backend,
                )
            elif len(inputs[var].shape) == 2:
                inputs[var] = Quantity(
                    inputs[var],
                    dims=[I_DIM, J_DIM],
                    units="unknown",
                    backend=self.quantity_factory.backend,
                )
            else:
                raise TypeError(
                    f"input data with strange len: {len(inputs[var].shape)}"
                )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateSediMelt(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "sm_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "sm_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "sm_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "sm_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "sm_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "sm_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "cvm": {"serialname": "sm_cv", "kend": self.config.npz, "shield": True},
            "temperature": {
                "serialname": "sm_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "delp": {"serialname": "sm_dp", "kend": self.config.npz, "shield": True},
            "z_edge": {
                "serialname": "sm_ze",
                "kend": self.config.npz + 1,
                "shield": True,
            },
            "z_terminal": {
                "serialname": "sm_zt",
                "kend": self.config.npz + 1,
                "shield": True,
            },
            "z_surface": {"serialname": "sm_zs", "shield": True},
            "icpk": {"serialname": "sm_ic", "kend": self.config.npz, "shield": True},
            "vterminal": {
                "serialname": "sm_vt",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_rain": {"serialname": "sm_r1", "shield": True},
        }
        self.in_vars["parameters"] = ["dt"]
        self.out_vars = {
            "qrain": {"serialname": "sm_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "sm_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "sm_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "sm_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "sm_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "cvm": {"serialname": "sm_cv", "kend": self.config.npz, "shield": True},
            "column_rain": {"serialname": "sm_r1", "shield": True},
        }

        self.stencil_factory = stencil_factory
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

        inputs["mode"] = "snow"
        inputs["stencil"] = True

        compute_func = SediMelt(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
            timestep=inputs.pop("dt"),
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateCalcVTIce(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "qfall": {"serialname": "vti_qi", "kend": self.config.npz, "shield": True},
            "density": {"serialname": "vti_den", "shield": True},
            "density_factor": {"serialname": "vti_denfac", "shield": True},
            "temperature": {
                "serialname": "vti_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal": {
                "serialname": "vti_vti",
                "kend": self.config.npz,
                "shield": True,
            },
        }

        self.out_vars = {
            "vterminal": {
                "serialname": "vti_vti",
                "kend": self.config.npz,
                "shield": True,
            },
        }

        self.stencil_factory = stencil_factory
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

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        inputs["mode"] = "ice"

        compute_func = CalcVT(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateCalcVTSnow(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "qfall": {"serialname": "vts_qs", "kend": self.config.npz, "shield": True},
            "density": {"serialname": "vts_den", "shield": True},
            "density_factor": {"serialname": "vts_denfac", "shield": True},
            "temperature": {
                "serialname": "vts_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal": {
                "serialname": "vts_vts",
                "kend": self.config.npz,
                "shield": True,
            },
        }

        self.out_vars = {
            "vterminal": {
                "serialname": "vts_vts",
                "kend": self.config.npz,
                "shield": True,
            },
        }

        self.stencil_factory = stencil_factory
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

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        inputs["mode"] = "snow"

        compute_func = CalcVT(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)


class TranslateInitSed(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "is_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "is_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "is_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "is_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "is_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "is_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "is_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "icpk": {"serialname": "is_icpk", "kend": self.config.npz, "shield": True},
            "preflux_water": {
                "serialname": "is_pfw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "is_pfr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "is_pfi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "is_pfs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "is_pfg",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_water": {
                "serialname": "is_vtw",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_rain": {
                "serialname": "is_vtr",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_ice": {
                "serialname": "is_vti",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_snow": {
                "serialname": "is_vts",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_graupel": {
                "serialname": "is_vtg",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_water": {"serialname": "is_w1", "shield": True},
            "column_rain": {"serialname": "is_r1", "shield": True},
            "column_ice": {"serialname": "is_i1", "shield": True},
            "column_snow": {"serialname": "is_s1", "shield": True},
            "column_graupel": {"serialname": "is_g1", "shield": True},
        }

        self.out_vars = {
            "icpk": {"serialname": "is_icpk", "kend": self.config.npz, "shield": True},
            "preflux_water": {
                "serialname": "is_pfw",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_rain": {
                "serialname": "is_pfr",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_ice": {
                "serialname": "is_pfi",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_snow": {
                "serialname": "is_pfs",
                "kend": self.config.npz,
                "shield": True,
            },
            "preflux_graupel": {
                "serialname": "is_pfg",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_water": {
                "serialname": "is_vtw",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_rain": {
                "serialname": "is_vtr",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_ice": {
                "serialname": "is_vti",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_snow": {
                "serialname": "is_vts",
                "kend": self.config.npz,
                "shield": True,
            },
            "vterminal_graupel": {
                "serialname": "is_vtg",
                "kend": self.config.npz,
                "shield": True,
            },
            "column_water": {"serialname": "is_w1", "shield": True},
            "column_rain": {"serialname": "is_r1", "shield": True},
            "column_ice": {"serialname": "is_i1", "shield": True},
            "column_snow": {"serialname": "is_s1", "shield": True},
            "column_graupel": {"serialname": "is_g1", "shield": True},
        }

        self.stencil_factory = stencil_factory
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

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = InitSed(
            self.stencil_factory,
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
