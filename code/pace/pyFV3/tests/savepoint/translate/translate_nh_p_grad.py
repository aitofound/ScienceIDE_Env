from f90nml import Namelist

import pyfv3.stencils.nh_p_grad as NH_P_Grad
from ndsl import StencilFactory
from pyfv3.testing import TranslateDycoreFortranData2Py


class TranslateNH_P_Grad(TranslateDycoreFortranData2Py):
    max_error = 5e-10

    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "u": {},
            "v": {},
            "pp": {},
            "gz": {},
            "pk3": {},
            "delp": {},
        }
        self.in_vars["parameters"] = ["dt", "ptop", "akap"]
        self.out_vars = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "pp": {"kend": grid.npz + 1},
            "gz": {"kend": grid.npz + 1},
            "pk3": {"kend": grid.npz + 1},
            "delp": {},
        }
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        self.compute_func = NH_P_Grad.NonHydrostaticPressureGradient(
            self.stencil_factory,
            self.grid.quantity_factory,
            grid_data=self.grid.grid_data,
            grid_type=self.config.grid_type,
            use_logp=self.config.use_logp,
        )
        self.make_storage_data_input_vars(inputs)
        self.compute_func(**inputs)
        return self.slice_output(inputs)
