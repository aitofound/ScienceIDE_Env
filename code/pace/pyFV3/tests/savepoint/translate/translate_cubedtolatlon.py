from f90nml import Namelist

from ndsl import Quantity, StencilFactory
from ndsl.constants import I_DIM, I_INTERFACE_DIM, J_DIM, J_INTERFACE_DIM, K_DIM
from ndsl.stencils.c2l_ord import CubedToLatLon
from ndsl.stencils.testing import ParallelTranslate2Py
from pyfv3 import DynamicalCoreConfig


class TranslateCubedToLatLon(ParallelTranslate2Py):
    inputs = {
        "u": {
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "m/s",
        },
        "v": {
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
    }

    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self._base.in_vars["data_vars"] = {"u": {}, "v": {}, "ua": {}, "va": {}}
        self._base.out_vars = {
            "ua": {},
            "va": {},
            "u": self.grid.y3d_domain_dict(),
            "v": self.grid.x3d_domain_dict(),
        }
        self.stencil_factory = stencil_factory
        self.config = DynamicalCoreConfig.from_f90nml(namelist)
        self.grid_type = self.config.grid_type

    def compute_parallel(self, inputs, communicator):
        self._base.make_storage_data_input_vars(inputs)
        u_quantity = _quantity_wrap(
            inputs["u"],
            self.inputs["u"]["dims"],
            self.grid.grid_indexing,
            self.stencil_factory.backend,
        )
        v_quantity = _quantity_wrap(
            inputs["v"],
            self.inputs["v"]["dims"],
            self.grid.grid_indexing,
            self.stencil_factory.backend,
        )
        state_dict = {"u": u_quantity, "v": v_quantity}

        self._cubed_to_latlon = CubedToLatLon(
            state=state_dict,
            stencil_factory=self.stencil_factory,
            quantity_factory=self.grid.quantity_factory,
            grid_data=self.grid.grid_data,
            order=self.config.c2l_ord,
            comm=communicator,
            grid_type=self.grid_type,
        )
        self._cubed_to_latlon(**inputs)
        return self._base.slice_output(inputs)


def _quantity_wrap(storage, dims, grid_indexing, backend):
    origin, extent = grid_indexing.get_origin_domain(dims)
    return Quantity(
        storage,
        dims=dims,
        units="unknown",
        origin=origin,
        extent=extent,
        backend=backend,
    )
