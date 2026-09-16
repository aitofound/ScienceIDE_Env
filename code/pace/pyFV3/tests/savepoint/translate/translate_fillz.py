import numpy as np
from f90nml import Namelist

from ndsl import StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.stencils.testing import pad_field_in_j
from pyfv3.stencils import fillz
from pyfv3.testing import TranslateDycoreFortranData2Py
from pyfv3.tracers import FVTracersAxisName, default_ai2_tracers


class TranslateFillz(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "dp2": {"istart": grid.is_, "iend": grid.ie, "axis": 1},
            "q2tracers": {"istart": grid.is_, "iend": grid.ie, "axis": 1},
        }
        self.in_vars["parameters"] = ["nq"]
        self.out_vars = {
            "q2tracers": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.js,
                "axis": 1,
            }
        }
        self.max_error = 1e-13
        self.ignore_near_zero_errors = {"q2tracers": True}
        self.stencil_factory = stencil_factory
        self.quantity_factory = grid.quantity_factory

    def make_storage_data_input_vars(
        self,
        inputs,
        storage_vars=None,
    ) -> None:
        default_ai2_tracers(self.quantity_factory)
        if storage_vars is None:
            storage_vars = self.storage_vars()
        info = storage_vars["dp2"]
        inputs["dp2"] = self.make_storage_data(
            np.squeeze(inputs["dp2"]), istart=info["istart"], axis=info["axis"]
        )

        inputs["tracers"] = {}
        info = storage_vars["q2tracers"]
        for i in range(int(inputs["nq"])):
            inputs["tracers"][i] = self.make_storage_data(
                np.squeeze(inputs["q2tracers"][:, :, i]),
                istart=info["istart"],
                axis=info["axis"],
            )
        del inputs["q2tracers"]

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        for name, value in tuple(inputs.items()):
            if hasattr(value, "shape") and len(value.shape) > 1 and value.shape[1] == 1:
                inputs[name] = self.make_storage_data(
                    pad_field_in_j(
                        value, self.grid.njd, backend=self.stencil_factory.backend
                    )
                )
        quantity_tracers = self.grid.quantity_factory.empty(
            [I_DIM, J_DIM, K_DIM, FVTracersAxisName], "n/a"
        )
        for i_tracer, value in tuple(inputs["tracers"].items()):
            if hasattr(value, "shape") and len(value.shape) > 1 and value.shape[1] == 1:
                quantity_tracers[:, :, :, i_tracer] = self.make_storage_data(
                    pad_field_in_j(
                        value, self.grid.njd, backend=self.stencil_factory.backend
                    )
                )
        inputs["tracers"] = quantity_tracers

        run_fillz = fillz.FillNegativeTracerValues(
            self.stencil_factory,
            inputs.pop("nq"),
        )
        run_fillz(**inputs)

        ds = self.grid.default_domain_dict()
        ds.update(self.out_vars["q2tracers"])

        if self.stencil_factory.backend.is_fortran_aligned():
            offset = None
        else:
            offset = -1

        out = {
            "q2tracers": quantity_tracers[
                ds["istart"] : ds["iend"] + 1, ds["jstart"], : ds["kend"] + 1, :offset
            ]
        }
        return out
