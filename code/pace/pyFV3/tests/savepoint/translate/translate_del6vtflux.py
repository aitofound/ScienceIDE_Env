from f90nml import Namelist

import pyfv3.stencils.delnflux as delnflux
from ndsl import StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.typing import Float
from pyfv3.testing import TranslateDycoreFortranData2Py


class TranslateDel6VtFlux(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        fxstat = grid.x3d_domain_dict()
        fxstat.update({"serialname": "fx2"})
        fystat = grid.y3d_domain_dict()
        fystat.update({"serialname": "fy2"})
        self.in_vars["data_vars"] = {
            "q": {"serialname": "wq"},
            "d2": {"serialname": "wd2"},
            "fx2": grid.x3d_domain_dict(),
            "fy2": grid.y3d_domain_dict(),
            "damp_c": {"serialname": "damp4"},
            "nord_w": {},
        }
        self.in_vars["parameters"] = []
        self.out_vars = {
            "fx2": grid.x3d_domain_dict(),
            "fy2": grid.y3d_domain_dict(),
            "d2": {"serialname": "wd2"},
            "q": {"serialname": "wq"},
        }
        self.stencil_factory = stencil_factory

    # use_sg -- 'dx', 'dy', 'rdxc', 'rdyc', 'sin_sg needed
    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        nord_col = self.grid.quantity_factory.zeros(dims=[K_DIM], units="unknown")
        nord_col[:] = nord_col.np.asarray(inputs.pop("nord_w"))
        self.compute_func = delnflux.DelnFluxNoSG(
            self.stencil_factory,
            self.grid.damping_coefficients,
            self.grid.rarea,
            nord_col,
        )

        # Convert relevant inputs to quantities:
        d2 = self.grid.quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_INTERFACE_DIM], units="unknown", dtype=Float
        )
        d2[:] = d2.np.asarray(inputs.pop("d2"))
        inputs["d2"] = d2

        self.compute_func(**inputs)
        return self.slice_output(inputs)
