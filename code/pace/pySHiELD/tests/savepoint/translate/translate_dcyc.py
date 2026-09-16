from pyshield.stencils.physics import interpolate_radiation
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class TranslateRadInterp(TranslatePhysicsFortranData2Py):
    def __init__(self, grid, namelist, stencil_factory):
        super().__init__(grid, namelist, stencil_factory)

        self.in_vars["data_vars"] = {
            "sinlat": {"shield": True},
            "coslat": {"shield": True},
            "xlon": {"shield": True},
            "coszen": {"shield": True},
            "t_sea": {"serialname": "tsurf", "shield": True},
            "t_surface": {"serialname": "tgrs", "shield": True},
            "t_surface_longwave": {"serialname": "tsflw", "shield": True},
            "sfcemis": {"serialname": "semis", "shield": True},
            "sfcdsw": {"shield": True},
            "sfcnsw": {"shield": True},
            "sfcdlw": {"shield": True},
            "swh": {"serialname": "htrsw", "shield": True},
            "swhc": {"shield": True},
            "hlw": {"serialname": "htrlw", "shield": True},
            "hlwc": {"serialname": "lwhc", "shield": True},
            "sfcnirbmu": {"shield": True, "serialname": "nirbmui"},
            "sfcnirdfu": {"shield": True, "serialname": "nirdfui"},
            "sfcvisbmu": {"shield": True, "serialname": "visbmui"},
            "sfcvisdfu": {"shield": True, "serialname": "visdfui"},
            "sfcnirbmd": {"shield": True, "serialname": "nirbmdi"},
            "sfcnirdfd": {"shield": True, "serialname": "nirdfdi"},
            "sfcvisbmd": {"shield": True, "serialname": "visbmdi"},
            "sfcvisdfd": {"shield": True, "serialname": "visdfdi"},
            "dtdt": {"shield": True},
            "dtdtc": {"shield": True},
            "xmu": {"shield": True},
            "xcosz": {"shield": True},
            "adjsfcdsw": {"shield": True},
            "adjsfcnsw": {"shield": True},
            "adjsfcdlw": {"shield": True},
            "adjsfculw": {"shield": True},
            "adjnirbmu": {"shield": True},
            "adjnirdfu": {"shield": True},
            "adjvisbmu": {"shield": True},
            "adjvisdfu": {"shield": True},
            "adjnirbmd": {"shield": True},
            "adjnirdfd": {"shield": True},
            "adjvisbmd": {"shield": True},
            "adjvisdfd": {"shield": True},
        }
        self.in_vars["parameters"] = ["solhr", "slag", "sdec", "cdec", "daily_mean"]
        self.out_vars = {
            "dtdt": {"shield": True},
            "dtdtc": {"shield": True},
            "xmu": {"shield": True},
            "xcosz": {"shield": True},
            "adjsfcdsw": {"shield": True},
            "adjsfcnsw": {"shield": True},
            "adjsfcdlw": {"shield": True},
            "adjsfculw": {"shield": True},
            "adjnirbmu": {"shield": True},
            "adjnirdfu": {"shield": True},
            "adjvisbmu": {"shield": True},
            "adjvisdfu": {"shield": True},
            "adjnirbmd": {"shield": True},
            "adjnirdfd": {"shield": True},
            "adjvisbmd": {"shield": True},
            "adjvisdfd": {"shield": True},
        }
        self.grid_indexing = stencil_factory.grid_indexing
        self._daily_mean = self.config.daily_mean
        self.compute_func = stencil_factory.from_origin_domain(
            interpolate_radiation,
            externals={"daily_mean": self.config.daily_mean},
            origin=self.grid_indexing.origin_full(),
            domain=self.grid_indexing.domain_full(),
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        assert self._daily_mean == inputs.pop("daily_mean")
        self.compute_func(**inputs)
        return self.slice_output(inputs)
