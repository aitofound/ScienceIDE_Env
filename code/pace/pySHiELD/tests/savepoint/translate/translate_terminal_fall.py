from ndsl import QuantityFactory, StencilFactory, SubtileGridSizer
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from pyshield.stencils.gfdl_cld_microphysics.terminal_fall import TerminalFall
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslateTerminalFall(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "tf_qv", "shield": True},
            "qliquid": {"serialname": "tf_ql", "shield": True},
            "qrain": {"serialname": "tf_qr", "shield": True},
            "qice": {"serialname": "tf_qi", "shield": True},
            "qsnow": {"serialname": "tf_qs", "shield": True},
            "qgraupel": {"serialname": "tf_qg", "shield": True},
            "temperature": {"serialname": "tf_pt", "shield": True},
            "delp": {"serialname": "tf_dp", "shield": True},
            "delz": {"serialname": "tf_dz", "shield": True},
            "ua": {"serialname": "tf_ua", "shield": True},
            "va": {"serialname": "tf_va", "shield": True},
            "wa": {"serialname": "tf_wa", "shield": True},
            "z_edge": {"serialname": "tf_ze", "shield": True},
            "z_terminal": {"serialname": "tf_zt", "shield": True},
            "column_energy_change": {"serialname": "tf_dte", "shield": True},
            "flux": {"serialname": "tf_pfi", "shield": True},
            "v_terminal": {"serialname": "tf_vt", "shield": True},
            "precipitation": {"serialname": "tf_i1", "shield": True},
        }

        self.in_vars["parameters"] = ["dt"]

        self.out_vars = {
            "qvapor": {"serialname": "tf_qv", "kend": self.config.npz, "shield": True},
            "qliquid": {"serialname": "tf_ql", "kend": self.config.npz, "shield": True},
            "qrain": {"serialname": "tf_qr", "kend": self.config.npz, "shield": True},
            "qice": {"serialname": "tf_qi", "kend": self.config.npz, "shield": True},
            "qsnow": {"serialname": "tf_qs", "kend": self.config.npz, "shield": True},
            "qgraupel": {
                "serialname": "tf_qg",
                "kend": self.config.npz,
                "shield": True,
            },
            "temperature": {
                "serialname": "tf_pt",
                "kend": self.config.npz,
                "shield": True,
            },
            "ua": {"serialname": "tf_ua", "kend": self.config.npz, "shield": True},
            "va": {"serialname": "tf_va", "kend": self.config.npz, "shield": True},
            "wa": {"serialname": "tf_wa", "kend": self.config.npz, "shield": True},
            "flux": {"serialname": "tf_pfi", "kend": self.config.npz, "shield": True},
            "precipitation": {"serialname": "tf_i1", "shield": True},
            "column_energy_change": {"serialname": "tf_dte", "shield": True},
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

        inputs["tracer"] = "ice"

        compute_func = TerminalFall(
            self.stencil_factory,
            self.quantity_factory,
            self.mpconfig,
            timestep=inputs.pop("dt"),
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
