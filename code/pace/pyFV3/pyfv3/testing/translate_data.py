from f90nml import Namelist

from ndsl import StencilFactory
from ndsl.stencils.testing import Grid, TranslateFortranData2Py
from pyfv3._config import DynamicalCoreConfig


class TranslateDycoreFortranData2Py(TranslateFortranData2Py):
    def __init__(
        self,
        grid: Grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ) -> None:
        super().__init__(grid, stencil_factory)
        self.config = DynamicalCoreConfig.from_f90nml(namelist)
