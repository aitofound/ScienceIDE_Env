from f90nml import Namelist

from ndsl import StencilFactory
from pyfv3.stencils import RayleighDamping
from pyfv3.testing import TranslateDycoreFortranData2Py


class TranslateRay_Fast(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.compute_func = RayleighDamping(
            stencil_factory,
            self.config.rf_cutoff,
            self.config.tau,
            self.config.hydrostatic,
        )
        self.in_vars["data_vars"] = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "w": {},
            "dp": {},
            "pfull": {},
        }
        self.in_vars["parameters"] = ["dt", "ptop"]
        self.out_vars = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "w": {},
        }
        self.stencil_factory = stencil_factory
