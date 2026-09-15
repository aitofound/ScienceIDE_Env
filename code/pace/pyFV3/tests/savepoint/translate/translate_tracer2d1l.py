import pytest
from f90nml import Namelist

from ndsl import StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.stencils.testing import ParallelTranslate
from pyfv3 import DynamicalCoreConfig
from pyfv3.stencils import FiniteVolumeTransport, TracerAdvection
from pyfv3.tracers import FVTracersAxisName, default_ai2_tracers
from pyfv3.utils.functional_validation import get_subset_func


class TranslateTracer2D1L(ParallelTranslate):
    inputs = {
        "tracers": {
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/m^2",
        }
    }

    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self._base.in_vars["data_vars"] = {
            "tracers": {},
            "dp1": {},
            "mfxd": grid.x3d_compute_dict(),
            "mfyd": grid.y3d_compute_dict(),
            "cxd": grid.x3d_compute_domain_y_dict(),
            "cyd": grid.y3d_compute_domain_x_dict(),
        }
        self._base.in_vars["parameters"] = ["nq"]
        self._base.out_vars = self._base.in_vars["data_vars"]
        self.stencil_factory = stencil_factory
        self._subset = get_subset_func(
            self.grid.grid_indexing,
            dims=[I_DIM, J_DIM, K_DIM],
            n_halo=((0, 0), (0, 0)),
        )
        self.config = DynamicalCoreConfig.from_f90nml(namelist)
        self.quantity_factory = grid.quantity_factory

    def collect_input_data(self, serializer, savepoint):
        input_data = self._base.collect_input_data(serializer, savepoint)
        return input_data

    def compute_parallel(self, inputs, communicator):
        default_ai2_tracers(self.quantity_factory)
        self._base.make_storage_data_input_vars(inputs)

        quantity_tracers = self.grid.quantity_factory.from_array(
            inputs["tracers"], [I_DIM, J_DIM, K_DIM, FVTracersAxisName], "n/a"
        )
        inputs["tracers"] = quantity_tracers
        nq = int(inputs.pop("nq"))

        transport = FiniteVolumeTransport(
            stencil_factory=self.stencil_factory,
            quantity_factory=self.grid.quantity_factory,
            grid_data=self.grid.grid_data,
            damping_coefficients=self.grid.damping_coefficients,
            grid_type=self.grid.grid_type,
            hord=self.config.hord_tr,
        )

        self.tracer_advection = TracerAdvection(
            self.stencil_factory,
            self.grid.quantity_factory,
            transport,
            self.grid.grid_data,
            communicator,
            inputs["tracers"],
            nq,
        )
        inputs["x_mass_flux"] = inputs.pop("mfxd")
        inputs["y_mass_flux"] = inputs.pop("mfyd")
        inputs["x_courant"] = inputs.pop("cxd")
        inputs["y_courant"] = inputs.pop("cyd")
        self.tracer_advection(**inputs)
        inputs["mfxd"] = inputs.pop("x_mass_flux")
        inputs["mfyd"] = inputs.pop("y_mass_flux")
        inputs["cxd"] = inputs.pop("x_courant")
        inputs["cyd"] = inputs.pop("y_courant")

        inputs["tracers"] = quantity_tracers.field[:]

        outputs = self._base.slice_output(inputs)
        return outputs

    def compute_sequential(self, inputs_list, communicator_list):
        pytest.skip(
            f"{self.__class__} only has a mpirun implementation, not running in mock-parallel"
        )

    def subset_output(self, varname: str, output):
        """
        Given an output array, return the slice of the array which we'd
        like to validate against reference data
        """
        if varname in ["tracers"]:
            return self._subset(output)
        else:
            return output
