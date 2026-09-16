import numpy as np

from ndsl import QuantityFactory, SubtileGridSizer
from ndsl.dsl.typing import Float, set_4d_field_size
from pyshield._config import TRACER_DIM
from pyshield.stencils.shallow_convection import (
    SAMFShalConvState,
    ScaleAwareMassFluxShallowConvection,
    ShallowConvectionConfig,
)
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py

FloatFieldShalConv = set_4d_field_size(7, Float)

SC_TRACER_DIM = "n_tracers_shal"


class TranslateShalConv(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "delp": {"serialname": "sc_delta", "shield": True},
            "prslp": {"serialname": "sc_prsl", "shield": True},
            "psp": {"serialname": "sc_pgr", "shield": True},
            "phil": {"serialname": "sc_phil", "shield": True},
            "qtr": {"serialname": "sc_clw", "shield": True},
            "q1": {"serialname": "sc_gq0", "shield": True},
            "t1": {"serialname": "sc_gt0", "shield": True},
            "u1": {"serialname": "sc_gu0", "shield": True},
            "v1": {"serialname": "sc_gv0", "shield": True},
            "rn": {"serialname": "sc_rain1", "shield": True},
            "kbot": {"serialname": "sc_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc_kcnv", "shield": True},
            "islimsk": {"serialname": "sc_islmsk", "shield": True},
            "garea": {"serialname": "sc_garea", "shield": True},
            "dot": {"serialname": "sc_vvl", "shield": True},
            "hpbl": {"serialname": "sc_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc_cnvw", "shield": True},
            "cnvc": {"serialname": "sc_cnvc", "shield": True},
        }
        self.in_vars["parameters"] = [
            "sc_clam_shal",
            "sc_c0s_shal",
            "sc_c1_shal",
            "sc_ncld",
            "sc_pgcon_shal",
            "sc_asolfac_shal",
            "sc_dtp",
            "sc_itc",
            "sc_ntchm",
            "sc_ntk",
            "sc_nsamftrac",
            "sc_ser_fscav",
        ]
        self.out_vars = {
            "delp": {"serialname": "sc_delta", "shield": True},
            "prslp": {"serialname": "sc_prsl", "shield": True},
            "psp": {"serialname": "sc_pgr", "shield": True},
            "phil": {"serialname": "sc_phil", "shield": True},
            "q1": {"serialname": "sc_gq0", "shield": True},
            "t1": {"serialname": "sc_gt0", "shield": True},
            "u1": {"serialname": "sc_gu0", "shield": True},
            "v1": {"serialname": "sc_gv0", "shield": True},
            "qtr": {"serialname": "sc_clw", "shield": True},
            "rn": {"serialname": "sc_rain1", "shield": True},
            "kbot": {"serialname": "sc_kbot", "shield": True, "index_variable": True},
            "ktop": {"serialname": "sc_ktop", "shield": True, "index_variable": True},
            "kcnv": {"serialname": "sc_kcnv", "shield": True},
            "dot": {"serialname": "sc_vvl", "shield": True},
            "hpbl": {"serialname": "sc_hpbl", "shield": True},
            "ud_mf": {"serialname": "sc_ud_mf", "shield": True},
            "dt_mf": {"serialname": "sc_dt_mf", "shield": True},
            "cnvw": {"serialname": "sc_cnvw", "shield": True},
            "cnvc": {"serialname": "sc_cnvc", "shield": True},
        }
        self.stencil_factory = stencil_factory

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=3,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )

        self.quantity_factory = QuantityFactory(
            sizer, backend=self.stencil_factory.backend
        )

        self.grid_indexing = stencil_factory.grid_indexing

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
        quantity_factory.add_data_dimensions(
            {
                SC_TRACER_DIM: inputs["sc_nsamftrac"] + 2,
            }
        )
        quantity_factory.add_data_dimensions(
            {
                TRACER_DIM: inputs["sc_nsamftrac"] + 4,
            }
        )

        self.make_storage_data_input_vars(inputs)
        config = ShallowConvectionConfig(
            dt_atmos=inputs.pop("sc_dtp"),
            ntke=int(inputs.pop("sc_ntk")),
            nsamftrac=int(inputs.pop("sc_nsamftrac")),
            ncld=int(inputs.pop("sc_ncld")),
            ntchm=int(inputs.pop("sc_ntchm")),
            ntiw=1,
            ntcw=2,
            ntcld=8,
            ntvap=0,
            itc=int(inputs.pop("sc_itc")),
            clam_shal=inputs.pop("sc_clam_shal"),
            c0s_shal=inputs.pop("sc_c0s_shal"),
            c1_shal=inputs.pop("sc_c1_shal"),
            pgcon_shal=inputs.pop("sc_pgcon_shal"),
            asolfac_shal=inputs.pop("sc_asolfac_shal"),
            fscav=inputs.pop("sc_ser_fscav"),
        )

        q1 = inputs.pop("q1")
        qtr = inputs.pop("qtr")
        shape = q1.shape + tuple([qtr.shape[3] + 2])
        qq = np.zeros((shape))
        qq[:, :, :, 0] = q1
        qq[:, :, :, 1:-1] = qtr
        inputs["qtr"] = qq
        state = SAMFShalConvState.init_from_storages(
            inputs,
            sizer=sizer,
            quantity_factory=quantity_factory,
        )
        self.compute_func = ScaleAwareMassFluxShallowConvection(
            self.stencil_factory,
            self.quantity_factory,
            config,
        )
        self.compute_func(state)

        inputs["delp"] = state.delp
        inputs["prslp"] = state.prslp
        inputs["psp"] = state.psp
        inputs["phil"] = state.phil
        inputs["q1"] = state.qtr[:, :, :, 0]
        inputs["t1"] = state.t1
        inputs["u1"] = state.u1
        inputs["v1"] = state.v1
        inputs["qtr"] = state.qtr[:, :, :, 1:-1]
        inputs["rn"] = state.rn
        inputs["kbot"] = state.kbot
        inputs["ktop"] = state.ktop
        inputs["kcnv"] = state.kcnv
        inputs["dot"] = state.dot
        inputs["hpbl"] = state.hpbl
        inputs["ud_mf"] = state.ud_mf
        inputs["dt_mf"] = state.dt_mf
        inputs["cnvw"] = state.cnvw
        inputs["cnvc"] = state.cnvc

        return self.slice_output(inputs)
