from ndsl import QuantityFactory, SubtileGridSizer
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.typing import Int
from pyshield.stencils.pbl.mfpblt import PBLMassFlux
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslateMFPBLT(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, config, stencil_factory):
        super().__init__(grid, config, stencil_factory)
        self.in_vars["data_vars"] = {
            "cnvflg": {"shield": True, "serialname": "pcnvflg"},
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
            "gdx": {"shield": True},
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "vpert": {"shield": True},
            "buo": {"serialname": "buou", "shield": True},
            "xmf": {"shield": True},
            "tcko": {"shield": True},
            "qcko": {"shield": True},
            "ucko": {"shield": True},
            "vcko": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
        }
        self.in_vars["parameters"] = [
            "kmpbl",
            "ntcw",
            "dt2",
            "ntrac1",
        ]

        self.out_vars = {
            "hpbl": {"shield": True},
            "kpbl": {"shield": True, "index_variable": True},
            "buo": {"serialname": "buou", "shield": True},
            "xmf": {"shield": True},
            "tcko": {"shield": True},
            "qcko": {"shield": True},
            "ucko": {"shield": True},
            "vcko": {"shield": True},
            "xlamue": {"shield": True, "kend": config.npz - 1},
        }
        self.stencil_factory = stencil_factory
        self.grid_indexing = self.stencil_factory.grid_indexing
        self.max_error = 1e-200

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
        for k in range(self.stencil_factory.grid_indexing.domain[2]):
            k_mask[:, :, k] = k
        self.make_storage_data_input_vars(inputs)
        inputs.pop("t1")

        cnvflg = quantity_factory.from_array(
            data=inputs.pop("cnvflg"),
            dims=[I_DIM, J_DIM],
            units="",
        )
        dt2 = (inputs["dt2"],)
        ntcw = (int(inputs["ntcw"]),)
        ntrac1 = (int(inputs["ntrac1"]),)
        kmpbl = (int(inputs["kmpbl"]),)
        print(dt2, ntcw, ntrac1, kmpbl)

        compute_func = PBLMassFlux(
            self.stencil_factory,
            quantity_factory,
            dt2=inputs.pop("dt2"),
            ntcw=int(inputs.pop("ntcw")) - 1,
            ntrac1=int(inputs.pop("ntrac1")),
            kmpbl=int(inputs.pop("kmpbl")),
        )

        compute_func(**inputs, k_mask=k_mask, cnvflg=cnvflg)

        return self.slice_output(inputs)
