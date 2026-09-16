from ndsl import QuantityFactory, SubtileGridSizer
from pyshield.stencils.pbl import PBLConfig, SATMEDMFVDiffState, ScaleAwareTKEMoistEDMF
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslatePBL(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "dv": {"serialname": "pbl_dv", "shield": True},
            "du": {"serialname": "pbl_du", "shield": True},
            "tdt": {"serialname": "pbl_tdt", "shield": True},
            "rtg": {"serialname": "pbl_rtg", "shield": True},
            "u1": {"serialname": "pbl_u1", "shield": True},
            "v1": {"serialname": "pbl_v1", "shield": True},
            "t1": {"serialname": "pbl_t1", "shield": True},
            "q1": {"serialname": "pbl_q1", "shield": True},
            "hsw": {"serialname": "pbl_swh", "shield": True},
            "hlw": {"serialname": "pbl_hlw", "shield": True},
            "xmu": {"serialname": "pbl_xmu", "shield": True},
            "garea": {"serialname": "pbl_garea", "shield": True},
            "islimsk": {"serialname": "pbl_islmsk", "shield": True},
            "psk": {"serialname": "pbl_psk", "shield": True},
            "rbsoil": {"serialname": "pbl_rbsoil", "shield": True},
            "zorl": {"serialname": "pbl_zorl", "shield": True},
            "u10m": {"serialname": "pbl_u10m", "shield": True},
            "v10m": {"serialname": "pbl_v10m", "shield": True},
            "fm": {"serialname": "pbl_fm", "shield": True},
            "fh": {"serialname": "pbl_fh", "shield": True},
            "tsea": {"serialname": "pbl_tsea", "shield": True},
            "heat": {"serialname": "pbl_heat", "shield": True},
            "evap": {"serialname": "pbl_evap", "shield": True},
            "stress": {"serialname": "pbl_stress", "shield": True},
            "spd1": {"serialname": "pbl_wind", "shield": True},
            "kpbl": {"serialname": "pbl_kpbl", "shield": True, "index_variable": True},
            "prsi": {"serialname": "pbl_prsi", "shield": True},
            "delta": {"serialname": "pbl_delta", "shield": True},
            "prsl": {"serialname": "pbl_prsl", "shield": True},
            "prslk": {"serialname": "pbl_prslk", "shield": True},
            "phii": {"serialname": "pbl_phii", "shield": True},
            "phil": {"serialname": "pbl_phil", "shield": True},
            "dusfc": {"serialname": "pbl_dusfc", "shield": True},
            "dvsfc": {"serialname": "pbl_dvsfc", "shield": True},
            "dtsfc": {"serialname": "pbl_dtsfc", "shield": True},
            "dqsfc": {"serialname": "pbl_dqsfc", "shield": True},
            "hpbl": {"serialname": "pbl_hpbl", "shield": True},
            "kinver": {
                "serialname": "pbl_kinver",
                "shield": True,
                "index_variable": True,
            },
            "dkt": {"serialname": "pbl_dkt", "shield": True},
        }
        self.in_vars["parameters"] = [
            "pbl_ntrac",
            "pbl_ntcw",
            "pbl_ntiw",
            "pbl_ntke",
            "pbl_dtp",
            "pbl_dspheat",
            "pbl_xkzm_m",
            "pbl_xkzm_h",
            "pbl_xkzm_ml",
            "pbl_xkzm_hl",
            "pbl_xkzm_mi",
            "pbl_xkzm_hi",
            "pbl_xkzm_s",
            "pbl_xkzminv",
            "pbl_do_dk_hb19",
            "pbl_xkzm_lim",
            "pbl_xkgdx",
            "pbl_rlmn",
            "pbl_rlmx",
            "pbl_cap_k0_land",
        ]

        self.out_vars = {
            "du": {"serialname": "pbl_du", "shield": True},
            "dv": {"serialname": "pbl_dv", "shield": True},
            "tdt": {"serialname": "pbl_tdt", "shield": True},
            "rtg": {"serialname": "pbl_rtg", "shield": True},
            "kpbl": {"serialname": "pbl_kpbl", "shield": True, "index_variable": True},
            "dusfc": {"serialname": "pbl_dusfc", "shield": True},
            "dvsfc": {"serialname": "pbl_dvsfc", "shield": True},
            "dtsfc": {"serialname": "pbl_dtsfc", "shield": True},
            "dqsfc": {"serialname": "pbl_dqsfc", "shield": True},
            "hpbl": {"serialname": "pbl_hpbl", "shield": True},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing

    def compute(self, inputs):
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        self.make_storage_data_input_vars(inputs)

        config = PBLConfig()
        config.ntracers = int(inputs.pop("pbl_ntrac"))
        config.ntcw = int(inputs.pop("pbl_ntcw") - 1)
        config.ntiw = int(inputs.pop("pbl_ntiw") - 1)
        config.ntke = int(inputs.pop("pbl_ntke") - 1)
        config.xkzminv = 1.0
        print("tracers: ", config.ntracers, config.ntcw, config.ntiw, config.ntke)
        config.dt_atmos = inputs.pop("pbl_dtp")
        config.dspheat = inputs.pop("pbl_dspheat")
        config.xkzm_m = inputs.pop("pbl_xkzm_m")
        config.xkzm_h = inputs.pop("pbl_xkzm_h")
        config.xkzm_ml = inputs.pop("pbl_xkzm_ml")
        config.xkzm_hl = inputs.pop("pbl_xkzm_hl")
        config.xkzm_mi = inputs.pop("pbl_xkzm_mi")
        config.xkzm_hi = inputs.pop("pbl_xkzm_hi")
        config.xkzm_s = inputs.pop("pbl_xkzm_s")
        config.xkzminv = inputs.pop("pbl_xkzminv")
        config.do_dk_hb19 = inputs.pop("pbl_do_dk_hb19")
        config.xkzm_lim = inputs.pop("pbl_xkzm_lim")
        config.xkgdx = inputs.pop("pbl_xkgdx")
        config.rlmn = inputs.pop("pbl_rlmn")
        config.rlmx = inputs.pop("pbl_rlmx")
        config.cap_k0_land = inputs.pop("pbl_cap_k0_land")

        inputs["kpbl"] = inputs["kpbl"].astype(int)
        inputs["kinver"] = inputs["kinver"].astype(int)

        compute_func = ScaleAwareTKEMoistEDMF(
            self.stencil_factory,
            quantity_factory,
            inputs.pop("garea"),
            config,
        )
        inputs["dtdt"] = inputs.pop("tdt")
        state = SATMEDMFVDiffState.init_from_storages(
            inputs,
            sizer=sizer,
            quantity_factory=quantity_factory,
        )

        compute_func(state)

        inputs.pop("dtdt")

        inputs["du"] = state.du
        inputs["dv"] = state.dv
        inputs["tdt"] = state.dtdt
        inputs["rtg"] = state.rtg
        inputs["kpbl"] = state.kpbl
        inputs["dusfc"] = state.dusfc
        inputs["dvsfc"] = state.dvsfc
        inputs["dtsfc"] = state.dtsfc
        inputs["dqsfc"] = state.dqsfc
        inputs["hpbl"] = state.hpbl

        return self.slice_output(inputs)
