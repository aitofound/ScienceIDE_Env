from ndsl import QuantityFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.typing import Int
from pyshield.stencils.pbl.mfscu import StratocumulusMassFlux
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslateMFSCU(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "cnvflg": {"shield": True, "serialname": "scuflg"},
            "zl": {"shield": True},
            "zm": {"shield": True},
            "q1": {"shield": True},
            "t1": {"shield": True},
            "u1": {"shield": True},
            "v1": {"shield": True},
            "plyr": {"shield": True},
            "pix": {"shield": True},
            "thlx": {"shield": True},
            "thvx": {"shield": True},
            "thlvx": {"shield": True},
            "gdx": {"shield": True},
            "thetae": {"shield": True},
            "radj": {"shield": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "radmin": {"shield": True},
            "buo": {"serialname": "buod", "shield": True},
            "xmfd": {"shield": True},
            "tcdo": {"shield": True},
            "qcdo": {"shield": True},
            "ucdo": {"shield": True},
            "vcdo": {"shield": True},
            "xlamde": {"shield": True, "kend": config.npz - 1},
        }
        self.in_vars["parameters"] = [
            "kmscu",
            "ntcw",
            "dt2",
            "ntrac1",
        ]

        self.out_vars = {
            "radj": {"shield": True},
            "krad": {"shield": True, "index_variable": True},
            "mrad": {"shield": True, "index_variable": True},
            "buo": {"serialname": "buod", "shield": True},
            "xmfd": {"shield": True},
            "tcdo": {"shield": True},
            "qcdo": {"shield": True},
            "ucdo": {"shield": True},
            "vcdo": {"shield": True},
            "xlamde": {"shield": True, "kend": config.npz - 1},
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

        k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )
        for k in range(1, self.stencil_factory.grid_indexing.domain[2] + 1):
            k_mask[:, :, k] = k
        self.make_storage_data_input_vars(inputs)
        inputs.pop("t1")
        cnvflg = quantity_factory.from_array(
            data=inputs.pop("cnvflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )
        mrad = quantity_factory.from_array(
            data=inputs.pop("mrad"),
            dims=[I_DIM, J_DIM],
            units="",
        )
        zm = quantity_factory.from_array(
            data=inputs.pop("zm"),
            dims=[I_DIM, J_DIM, K_DIM],
            units="",
        )
        ntrac1 = int(inputs.pop("ntrac1"))

        compute_func = StratocumulusMassFlux(
            self.stencil_factory,
            quantity_factory,
            dt2=inputs.pop("dt2"),
            ntcw=int(inputs.pop("ntcw") - 1),
            ntracers=ntrac1 + 1,
            ntrac1=ntrac1,
            kmscu=int(inputs.pop("kmscu") - 1),
            ntke=ntrac1,
        )

        compute_func(**inputs, k_mask=k_mask, cnvflg=cnvflg, mrad=mrad, zm=zm)
        inputs["mrad"] = mrad

        return self.slice_output(inputs)
